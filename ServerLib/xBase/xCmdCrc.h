#ifndef _NET_PACKET_H_
#define _NET_PACKET_H_

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>

static uint32_t crc32_table[256] = {0};
const uint32_t crc32_key = 0xEDB88328;

void make_crc32_table() {
    uint32_t c;
    int i = 0;
    int bit = 0;

    for (i = 0; i < 256; i++) {
        c = (uint32_t)i;

        for (bit = 0; bit < 8; bit++) {
            if (c & 1) {
                c = (c >> 1) ^ (crc32_key);
            } else {
                c = c >> 1;
            }
        }
        crc32_table[i] = c;
    }
}

uint32_t make_crc(uint32_t crc, unsigned char *string, uint32_t size) {

    while (size--)
        crc = (crc >> 8) ^ (crc32_table[(crc ^ *string++) & 0xff]);

    return crc;
}


#endif

