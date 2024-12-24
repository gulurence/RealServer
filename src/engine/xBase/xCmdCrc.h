#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint32_t g_Crc32Table[256] = {0};
const uint32_t g_Crc32Key = 0xEDB88328;

void GMakeCrc32Table() { 
    uint32_t c;
    int i = 0;
    int bit = 0;

    for (i = 0; i < 256; i++) {
        c = (uint32_t)i;
        for (bit = 0; bit < 8; bit++) {
            if (c & 1) {
                c = (c >> 1) ^ (g_Crc32Key);
            } else {
                c = c >> 1;
            }
        }
        g_Crc32Table[i] = c;
    }
}

uint32_t GMakeCrc(uint32_t crc, unsigned char *string, uint32_t size) {
    while (size--) {
        crc = (crc >> 8) ^ (g_Crc32Table[(crc ^ *string++) & 0xff]);
    }
    return crc;
}







