#include "kissLIB.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>


HANDLE hSerial;


/* Write function for KISS framing */
int write(kiss_instance_t *kiss, uint8_t *data, size_t dataLen)
{
    DWORD bytesWritten;

    if(!WriteFile(kiss->context, data, dataLen, &bytesWritten, NULL))
    {
        fprintf(stderr, "Error writing to serial port\n");
        return 1;
    }

    return 0;
}

/* Read function for KISS framing */
int read(kiss_instance_t *kiss, uint8_t *buffer, size_t dataLen, size_t *read)
{
    if(!ReadFile(kiss->context, buffer, dataLen, (LPDWORD)read, NULL))
    {
        fprintf(stderr, "Error reading from serial port\n");
        return 1;
    }

    return 0;
}

void printKissStatus(kiss_instance_t *kiss)
{
    printf("KISS Buffer Size: %d\n", kiss->buffer_size);
    printf("KISS Index: %d\n", kiss->index);
    printf("KISS Buffer Content: (at %d)\n", kiss->buffer);
    printf("KISS TX Delay: %d\n", kiss->TXdelay);
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

void switchStatus(int status)
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
        default:
            printf("UNKNOWN STATUS\n");
            break;
    }
}


/** Example main function demonstrating KISS receive and decode.
 */
int main()
{
    hSerial = CreateFile("COM3", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Error opening serial port\n");
        return 1;
    }
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if(!GetCommState(hSerial, &dcbSerialParams))
    {
        fprintf(stderr, "Error getting serial port state\n");
        CloseHandle(hSerial);
        return 1;
    }

    dcbSerialParams.BaudRate = CBR_9600;
    dcbSerialParams.ByteSize = 8;  
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;

    if(!SetCommState(hSerial, &dcbSerialParams))
    {
        fprintf(stderr, "Error setting serial port state\n");
        CloseHandle(hSerial);
        return 1;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if(!SetCommTimeouts(hSerial, &timeouts))
    {
        fprintf(stderr, "Error setting serial port timeouts\n");
        CloseHandle(hSerial);
        return 1;
    }


    #define MAX_BUFFER_SIZE 1024
    uint8_t buffer[MAX_BUFFER_SIZE];

    kiss_instance_t kiss;
    kiss_init(&kiss, buffer, MAX_BUFFER_SIZE, 1, write, read, hSerial);


    char str[] = "Hello,World!";

    kiss_encode(&kiss, (uint8_t*)str, strlen(str), KISS_HEADER_DATA(0));

    printKissStatus(&kiss);

    do
    {

        kiss_send_frame(&kiss);

        if(kiss.Status != KISS_TRANSMITTED)
        {
            printf("Error: Frame not transmitted properly.\n");
            exit(1);
        }
        else
        {
            printf("Frame transmitted successfully.\n");
        }

        Sleep(kiss.TXdelay / 100.0);

        kiss_receive_frame(&kiss, 1);

        printf("KISS Status after receive: ");
        switchStatus(kiss.Status);
        printf("\n");   
 
    } while (1);
    

    CloseHandle(hSerial);

    return 0;
}
