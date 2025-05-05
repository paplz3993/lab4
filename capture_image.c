#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include "bmp_utility.h"

#define HW_REGS_BASE (0xff200000)
#define HW_REGS_SPAN (0x00200000)
#define HW_REGS_MASK (HW_REGS_SPAN - 1)
#define LED_BASE 0x1000
#define PUSH_BASE 0x3010
#define VIDEO_BASE 0x0000

#define IMAGE_WIDTH 240
#define IMAGE_HEIGHT 240

#define FPGA_ONCHIP_BASE (0xC8000000)
#define IMAGE_SPAN (IMAGE_WIDTH * IMAGE_HEIGHT * 4)
#define IMAGE_MASK (IMAGE_SPAN - 1)

// Define the target dimensions for the scaled image
#define SCALED_WIDTH 28
#define SCALED_HEIGHT 28

int main(void) {
    volatile unsigned int *video_in_dma = NULL;
    volatile unsigned int *key_ptr = NULL;
    volatile unsigned short *video_mem = NULL;
    void *virtual_base;
    void *video_base;
    int fd;

    // Open /dev/mem
    if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
        printf("ERROR: could not open \"/dev/mem\"...\n");
        return 1;
    }

    // Map physical memory into virtual address space
    virtual_base = mmap(NULL, HW_REGS_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, HW_REGS_BASE);
    if (virtual_base == MAP_FAILED) {
        printf("ERROR: mmap() failed...\n");
        close(fd);
        return 1;
    }

    // Map physical memory of video into virtual address space
    video_base = mmap(NULL, IMAGE_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, FPGA_ONCHIP_BASE);
    if (video_base == MAP_FAILED) {
        printf("ERROR: IVB mmap() failed...\n");
        close(fd);
        return 1;
    }

    // Calculate the virtual address where our device is mapped
    video_in_dma = (unsigned int *)(virtual_base + ((VIDEO_BASE) & (HW_REGS_MASK)));
    key_ptr = (unsigned int *)(virtual_base + ((PUSH_BASE) & (HW_REGS_MASK)));
    video_mem = (unsigned short *)(video_base + ((FPGA_ONCHIP_BASE) & (IMAGE_MASK)));

    int value = *(video_in_dma + 3);

    printf("Video In DMA register updated at:0x%x\n", (video_in_dma));

    // Modify the PIO register
    *(video_in_dma + 3) = 0x4;

    value = *(video_in_dma + 3);

    printf("enabled video:0x%x\n", value);

    while (1) {
        value = *key_ptr;
        if (*key_ptr != 7) {
            printf("button pressed\n");
            break;
        }
    }

    *(video_in_dma + 3) = 0x0;

    value = *(video_in_dma + 3);

    printf("disabled video:0x%x\n", value);

    // Arrays to store the original and grayscale images
    unsigned short pixels[IMAGE_HEIGHT][IMAGE_WIDTH];
    unsigned char pixels_bw[IMAGE_HEIGHT][IMAGE_WIDTH];

    // Capture the image and convert to grayscale
    int x, y;
    for (y = 0; y < IMAGE_HEIGHT; y++) {
        for (x = 0; x < IMAGE_WIDTH; x++) {
            pixels[y][x] = *(video_mem + (y << 9) + x);
            int red = (pixels[y][x] >> 11) & 0x1f;
            int green = (pixels[y][x] >> 5) & 0x3f;
            int blue = pixels[y][x] & 0x1f;
            int red8 = (red << 3) | (red >> 2);
            int green8 = (green << 2) | (green >> 4);
            int blue8 = (blue << 3) | (blue >> 2);
            int gray8 = (red8 + green8 + blue8) / 3;
            int red5 = gray8 >> 3;
            int green6 = gray8 >> 2;
            int blue5 = gray8 >> 3;
            pixels_bw[y][x] = (red5 << 11) | (green6 << 5) | blue5;
        }
    }

    // Save the original 240x240 images
    const char* filename = "final_image_color.bmp";
    saveImageShort(filename, &pixels[0][0], 240, 240);

    const char* filename1 = "final_image_bw.bmp";
    saveImageGrayscale(filename1, &pixels_bw[0][0], 240, 240);

    // Step 1: Create an array to store the scaled 28x28 grayscale image
    unsigned char scaled_pixels_bw[SCALED_HEIGHT][SCALED_WIDTH];

    // Step 2: Calculate the scaling factor
    float scale_x = (float)IMAGE_WIDTH / SCALED_WIDTH;  // 240 / 28 ≈ 8.57
    float scale_y = (float)IMAGE_HEIGHT / SCALED_HEIGHT; // 240 / 28 ≈ 8.57

    // Step 3: Downscale the image by averaging blocks of pixels
    for (int sy = 0; sy < SCALED_HEIGHT; sy++) {
        for (int sx = 0; sx < SCALED_WIDTH; sx++) {
            // Calculate the corresponding block in the original image
            int start_x = (int)(sx * scale_x);
            int start_y = (int)(sy * scale_y);
            int end_x = (int)((sx + 1) * scale_x);
            int end_y = (int)((sy + 1) * scale_y);

            // Ensure we don't go out of bounds
            if (end_x > IMAGE_WIDTH) end_x = IMAGE_WIDTH;
            if (end_y > IMAGE_HEIGHT) end_y = IMAGE_HEIGHT;

            // Sum the grayscale values in the block
            int sum = 0;
            int count = 0;
            for (int y = start_y; y < end_y; y++) {
                for (int x = start_x; x < end_x; x++) {
                    // Extract the grayscale value from pixels_bw
                    // Since pixels_bw is in RGB565 format, extract the gray value
                    int gray = pixels_bw[y][x] & 0x1f; // Use blue channel (same as red and green due to grayscale conversion)
                    int gray8 = (gray << 3) | (gray >> 2); // Convert back to 8-bit
                    sum += gray8;
                    count++;
                }
            }

            // Compute the average and store in the scaled image
            scaled_pixels_bw[sy][sx] = (unsigned char)(sum / count);
        }
    }

    // Step 4: Save the scaled 28x28 grayscale image
    const char* filename_scaled = "final_image_scaled.bmp";
    saveImageGrayscale(filename_scaled, &scaled_pixels_bw[0][0], SCALED_WIDTH, SCALED_HEIGHT);

    // Clean up
    if (munmap(virtual_base, HW_REGS_SPAN) != 0) {
        printf("ERROR: munmap() failed...\n");
        close(fd);
        return 1;
    }

    if (munmap(video_base, IMAGE_SPAN) != 0) {
        printf("ERROR: video munmap() failed...\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}