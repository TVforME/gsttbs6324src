/*
 * test_offsets.c - Test program to verify encoder register offsets
 * 
 * This program can be used to verify the memory offsets used by the
 * TBS6324 encoder IOCTL interface. It helps debug register mapping issues.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/dvb/frontend.h>

/* Custom IOCTL definitions */
#define FE_24CXX_READ  _IOWR('o', 50, struct dvb_frontend_parameters)
#define FE_24CXX_WRITE _IOWR('o', 51, struct dvb_frontend_parameters)

/* Register offsets (as documented in encoder specification) */
#define REG_COMMAND      0x0000
#define REG_ENCODER_TYPE 0x0001
#define REG_VIDEO_WIDTH  0x0002
#define REG_VIDEO_HEIGHT 0x0003
#define REG_FPS          0x0004
#define REG_BITRATE      0x0005
#define REG_GOP          0x0006
#define REG_PROFILE      0x0007
#define REG_PMT_PID      0x0008
#define REG_VIDEO_PID    0x0009
#define REG_AUDIO_PID    0x000A
#define REG_AUDIO_CODEC  0x000B
#define REG_AUDIO_BITRATE 0x000C
#define REG_SERVICE_ID   0x000D

void print_register_info(void) {
    printf("TBS6324 Encoder Register Offsets:\n");
    printf("====================================\n");
    printf("COMMAND      : 0x%04X\n", REG_COMMAND);
    printf("ENCODER_TYPE : 0x%04X\n", REG_ENCODER_TYPE);
    printf("VIDEO_WIDTH  : 0x%04X\n", REG_VIDEO_WIDTH);
    printf("VIDEO_HEIGHT : 0x%04X\n", REG_VIDEO_HEIGHT);
    printf("FPS          : 0x%04X\n", REG_FPS);
    printf("BITRATE      : 0x%04X\n", REG_BITRATE);
    printf("GOP          : 0x%04X\n", REG_GOP);
    printf("PROFILE      : 0x%04X\n", REG_PROFILE);
    printf("PMT_PID      : 0x%04X\n", REG_PMT_PID);
    printf("VIDEO_PID    : 0x%04X\n", REG_VIDEO_PID);
    printf("AUDIO_PID    : 0x%04X\n", REG_AUDIO_PID);
    printf("AUDIO_CODEC  : 0x%04X\n", REG_AUDIO_CODEC);
    printf("AUDIO_BITRATE: 0x%04X\n", REG_AUDIO_BITRATE);
    printf("SERVICE_ID   : 0x%04X\n", REG_SERVICE_ID);
    printf("\n");
}

int main(int argc, char *argv[]) {
    int encoder = 0;
    char device[64];
    int fd;
    
    if (argc > 1) {
        encoder = atoi(argv[1]);
        if (encoder < 0 || encoder > 3) {
            fprintf(stderr, "Error: Encoder must be 0-3\n");
            return 1;
        }
    }
    
    print_register_info();
    
    snprintf(device, sizeof(device), "/dev/dvb/adapter%d/frontend0", encoder);
    
    printf("Opening device: %s\n", device);
    fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }
    
    printf("Device opened successfully\n");
    printf("This is a test program - no actual register access performed\n");
    
    close(fd);
    
    return 0;
}