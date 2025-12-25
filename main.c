
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>



#include "kissLIB.h"

/* Write function for KISS framing */
int write(kiss_instance_t *kiss, uint8_t *data, size_t dataLen)
{
/*    for(size_t i = 0; i < dataLen; i++)
    {
        printf("%02X ", data[i]);
    }*/
    return 0;
}

/* Read function for KISS framing */
int read(kiss_instance_t *kiss, uint8_t *buffer, size_t dataLen, size_t *read)
{
    for(size_t i = 0; i < dataLen; i++)
    {
        scanf("%2hhx", &buffer[i]);
    }
    *read = dataLen;

    return 0;
}

void printKissStatus(uint8_t status)
{
    switch(status)
    {
        case KISS_NOTHING:
            printf("KISS_NOTHING\n");
            break;
        case KISS_TRANSMITTING:
            printf("KISS_TRANSMITTING\n");
            break;
        case KISS_TRANSMITTED:
            printf("KISS_TRANSMITTED\n");
            break;
        case KISS_RECEIVING:
            printf("KISS_RECEIVING\n");
            break;
        case KISS_RECEIVED:
            printf("KISS_RECEIVED\n");
            break;
        case KISS_RECEIVED_ERROR:
            printf("KISS_RECEIVED_ERROR\n");
            break;
        case KISS_ERROR_STATE:
            printf("KISS_ERROR_STATE\n");
            break;
        default:
            printf("Unknown status: %d\n", status);
            break;
    }
}

void printKiss(kiss_instance_t *kiss)
{
    printf("KISS Buffer Size: %d\n", kiss->buffer_size);
    printf("KISS Index: %d\n", kiss->index);
    printf("KISS Buffer Content: (at %d)\n", kiss->buffer);
    printf("KISS TX Delay: %d\n", kiss->TXdelay);
    printf("KISS Status: ");
    printKissStatus(kiss->Status);
    printf("KISS Buffer Data: \n");
    for (uint16_t i = 0; i < kiss->index; i++)
    {
        printf("%02X ", kiss->buffer[i]);
    }
    printf("\n----------\n");
}

void printErrorkiss(int err)
{
    switch(err)
    {
        case KISS_ERR_INVALID_PARAMS:
            printf("KISS_ERR_INVALID_PARAMS\n");
            break;
        case KISS_ERR_BUFFER_OVERFLOW:
            printf("KISS_ERR_BUFFER_OVERFLOW\n");
            break;
        case KISS_ERR_INVALID_FRAME:
            printf("KISS_ERR_INVALID_FRAME\n");
            break;
        case KISS_ERR_NO_DATA_RECEIVED:
            printf("KISS_ERR_NO_DATA_RECEIVED\n");
            break;
        case KISS_ERR_DATA_NOT_ENCODED:
            printf("KISS_ERR_DATA_NOT_ENCODED\n");
            break;
        case KISS_ERR_CRC32_MISMATCH:
            printf("KISS_ERR_CRC32_MISMATCH\n");
            break;
        default:
            printf("Unknown error: %d\n", err);
            break;
    }
}


/** Example main function demonstrating KISS receive and decode.
 */
int main()
{
    // buffer for KISS instance
    uint8_t buffer[1024];
    // buffer for received data
    uint8_t output[1024];
    // KISS header byte
    uint8_t header;
    size_t len = 0;


    kiss_instance_t kiss;

    int kiss_err = 0;

    // Initialize KISS instance
    kiss_err = kiss_init(&kiss, buffer, sizeof(buffer), 100, write, read, NULL);
    if (kiss_err != 0)
    {
        fprintf(stderr, "Failed to initialize KISS instance: %d\n", kiss_err);
        return EXIT_FAILURE;
    }
    else
    {
        printf("KISS instance initialized successfully.\n");
    }

    // Encode and send test data with CRC32
    size_t lenTest = 4;
    uint8_t testData[] = {0x41, 0x42, 0x41, 0x42}; // "ABAB"

    printKiss(&kiss);

    // Encode with CRC32
    kiss_err = kiss_encode_crc32(&kiss, testData, &lenTest, KISS_HEADER_DATA(0));


    printKiss(&kiss);

    // Send encoded frame
    kiss_err = kiss_send_frame(&kiss);

    // Simulate receiving the same data (for testing, we use the same buffer)
    kiss.buffer[2] = 0x43; // Corrupt data for testing

    printKiss(&kiss);

    // Receive and decode with CRC32 verification
    kiss_err = kiss_decode_crc32(&kiss, output, &len, &header);

    printKiss(&kiss);
        
    // Print any error
    if(kiss_err != 0)
    {
        printf("\nError: ");
        printErrorkiss(kiss_err);
    }

    return 0;
}
