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

/** Example main function demonstrating KISS receive and decode.
 */
int main()
{
    // Create KISS instance and buffers
    kiss_instance_t kiss;
    uint8_t buffer[256];
    // length of data received storage
    uint16_t length = 0;
    // KISS header storage
    uint8_t header = 0;

    // Initialize KISS instance
    kiss_init(&kiss, buffer, (uint16_t)sizeof(buffer), write, read);
    // Receive a KISS frame
    kiss_receive_frame(&kiss, buffer, &length);
    // Decode the received KISS frame
    kiss_decode(&kiss, buffer, &length, &header);
    // Print the decoded data length and header
    printf("Decoded data (%d bytes): ", length);
    printf("Header: %02X\n", header);

    // print the decoded data
    for (uint16_t i = 0; i < length; i++)
        printf("%c", (char)buffer[i]);

    printf("\n");

    return 0;
}
