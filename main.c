#include "kissLIB.h"
#include <stdio.h>
#include <stdlib.h>


void write(uint8_t byte)
{
    printf("%02X ", byte);
}

void read(uint8_t *byte, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++)
    {
        scanf("%hhX", &byte[i]);
    }
}


int main()
{
    kiss_instance_t kiss;
    uint8_t buffer[256];
    uint16_t length;
    uint8_t header = 0;

    kiss_init(&kiss, buffer, (uint16_t)sizeof(buffer), write, read);

    kiss_receive_frame(&kiss, buffer, &length);

    kiss_decode(&kiss, buffer, &length, &header);

    printf("Decoded data (%d bytes): ", length);
    printf("Header: %02X\n", header);
    for (uint16_t i = 0; i < length; i++)
    {
        printf("%c", (char)buffer[i]);
    }
    printf("\n");

    return 0;
}
