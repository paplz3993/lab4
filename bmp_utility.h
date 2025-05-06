#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring> 


#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t file_type; // "BM"
    uint32_t file_size; // Size of the file (in bytes)
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset_data; // Start position of pixel data (bytes from the beginning of the file)

    BMPFileHeader() 
        : file_type(0x4D42), // "BM"
          file_size(0),
          reserved1(0),
          reserved2(0),
          offset_data(54) {}
};

struct BMPInfoHeader {
    uint32_t size;        // Size of this header (in bytes)
    int32_t width;        // width of bitmap in pixels
    int32_t height;       // height of bitmap in pixels
    uint16_t planes;      // No. of planes for the target device, this is always 1
    uint16_t bit_count;   // No. of bits per pixel
    uint32_t compression; // 0 or 3 - uncompressed
    uint32_t size_image;  // Size of the image's pixel data in bytes
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t colors_used;      // No. color indexes in the color table
    uint32_t colors_important; // No. of colors used for displaying the bitmap

    BMPInfoHeader() 
        : size(40),
          width(0),
          height(0),
          planes(1),
          bit_count(24),
          compression(0),
          size_image(0),
          x_pixels_per_meter(0),
          y_pixels_per_meter(0),
          colors_used(0),
          colors_important(0) {}
};
#pragma pack(pop)


void writeHeaders(std::ofstream &of, const BMPFileHeader &file_header, const BMPInfoHeader &info_header) {
    of.write((const char*)&file_header, sizeof(file_header));
    of.write((const char*)&info_header, sizeof(info_header));
}

void saveImage(const char* filename, unsigned char data[320][240], int width, int height) {
    BMPFileHeader file_header;
    BMPInfoHeader info_header;

    info_header.width = width;
    info_header.height = height;
    file_header.file_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + width * height * 3;

    //std::ofstream of{filename, std::ios_base::binary};
    std::ofstream of(filename, std::ios_base::binary);

    if (!of) {
        std::cerr << "Could not open the output image file." << std::endl;
        return;
    }

    writeHeaders(of, file_header, info_header);

    // The BMP format stores data in the bottom-up order
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            unsigned char pixel[3];
            //pixel[0] = data[y * width + x]; // Blue
            //pixel[1] = data[y * width + x]; // Green
            //pixel[2] = data[y * width + x]; // Red
            pixel[0] = data[y][x]; // Blue
            pixel[1] = data[y][x]; // Green
            pixel[2] = data[y][x]; // Red
            of.write((char*)pixel, 3);
        }
    }

    of.close();
}


void scaleImagePreservingAspectRatio(
    const unsigned char* input, unsigned char* output,
    int originalWidth, int originalHeight,
    int targetWidth, int targetHeight) {

    // Calculate the scaling factor and the size of the scaled image
    float scaleX = static_cast<float>(targetWidth) / originalWidth;
    float scaleY = static_cast<float>(targetHeight) / originalHeight;
    float scale = std::min(scaleX, scaleY);

    int scaledWidth = originalWidth * scale;
    int scaledHeight = originalHeight * scale;

    // Calculate padding to center the image
    int padX = (targetWidth - scaledWidth) / 2;
    int padY = (targetHeight - scaledHeight) / 2;

    // Initialize the output image with black (0)
    std::memset(output, 0, targetWidth * targetHeight);

    // Scale and center the image
    for (int y = 0; y < scaledHeight; ++y) {
        for (int x = 0; x < scaledWidth; ++x) {
            int origX = x / scale;
            int origY = y / scale;
            output[(y + padY) * targetWidth + (x + padX)] = input[origY * originalWidth + origX ];
        }
    }
}



void saveImageGrayscale(const char* filename, const unsigned char* temp, int width, int height) {
    BMPFileHeader file_header;
    BMPInfoHeader info_header;

    // Calculate padding to align rows to a 4-byte boundary
    int paddingAmount = (4 - (width * 3) % 4) % 4;
    file_header.file_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + (width * 3 + paddingAmount) * height;

    std::ofstream of(filename, std::ios_base::binary);

    if (!of) {
        std::cerr << "Could not open the output image file." << std::endl;
        return;
    }

    // Initialize BMP headers
    info_header.width = width;
    info_header.height = height;

    // Write headers
    of.write((const char*)&file_header, sizeof(file_header));
    of.write((const char*)&info_header, sizeof(info_header));

    // Write pixel data
    for (int y = height - 1; y >= 0; y--) { // BMP images are stored upside-down
        for (int x = 0; x < width; x++) {
            unsigned char grayValue = temp[x+y*width];
            unsigned char pixel[3] = {grayValue, grayValue, grayValue}; // Grayscale is represented by repeating the same value in R, G, and B
            of.write((char*)pixel, 3);
        }
        // Write padding bytes at the end of each row, if necessary
        unsigned char padding[3] = {0, 0, 0};
        of.write((char*)padding, paddingAmount);
    }

    of.close();
    std::cout << "Image saved to " << filename << std::endl;
}


void saveImageShort(const char* filename, const unsigned short* temp, int width, int height) {
    BMPFileHeader file_header;
    BMPInfoHeader info_header;

    // Calculate padding to align rows to a 4-byte boundary
    int paddingAmount = (4 - (width * 3) % 4) % 4;
    file_header.file_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + (width * 3 + paddingAmount) * height;

    std::ofstream of(filename, std::ios_base::binary);

    if (!of) {
        std::cerr << "Could not open the output image file." << std::endl;
        return;
    }

    // Initialize BMP headers
    info_header.width = width;
    info_header.height = height;

    // Write headers
    of.write((const char*)&file_header, sizeof(file_header));
    of.write((const char*)&info_header, sizeof(info_header));

    // Write pixel data
    for (int y = height - 1; y >= 0; y--) { // BMP images are stored upside-down
        for (int x = 0; x < width; x++) {
            short value = temp[x+y*width];
            // Extract the color components
            unsigned char red = (value >> 11) & 0x1F;
            unsigned char green = (value >> 5) & 0x3F;
            unsigned char blue = value & 0x1F;
            // Scale the color components
            red = (red << 3) | (red >> 2);
            green = (green << 2) | (green >> 4);
            blue = (blue << 3) | (blue >> 2);
            // Write the color components
            unsigned char pixel[3] = {blue, green, red}; // BMP format uses BGR
            of.write((char*)pixel, 3);
        }
        // Write padding bytes at the end of each row
        unsigned char padding[3] = {0};
        of.write((char*)padding, paddingAmount);
    }

    of.close();
    std::cout << "Image saved to " << filename << std::endl;
}



struct BMPHeader {
    uint32_t size;
    uint16_t reserved1, reserved2;
    uint32_t offset;
};



void flipImageVertically(unsigned char* data, int width, int height) {
    unsigned char* tempRow = new unsigned char[width];
    for (int y = 0; y < height / 2; y++) {
        memcpy(tempRow, data + y * width, width); // Copy top row to tempRow
        memcpy(data + y * width, data + (height - y - 1) * width, width); // Copy bottom row to top
        memcpy(data + (height - y - 1) * width, tempRow, width); // Copy tempRow to bottom
    }
    delete[] tempRow;
}




unsigned char* loadBMPGrayscale(const char* filename, int* width, int* height) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Unable to open file " << filename << std::endl;
        return NULL;
    }

    BMPFileHeader fileHeader;
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(BMPFileHeader));

    BMPInfoHeader bmpInfoHeader;
    file.read(reinterpret_cast<char*>(&bmpInfoHeader), sizeof(BMPInfoHeader));

    // Check for grayscale by expecting 8 bits per pixel
    std::cout << bmpInfoHeader.bit_count << std::endl;
    if(bmpInfoHeader.bit_count != 8) {
        std::cerr << "Unsupported bit depth for grayscale (expected 8-bit)." << std::endl;
        return NULL;
    }

    *width = bmpInfoHeader.width;
    *height = abs(bmpInfoHeader.height);

    // Allocate memory for the grayscale image
    unsigned char* data = new unsigned char[*width * *height];

    // Read the pixel data directly into the buffer
    file.seekg(fileHeader.offset_data, std::ios::beg);
    file.read(reinterpret_cast<char*>(data), *width * *height);

    file.close();
    return data;
}

/*
unsigned char* loadBMPGrayscale24bit(const char* filename, int* width, int* height) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Unable to open file " << filename << std::endl;
        return nullptr;
    }

    BMPFileHeader fileHeader;
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(BMPFileHeader));

    BMPInfoHeader bmpInfoHeader;
    file.read(reinterpret_cast<char*>(&bmpInfoHeader), sizeof(BMPInfoHeader));

    if (bmpInfoHeader.bit_count != 24) {
        std::cerr << "Unsupported bit depth (expected 24-bit for conversion to grayscale)." << std::endl;
        return nullptr;
    }

    *width = bmpInfoHeader.width;
    *height = abs(bmpInfoHeader.height);

    // Calculate padding for scanlines
    int padding = (4 - (*width * 3) % 4) % 4;
    
    // Allocate memory for the 24-bit color image
    unsigned char* colorData = new unsigned char[*width * *height * 3 + *height * padding];

    // Read the pixel data
    file.seekg(fileHeader.offset_data, std::ios::beg);
    file.read(reinterpret_cast<char*>(colorData), *width * *height * 3 + *height * padding);

    // Allocate memory for the grayscale image
    unsigned char* grayscaleData = new unsigned char[*width * *height];

    // Convert each pixel to grayscale
    for (int y = 0; y < *height; ++y) {
        for (int x = 0; x < *width; ++x) {
            unsigned char* pixel = colorData + (y * (*width * 3 + padding)) + (x * 3);
            unsigned char red = pixel[2];
            unsigned char green = pixel[1];
            unsigned char blue = pixel[0];
            grayscaleData[y * *width + x] = static_cast<unsigned char>(0.2989 * red + 0.5870 * green + 0.1140 * blue);
        }
    }

    // Clean up the color data
    delete[] colorData;

    file.close();
    return grayscaleData;
}


void saveImage8Bit(const char* filename, const unsigned char* temp, int width, int height) {
    BMPFileHeader file_header;
    BMPInfoHeader info_header;
    info_header.width = width;
    info_header.height = height;

    // Calculate padding to align rows to a 4-byte boundary
    int paddingAmount = (4 - (width % 4)) % 4;
    int paletteSize = 256 * 4; // 256 colors, 4 bytes per color
    file_header.file_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + paletteSize + (width + paddingAmount) * height;

    std::ofstream of(filename, std::ios_base::binary);

    if (!of) {
        std::cerr << "Could not open the output image file." << std::endl;
        return;
    }

    // Write headers
    of.write((const char*)&file_header, sizeof(file_header));
    of.write((const char*)&info_header, sizeof(info_header));

    // Write the color palette (256 shades of gray)
    for (int i = 0; i < 256; ++i) {
        unsigned char paletteEntry[4] = {static_cast<unsigned char>(i), static_cast<unsigned char>(i), static_cast<unsigned char>(i), 0};
        of.write((char*)paletteEntry, 4);
    }

    // Write pixel data
    for (int y = height - 1; y >= 0; y--) { // BMP images are stored upside-down
        of.write((char*)(temp + y * width), width);
        if (paddingAmount > 0) {
            static const unsigned char padding[3] = {0};
            of.write((char*)padding, paddingAmount);
        }
    }

    of.close();
    std::cout << "Image saved to " << filename << std::endl;
}



unsigned char* loadBMP(const char* filename, int* width, int* height) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Unable to open file " << filename << std::endl;
        return NULL;
    }

    BMPFileHeader fileHeader;
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(BMPFileHeader));

    BMPInfoHeader bmpInfoHeader;
    file.read(reinterpret_cast<char*>(&bmpInfoHeader), sizeof(BMPInfoHeader));

    std::cout << bmpInfoHeader.bit_count << std::endl;

    if(bmpInfoHeader.bit_count != 24) {
        std::cerr << "Unsupported bit depth (only 24-bit supported)." << std::endl;
        return NULL;
    }

    *width = bmpInfoHeader.width;
    *height = abs(bmpInfoHeader.height);

    unsigned char* data = new unsigned char[*width * *height];
    unsigned char* pixelData = new unsigned char[bmpInfoHeader.size_image];
    file.seekg(fileHeader.offset_data, std::ios::beg);
    file.read(reinterpret_cast<char*>(pixelData), bmpInfoHeader.size_image);

    // Convert to grayscale
    for(int y = 0; y < *height; y++) {
        for(int x = 0; x < *width; x++) {
            int index = (y * *width + x) * 3;
            unsigned char b = pixelData[index];
            unsigned char g = pixelData[index + 1];
            unsigned char r = pixelData[index + 2];

            // Simple average for grayscale
            data[y * *width + x] = (r + g + b) / 3;
        }
    }

    delete[] pixelData;
    file.close();
    return data;
}
*/
