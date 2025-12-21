#include "kissLIB.h"
#include <stdio.h>
#include <stdlib.h>

/* Write function for KISS framing */
void write(uint8_t byte)
{
    printf("%02X ", byte);
}

/* Read function for KISS framing */
void read(uint8_t *byte, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++)
    {
        scanf("%hhX", &byte[i]);
    }
}

void printKissStatus(kiss_instance_t *kiss)
{
    printf("KISS Buffer Size: %d\n", kiss->buffer_size);
    printf("KISS Index: %d\n", kiss->index);
    printf("KISS Buffer Content: (at %d)\n", kiss->buffer);
    for (uint16_t i = 0; i < kiss->index; i++)
    {
        printf("%02X ", kiss->buffer[i]);
    }
    printf("\n----------\n");
}



/** Example main function demonstrating KISS receive and decode.
 */
int main()
{
    // Create KISS instance and buffers
    kiss_instance_t kiss;
    uint8_t buffer[256];

    uint8_t output[256];
    // length of data received storage
    uint16_t length = 0;
    // KISS header storage
    uint8_t header = 0;

    uint8_t error = 0;

    // Initialize KISS instance
    error = kiss_init(&kiss, buffer, (uint16_t)sizeof(buffer), write, read);
    if(error != 0)
    {
        printf("KISS initialization failed with error: %d\n", error);
        return -1;
    }


    printKissStatus(&kiss);

    // Receive a KISS frame
    error = kiss_receive_frame(&kiss, (uint32_t)sizeof(buffer));
    if(error != 0)
    {
        printf("KISS receive frame failed with error: %d\n", error);
        return -1;
    }


    printKissStatus(&kiss);

    // Decode the received KISS frame
    error = kiss_decode(&kiss, output, &length, &header);
    if(error != 0)
    {
        printf("KISS decode failed with error: %d\n", error);
        return -1;
    }

    printKissStatus(&kiss);


    // Print the decoded data length and header
    printf("Decoded data (%d bytes): ", length);
    printf("Header: %02X\n", header);

    // print the decoded data
    for (uint16_t i = 0; i < length; i++)
        printf("%c", (char)output[i]);

    printf("\n");

    return 0;
}
