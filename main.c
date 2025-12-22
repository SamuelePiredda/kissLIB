#include "kissLIB.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Write function for KISS framing */
void write(uint8_t byte)
{
    printf("%02X ", byte);
}

/* Read function for KISS framing */
void read(uint8_t *byte, uint16_t count)
{
    for(uint16_t i = 0; i < count; i++)
        scanf(" %2hhx", &byte[i]);
}

void printKissStatus(kiss_instance_t *kiss)
{
    printf("KISS Buffer Size: %d\n", kiss->buffer_size);
    printf("KISS Index: %d\n", kiss->index);
    printf("KISS Buffer Content: (at %d)\n", kiss->buffer);
    printf("KISS TX Delay: %d\n", kiss->TXdelay);
    printf("KISS Speed: %d\n", kiss->speed);
    for (uint16_t i = 0; i < kiss->index; i++)
    {
        printf("%02X ", kiss->buffer[i]);
    }
    printf("\n----------\n");
}


char randomCharacter()
{
    return (char)(rand() % 256);
}


/** Example main function demonstrating KISS receive and decode.
 */
int main()
{
    #define buffer_size  1024

    uint8_t buffer[buffer_size];


    kiss_instance_t kiss;


    kiss_init(&kiss, buffer, buffer_size, 10, 9600, write, read);

    printKissStatus(&kiss);

    kiss_encode(&kiss, (uint8_t *)"Hello, KISS!", 12, KISS_HEADER_DATA(0));

    printKissStatus(&kiss);

    char received_data[buffer_size];
    uint16_t received_length = 0;
    uint8_t header = 0;

    kiss_decode(&kiss, (uint8_t *)received_data, (uint16_t *)&received_length, &header);

    printf("Decoded Header: %02X\n", header);
    printf("Decoded Data (%d bytes): ", received_length);
    for (uint16_t i = 0; i < received_length; i++)
    {
        printf("%c", received_data[i]);
    }



    return 0;
}
