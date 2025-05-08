#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <iostream>
#include <time.h>
#include <numeric>
#include <cmath>
#include "bmp_utility.h"

// OpenCL header file and Intel FPGA SDK header file
// Uncomment when running FPGA Kernel
#include "CL/opencl.h"
#include "AOCLUtils/aocl_utils.h" 

// Toggle this to run on CPU (DE1-SoC or your machine) and FPGA
// 0 to run on CPU
// 1 to run on FPGA
#define FPGA 1

#if FPGA == 1// If FPGA == 1, OpenCL related code will execute

    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_kernel kernel;
    cl_program program;

    std::string deviceInfo;
    std::string aocxFilename;

    // namespace for Intel FPGS SDK
    using namespace aocl_utils;

    unsigned num_devices = 0;

#endif

// Image size in 1D array = 28 x 28
const int inputSize = 784; // 28x28 input image

// Number of neurons in the hidden layer
// This code assumes a single hidden layer
// If you use more layers modify accordingly
const int numNeurons = 10;

// Tile size to perform matrix multiplication
// Experiment with this size and report those values 
const int inputTileSize = 28;

// Variables to hold input data
std::vector<float> image_data; // image data

std::vector<float> hidden_layer1_weights;
std::vector<float> hidden_layer1_biases;
std::vector<float> hidden_layer1_out;

std::vector<float> output_layer_weights;
std::vector<float> output_layer_biases;
std::vector<float> output_layer_out;

#if FPGA == 1

// Neural network buffers
// We transfer data to corresponding buffers before launching the Kernel

cl_mem inputTileBuffer; // Buffer for the input image
cl_mem weightsTileBuffer; // Buffers for layer1 weigths
cl_mem outputBuffer; // Buffers for the output of each layer

#endif

std::string layer1_weightsPath = "fc1_weight.bin";
std::string layer1_biasesPath = "fc1_bias.bin";

std::string output_weightsPath = "fc2_weight.bin";
std::string output_biasesPath = "fc2_bias.bin";

#if FPGA == 1 // functions that setup opencl environment, problem, run and cleanup 
            // needed only when running the kernel   
bool init_opencl();
void run();
void cleanup();
#endif

void normalizeImage(unsigned char* imageData, size_t imageSize, std::vector<float>& normalizedImage);
void setupDataAndModels();
void run_cpu();
void processTiles_weightStatinary_CPU(int numNeurons,
    int inputSize, // Size of the input array
    int inputTileSize,  // Tile size of the Input vector          
    std::vector<float>& weights, // Weights array
    std::vector<float>& biases,  // biases array
    std::vector<float>& inputs,  // inputs array 
    std::vector<float>& outputs  // outputs array
    );
void cleanup_cpu();

void relu(std::vector<float>& v);

int getMaxIn(std::vector<float>& v);

bool loadModelParameters(const std::string& weightsPath, const std::string& biasesPath, 
                         std::vector<float>& weightsBuffer, std::vector<float>& biases);

std::vector<float> loadFloatsFromFile(const std::string& filename);
void log_softmax(std::vector<float>& v);

std::vector<float> loadWeights(int weightsStartIndex,int numNeurons,int inputTileSize,int inputSize,
    std::vector<float>& weights,std::vector<float>& temp_wts); // Added prototype

// Code execution starts here
int main(int argc, char **argv) {

  #if FPGA == 1
  // Options from base OpenCL code, ignore for this lab  
  Options options(argc, argv);

  // Optional argument to specify the problem size.
  // Relative path to aocx filename.
    if(options.has("aocx")) {
        aocxFilename = options.get<std::string>("aocx");  
    } else {
        aocxFilename = "matrix_mul";
    }

  // Initialize OpenCL.
  if(!init_opencl()) {
    return -1;
  }
  #endif

  setupDataAndModels();

  // Run the kernel.
  #if FPGA == 1
  //Initialize the problem data.
  run();
  #else
  run_cpu();
  #endif

  // Free the resources allocated
  #if FPGA == 1
    cleanup();
  #else
    cleanup_cpu();
  #endif

  return 0;
}

void setupDataAndModels(){
    const char* filename = "first_image_mnist.bmp";
    int width = 0;
    int height = 0;

    unsigned char* pre_image_data = loadBMPGrayscale(filename, &width, &height);
    if (!pre_image_data) {
        std::cerr << "Failed to load image: " << filename << std::endl;
        #if FPGA == 1
            cleanup();
        #endif
        return;
    }
    flipImageVertically(pre_image_data, width, height);

    normalizeImage(pre_image_data, width*height, image_data);
    
    printf("done loading image:%d\n",width*height);

    if (!loadModelParameters(layer1_weightsPath,layer1_biasesPath,hidden_layer1_weights,hidden_layer1_biases)) {
        std::cerr << "Failed to load model layer 1 parameters." << std::endl;
        #if FPGA == 1
            cleanup();
        #endif
        return;
    }

    if (!loadModelParameters(output_weightsPath,output_biasesPath,output_layer_weights,output_layer_biases)) {
        std::cerr << "Failed to load model output layer parameters." << std::endl;
        #if FPGA == 1
            cleanup();
        #endif
        return;
    }

    printf("loaded model parameters\n");
}

void log_softmax(std::vector<float>& v) {
    float maxElement = *std::max_element(v.begin(), v.end());
    std::vector<float> exp_values(v.size());
    float sum = 0.0f;

    // Calculate exponentials and sum them
    for(size_t i = 0; i < v.size(); ++i) {
        exp_values[i] = std::exp(v[i] - maxElement);
        sum += exp_values[i];
    }

    // Normalize
    for(size_t i = 0; i < v.size(); ++i) {
        v[i] = std::log(exp_values[i] / sum);
    }
}

void normalizeImage(unsigned char* imageData, size_t imageSize, std::vector<float>& normalizedImage) {
    normalizedImage.resize(imageSize);

    float mean=0.1307f;
    float std=0.3081f;
    for (size_t i = 0; i < imageSize; ++i) {
        normalizedImage[i] = (imageData[i] / 255.0f - mean) / std;
    }
}

std::vector<float> loadFloatsFromFile(const std::string& filename) {
    // Open the file in binary mode
    std::ifstream file(filename.c_str(), std::ios::binary | std::ios::ate);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return {}; // Return an empty vector in case of failure
    }

    // Determine the file size
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Calculate the number of float elements
    std::streamsize numElements = size / sizeof(float);

    std::vector<float> buffer(numElements);

    // Read the file content into the buffer
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Failed to read floats from file: " << filename << std::endl;
        return {}; // Return an empty vector in case of failure
    }

    file.close(); // Close the file
    return buffer; // Return the loaded floats
}

bool loadModelParameters(const std::string& weightsPath, const std::string& biasesPath, 
                         std::vector<float>& weightsBuffer, std::vector<float>& biases) {
    weightsBuffer = loadFloatsFromFile(weightsPath);
    biases = loadFloatsFromFile(biasesPath);
    
    if (weightsBuffer.empty() || biases.empty()) {
        return false; // Return false if either weights or biases failed to load
    }
    return true; // Successfully loaded and transferred weights and biases
}

#if FPGA == 1
bool init_opencl() {
  cl_int status;

    // Start everything at NULL to help identify errors.
    kernel = NULL;
    queue = NULL;
    context = NULL;
    program = NULL;
    
    // Locate files via relative paths.
    if(!setCwdToExeDir()) {
        cleanup();
        return false;
    }

    // Get the OpenCL platform.
    platform = findPlatform("Intel(R) FPGA");
    if (platform == NULL) {
        cleanup();
        return false;
    }

    // Get the first device.
    status = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1, &device, NULL);
    if (status != CL_SUCCESS) {
        checkError(status, "Error: could not query devices");
        cleanup();
        return false;
    }

    char info[256];
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(info), info, NULL);
    deviceInfo = info;

    // Create the context.
    context = clCreateContext(0, 1, &device, &oclContextCallback, NULL, &status);
    if (status != CL_SUCCESS) {
        checkError(status, "Error: could not create OpenCL context");
        cleanup();
        return false;
    }

    // Create the command queues for the kernels.
    queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &status);
    if (status != CL_SUCCESS) {
        checkError(status, "Failed to create command queue");
        cleanup();
        return false;
    }

    // Create the program.
    std::string binary_file = getBoardBinaryFile(aocxFilename.c_str(), device);
    std::cout << "Using AOCX: " << binary_file << "\n";
    program = createProgramFromBinary(context, binary_file.c_str(), &device, 1);
    if (!program) {
        std::cerr << "Failed to create program from binary" << std::endl;
        cleanup();
        return false;
    }

    // Build the program that was just created.
    status = clBuildProgram(program, 1, &device, "", NULL, NULL);
    if (status != CL_SUCCESS) {
        checkError(status, "Error: could not build program");
        cleanup();
        return false;
    }
    
    // Create the kernels
    kernel = clCreateKernel(program, "hidden", &status);
    if (status != CL_SUCCESS) {
        checkError(status, "Failed to create cnn kernel");
        cleanup();
        return false;
    }

    printf("done creating buffer\n");
    return true;
}

void processTiles_weightStatinary(int numNeurons,
    int inputSize, // Size of the input array
    int inputTileSize,  // Tile size of the Input vector          
    std::vector<float>& weights, // Weights array
    std::vector<float>& biases,  // biases array
    std::vector<float>& inputs,  // inputs array 
    std::vector<float>& outputs  // outputs array
    ) {

    printf("in the weight stationary function\n");

    if (weights.empty() || biases.empty() || inputs.empty()) {
        std::cerr << "Error: Input, weights, or biases are empty in processTiles_weightStatinary" << std::endl;
        return;
    }

    cl_int err;

    int outputNeuronsTileSize = 10;
    int currentTileSize = inputTileSize;

    weightsTileBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY, currentTileSize * outputNeuronsTileSize * sizeof(float), NULL, &err);
    if (err != CL_SUCCESS) {
        checkError(err, "Failed to create weightsTileBuffer");
        return;
    }
    
    inputTileBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY, currentTileSize * sizeof(float), NULL, &err);
    if (err != CL_SUCCESS) {
        checkError(err, "Failed to create inputTileBuffer");
        clReleaseMemObject(weightsTileBuffer);
        return;
    }

    outputBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, outputNeuronsTileSize * sizeof(float), NULL, &err);
    if (err != CL_SUCCESS) {
        checkError(err, "Failed to create outputBuffer");
        clReleaseMemObject(weightsTileBuffer);
        clReleaseMemObject(inputTileBuffer);
        return;
    }

    printf("done creating buffer\n");

    float pattern = 0.0f; // The pattern to fill, here it's 0.0 for float
    size_t pattern_size = sizeof(float); // Size of the pattern, here it's the size of a float
    size_t offset = 0; // Start offset within the buffer
    size_t size = numNeurons * sizeof(float); // Size of the buffer to fill

    // Set output buffer to zeros, use this buffer to accumulate results for dot product
    err = clEnqueueFillBuffer(queue, outputBuffer, &pattern, pattern_size, offset, size, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        checkError(err, "Failed to fill outputBuffer");
        clReleaseMemObject(weightsTileBuffer);
        clReleaseMemObject(inputTileBuffer);
        clReleaseMemObject(outputBuffer);
        return;
    }

    clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&inputTileBuffer);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&weightsTileBuffer);
    clSetKernelArg(kernel, 2, sizeof(int), &inputTileSize);
    clSetKernelArg(kernel, 3, sizeof(int), &outputNeuronsTileSize);
    clSetKernelArg(kernel, 4, sizeof(cl_mem), (void*)&outputBuffer);


    int numTiles = inputSize / inputTileSize;
    int weightsPerTile = numNeurons * inputTileSize;

    for (int tileIndex = 0; tileIndex < numTiles; ++tileIndex) {
        int weightsStartIndex = tileIndex * inputTileSize;
        std::vector<float> temp_wts(weightsPerTile);
        loadWeights(weightsStartIndex, numNeurons, inputTileSize, inputSize, weights, temp_wts);

        std::vector<float> inputSlice(inputs.begin() + weightsStartIndex, inputs.begin() + weightsStartIndex + inputTileSize);
        err = clEnqueueWriteBuffer(queue, weightsTileBuffer, CL_TRUE, 0, weightsPerTile * sizeof(float), temp_wts.data(), 0, NULL, NULL);
        if (err != CL_SUCCESS) {
            checkError(err, "Failed to write weightsTileBuffer");
            break;
        }

        err = clEnqueueWriteBuffer(queue, inputTileBuffer, CL_TRUE, 0, inputTileSize * sizeof(float), inputSlice.data(), 0, NULL, NULL);
        if (err != CL_SUCCESS) {
            checkError(err, "Failed to write inputTileBuffer");
            break;
        }

        size_t global_work_size[] = {static_cast<size_t>(10)};
        size_t local_work_size[] = {static_cast<size_t>(1)};
        err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, global_work_size, local_work_size, 0, NULL, NULL);
        if (err != CL_SUCCESS) {
            checkError(err, "Failed to enqueue kernel");
            break;
        }

        clFinish(queue);    
    }

    outputs.resize(numNeurons);
    err = clEnqueueReadBuffer(queue, outputBuffer, CL_TRUE, 0, numNeurons * sizeof(float), outputs.data(), 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        checkError(err, "Failed to read outputBuffer");
    }

    // Add biases
    for (int i = 0; i < numNeurons; ++i) {
        outputs[i] += biases[i];
    }

    clReleaseMemObject(inputTileBuffer);
    clReleaseMemObject(weightsTileBuffer);
    clReleaseMemObject(outputBuffer);
}

void run() {
    cl_int status;

    // hidden 
    const unsigned hiddenLayerIndex = 0;
    const unsigned outputLayerIndex = 1;

    size_t hidden_weights_offset = 0; // Starting at the beginning for the hidden layer
    size_t output_weights_offset = 784 * 10; // Adjust based on your buffer organization
    size_t hidden_biases_offset = 0;
    size_t output_biases_offset = 10; // Assuming biases are also concatenated

    size_t hidden_work_size = {10};
    size_t output_work_size = {10};

    printf("started running on fpga\n");

    hidden_layer1_out.resize(numNeurons);
    processTiles_weightStatinary(hidden_work_size,
        inputSize, // Size of the input array
        inputTileSize,  // Tile size of the Input vector          
        hidden_layer1_weights, // Weights array
        hidden_layer1_biases,  // biases array
        image_data,  // inputs array 
        hidden_layer1_out  // outputs array
    );

    std::cout << "Output of fc1 (before ReLU): ";
    for (int i = 0; i < 10; ++i) {
        std::cout << hidden_layer1_out[i] << " ";
    }
    std::cout << std::endl;

    relu(hidden_layer1_out);

    std::cout << "Output of fc1 (after ReLU): ";
    for (int i = 0; i < 10; ++i) {
        std::cout << hidden_layer1_out[i] << " ";
    }
    std::cout << std::endl;

    output_layer_out.resize(numNeurons);
    processTiles_weightStatinary(output_work_size,
        output_work_size, // Size of the input array
        10,  // Tile size of the Input vector          
        output_layer_weights, // Weights array
        output_layer_biases,  // biases array
        hidden_layer1_out,  // inputs array 
        output_layer_out  // outputs array
    );

    std::cout << "Output of fc2 (before LogSoftmax): ";
    for (int i = 0; i < 10; ++i) {
        std::cout << output_layer_out[i] << " ";
    }
    std::cout << std::endl;

    log_softmax(output_layer_out);

    std::cout << "Output of fc2 (after LogSoftmax): ";
    for (int i = 0; i < 10; ++i) {
        std::cout << output_layer_out[i] << " ";
    }
    std::cout << std::endl;

    int Label = getMaxIn(output_layer_out);
    printf("Predicted label:%d\n", Label);
}
#endif // Closing the outer #if FPGA == 1 block

void matrixMulCPU(
    std::vector<float>& input_tile,  // Tile of the Input vector
    std::vector<float>& weights_tile, // Tile of the Weights matrix
    int input_tile_size,                  // Size of the input tile
    int output_neurons_tile_size,         // Size of the output tile (number of neurons in this tile)
    std::vector<float>& output_tile                // Output vector tile
){

    int neuron_id = 0;
    for(neuron_id;neuron_id<output_neurons_tile_size;neuron_id++){
    // Ensure we don't process more neurons than we have in this tile
    if (neuron_id < output_neurons_tile_size) {
        float temp_sum = 0.0;
        
        // Compute the dot product of the input tile and the corresponding weights
        for (int i = 0; i < input_tile_size; ++i) {
          int weight_index = neuron_id * input_tile_size + i;
          temp_sum += (float)input_tile[i] * (float)weights_tile[weight_index];
     }

        // Write the computed sum for this neuron to the output tile
        output_tile[neuron_id] += temp_sum;
    }
    }
}

std::vector<float> loadWeights(int weightsStartIndex,int numNeurons,int inputTileSize,int inputSize,
    std::vector<float>& weights,std::vector<float>& temp_wts){
    
    int index = 0;
    for(int i=0;i<numNeurons;i++){
        for(int j=0;j<inputTileSize;j++){
            temp_wts[index] = weights[(i)*inputSize + j+weightsStartIndex];
            index++;
        }
    }
    return temp_wts;    
}

#if FPGA == 0
void processTiles_weightStatinary_CPU(
    int numNeurons,
    int inputSize, // Size of the input array
    int inputTileSize,  // Tile size of the Input vector          
    std::vector<float>& weights, // Weights array
    std::vector<float>& biases,  // biases array
    std::vector<float>& inputs,  // inputs array 
    std::vector<float>& outputs  // outputs array
    ) {

    printf("in the weight stationary function of CPU\n");    

    int numTiles = inputSize / inputTileSize; // Ensure this division is an integer
    int totalWeights = inputSize * numNeurons;
    int weightsPerTile = numNeurons*inputTileSize; // Assuming an even distribution of neurons per tile

    for (int tileIndex = 0; tileIndex < numTiles; ++tileIndex) {
        
        int weightsStartIndex = tileIndex * inputTileSize; 
        std::vector<float> temp_wts;
        temp_wts.resize(numNeurons * inputTileSize);
        loadWeights(weightsStartIndex,numNeurons,inputTileSize,inputSize,weights,temp_wts);

        std::vector<float> inputSlice(std::next(inputs.begin(), weightsStartIndex), std::next(inputs.begin(), weightsStartIndex+inputTileSize));

        matrixMulCPU(
            inputSlice,  // Tile of the Input vector
            temp_wts, // Tile of the Weights matrix
            inputTileSize,                  // Size of the input tile
            numNeurons,         // Size of the output tile (number of neurons in this tile)
            outputs
        );
    }

    for(int i=0;i<numNeurons;i++){
        outputs[i] += biases[i];
    } 
}

void run_cpu() {
    

    // hidden 
    const unsigned hiddenLayerIndex = 0;
    const unsigned outputLayerIndex = 1;

    size_t hidden_weights_offset = 0; // Starting at the beginning for the hidden layer
    size_t output_weights_offset = 784 * 10; // Adjust based on your buffer organization
    size_t hidden_biases_offset = 0;
    size_t output_biases_offset = 10; // Assuming biases are also concatenated

    size_t hidden_work_size = {10};
    size_t output_work_size = {10};

    printf("started running on CPU\n");

    hidden_layer1_out.resize(numNeurons * inputTileSize);
    processTiles_weightStatinary_CPU(hidden_work_size,
    inputSize, // Size of the input array
    inputTileSize,  // Tile size of the Input vector          
    hidden_layer1_weights, // Weights array
    hidden_layer1_biases,  // biases array
    image_data,  // inputs array 
    hidden_layer1_out  // outputs array
    );

    std::cout << "Output of fc1 (before ReLU): ";

    for(int i=0;i<10;i++){
        std::cout << hidden_layer1_out[i] << " ";
    }
    std::cout << std::endl;

    relu(hidden_layer1_out);

    std::cout << "Output of fc1 (after ReLU): ";

    for(int i=0;i<10;i++){
        std::cout << hidden_layer1_out[i] << " ";
    }
    std::cout << std::endl;

    output_layer_out.resize(numNeurons * inputTileSize);

    processTiles_weightStatinary_CPU(output_work_size,
    output_work_size, // Size of the input array
    10,  // Tile size of the Input vector          
    output_layer_weights, // Weights array
    output_layer_biases,  // biases array
    hidden_layer1_out,  // inputs array 
    output_layer_out  // outputs array
    );

    std::cout << "Output of fc2 (before LogSoftmax): "; 
    for(int i=0;i<10;i++){
        std::cout << output_layer_out[i] << " ";
    }
    std::cout << std::endl;

    log_softmax(output_layer_out);

    std::cout << "Output of fc2 (after LogSoftmax): "; 
    for(int i=0;i<10;i++){
        std::cout << output_layer_out[i] << " ";
    }
    std::cout << std::endl;

    int Label = getMaxIn(output_layer_out);
    printf("Predicted label:%d\n",Label);
}
#endif

int getMaxIn(std::vector<float>& v){
    int maxIndex = std::distance(v.begin(), std::max_element(v.begin(), v.end()));
    return maxIndex;
}

void relu(std::vector<float>& v) {
    for (size_t i = 0; i < v.size(); ++i) {
        v[i] = std::max(0.0f, v[i]);
    }
}

void cleanup_cpu() {
}

void cleanup() {
    #if FPGA == 1
    cl_int status;

    // Release kernels
    if(kernel) {
        status = clReleaseKernel(kernel);
        checkError(status, "Failed to release kernel");
    }

    //Release programs
    //Assuming you have stored your program objects similar to kernels
    if(program) {
        status = clReleaseProgram(program);
        checkError(status, "Failed to release program");
    }

    // Finally, release the context
    if(context) {
        status = clReleaseContext(context);
        checkError(status, "Failed to release context");
    }
    #endif

    // If you have other resources allocated, make sure to release them properly
}