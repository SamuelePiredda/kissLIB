#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <time.h>
#include <conio.h>

#include "kissLIB.h"


/* * Callback function for KISS framing transmission.
 * This simulates a physical TX channel by writing data to a local file.
 */
int32_t write(kiss_instance_t *const kiss, const uint8_t *const data, size_t dataLen)
{
    /* Simulate transmission latency by scaling the TXdelay parameter */
    Sleep(kiss->TXdelay*10);
    
    /* Open the output file in write-binary mode, overwriting existing content */
    FILE *f = fopen("obc.txt", "wb+");
    if(NULL == f) 
    {
        /* Return a custom error code if the file system is inaccessible */
        return 1000;
    }
    
    /* Write the entire KISS frame to the file */
    fwrite(data, 1, dataLen, f);
    fclose(f);
    return 0;
}

/* * Callback function for KISS framing reception.
 * Implements a "mailbox" system using a local file to simulate incoming data.
 */
int32_t read(kiss_instance_t *const kiss, uint8_t *const buffer, size_t dataLen, size_t *const read)
{
    FILE *f;
    uint32_t i = 0;
    do
    {
        /*
         * Synchronization Logic:
         * To prevent "Access Denied" errors when both processes (OBC and EPS) 
         * access the file simultaneously, we implement a polling loop with a 2ms pause.
         * The maximum number of attempts is retrieved from the kiss instance context.
         */
        Sleep(2);
        f = fopen("eps.txt", "rb");
    } 
    while (NULL == f && i++ < *((uint32_t*)kiss->context));     
    
    if(NULL == f)
    {
        /* No data found or file locked after all retries */
        return KISS_ERR_NO_DATA_RECEIVED;
    }
    
    /* Read the raw data into the buffer and store the number of bytes read */
    *read = fread(buffer, 1, dataLen, f);    
    fclose(f);
    
    /* Clean up the "channel" by deleting the file after a successful read */
    remove("eps.txt");
    return 0;
}

/* * Utility function to generate pseudo-random telemetry values.
 * Returns a value within the [low, up] inclusive range.
 */
uint32_t rand_sens(uint32_t low, uint32_t up)
{
    return low + (uint32_t)rand() % (up - low + 1);
}

int main()
{
    /* * Configuration Parameters:
     * These represent internal settings of the EPS that can be queried or modified.
     */
    #define PARAM1_ID 1
    uint16_t PARAM1 = 10;
    #define PARAM2_ID 2
    uint16_t PARAM2 = 15;
    #define PARAM3_ID 3
    uint16_t PARAM3 = 20;
    #define PARAM4_ID 4
    uint16_t PARAM4 = 25;

    /* * Sensor Simulation:
     * Dynamic values representing telemetry data (e.g., Voltage, Current, Temperature).
     */
    #define SENS1_ID 5
    uint32_t SENS1 = 100;
    #define SENS2_ID 6
    uint32_t SENS2 = 1000;
    #define SENS3_ID 7
    uint32_t SENS3 = 2000;

    /* General purpose data buffer for string storage */
    #define DATA_ID 8
    char DATA[128];

    /* Timing variable to manage the 1Hz update rate */
    time_t last_time = time(NULL);

    /* Command ID for resetting the internal data string */
    #define DATA_CMD_RESET 10

    /* Initialize the DATA string with a default message */
    strcpy(DATA, "Ciao");

    /* Structure containing the state and configuration of the KISS protocol */
    kiss_instance_t kiss_obc_i;

    /* Set maximum retry count for the read callback synchronization */
    uint32_t maxR = 10;

    int32_t kiss_obc_err = KISS_OK;
    #define MAX_BUFF 256

    /* * Internal KISS buffers:
     * buffer_obc: Used by the library for frame assembly/disassembly.
     * output_obc: Stores the decoded payload for the application layer.
     */
    uint8_t buffer_obc[MAX_BUFF];
    uint8_t output_obc[MAX_BUFF];
    size_t output_obc_len = 0;
    uint8_t header_obc;
    
    /* * Instance Initialization:
     * Links the callbacks and sets up the internal workspace.
     */
    kiss_obc_err = kiss_init(&kiss_obc_i, buffer_obc, MAX_BUFF, 1, write, read, &maxR, 0);
    if(kiss_obc_err != KISS_OK)
    {
        printf("Error init kiss instance\n");
        return 1;
    }

    /* Flag to force a UI refresh when new data arrives */
    int update = 0;

    /* --- Primary Execution Loop --- */
    while(1)
    {     
        /* * Attempt to fetch and parse a KISS frame.
         * The function returns KISS_OK only if a full valid frame is decoded.
         */
        kiss_obc_err = kiss_receive_and_decode(&kiss_obc_i, output_obc, MAX_BUFF, &output_obc_len, 1, &header_obc);
        
        if(KISS_OK == kiss_obc_err)
        {
            update = 1;
            /* Packet processing based on the KISS Protocol Header */
            switch(header_obc)
            {
                case KISS_HEADER_COMMAND:
                    /* Convert the first two bytes of the payload into a 16-bit command ID */
                    uint16_t cmd = KISS_BYTE_TO_UINT16(output_obc[0], output_obc[1]);
                    switch(cmd)
                    {
                        case DATA_CMD_RESET:
                            /* Clear the internal data string */
                            strcpy(DATA, "");
                            break;
                    }
                    break;

                case KISS_HEADER_DATA(5):
                    /* * Packet contains raw text data. 
                     * Clear local buffer and copy the new string safely. 
                     */
                    for(int i = 0; i < 128; i++)
                    {
                        DATA[i] = 0;
                    }
                    snprintf(DATA, sizeof(DATA), "%s", output_obc);
                    /* Ensure null-termination even if snprintf reaches length limit */
                    DATA[output_obc_len] = '\0';
                    break;

                case KISS_HEADER_REQUEST_PARAM:
                    /* * Master requested the value of a specific parameter.
                     * Extract the ID from the received frame.
                     */
                    uint16_t id = 0;
                    kiss_extract_param(&kiss_obc_i, &id, NULL, 0, NULL);
                    uint8_t *bs;
                    size_t len_bs = 0;

                    uint8_t exit_all = 0;
                    /* Map the ID to the local variable and split into bytes */
                    switch(id)
                    {
                        case PARAM1_ID:
                            bs = (uint8_t*)&PARAM1;
                            len_bs = 2;
                            break;
                        case PARAM2_ID:
                            bs = (uint8_t*)&PARAM2;
                            len_bs = 2;
                            break;
                        case PARAM3_ID:
                            bs = (uint8_t*)&PARAM3;
                            len_bs = 2;
                            break;
                        case PARAM4_ID:
                            bs = (uint8_t*)&PARAM4;
                            len_bs = 2;
                            break;     
                        case SENS1_ID:
                            bs = (uint8_t*)&SENS1;
                            len_bs = 4;  
                            break;
                        case SENS2_ID:
                            bs = (uint8_t*)&SENS2;
                            len_bs = 4;
                            break;
                        case SENS3_ID:
                            bs = (uint8_t*)&SENS3;
                            len_bs = 4;
                            break;      
                        default:
                            kiss_send_nack(&kiss_obc_i);
                            exit_all = 1;
                            break;
                    }

                    if(exit_all) 
                    {
                        break;
                    }
                    
                    /* * Build the response frame:
                     * 1. Start encoding with the ID as payload.
                     * 2. Append (push) the actual parameter value.
                     * 3. Send the final encapsulated frame.
                     */
                    kiss_obc_err = kiss_encode(&kiss_obc_i, (uint8_t *)&id, 2, KISS_HEADER_REQUEST_PARAM);
                    if(kiss_obc_err != KISS_OK)
                    {
                        printf("Error encoding ID\n");
                        return 1;
                    }
                    
                    kiss_obc_err = kiss_push_encode(&kiss_obc_i, bs, len_bs);
                    if(kiss_obc_err != KISS_OK)
                    {
                        printf("Error push encoding value\n");
                        return 1;
                    }
                    
                    /* Apply delay before sending to simulate hardware turnaround time */
                    Sleep(kiss_obc_i.TXdelay*10);
                    kiss_obc_err = kiss_send_frame(&kiss_obc_i);
                    if(kiss_obc_err != KISS_OK)
                    {
                        printf("Error sending value\n");
                        return 1;
                    }
                    break;

                case KISS_HEADER_SET_PARAM:
                    /* * Master requested to update a parameter value.
                     * Extract both the ID and the new value from the frame.
                     */
                    uint8_t value_b[8];
                    size_t len = 0;
                    kiss_obc_err = kiss_extract_param(&kiss_obc_i, &id, value_b, 8, &len);
                    
                    /* Reconstruct the 16-bit value from bytes */
                    uint16_t value = KISS_BYTE_TO_UINT16(value_b[0], value_b[1]);
                    
                    /* Apply the change to the corresponding local variable */
                    switch(id)
                    {
                        case PARAM1_ID:
                            PARAM1 = value;
                            break;
                        case PARAM2_ID:
                            PARAM2 = value;
                            break;
                        case PARAM3_ID:
                            PARAM3 = value;
                            break;
                        case PARAM4_ID:
                            PARAM4 = value;
                            break;
                    }
                    if(kiss_obc_err != KISS_OK)
                    {
                        printf("Error during extract set param %d\n", kiss_obc_err);
                        return 1;
                    }
                    break;
            }
        }

        /* * Housekeeping and UI Update:
         * Occurs every 1 second or immediately after receiving a valid packet.
         */
        if(time(NULL) - last_time >= 1 || 1 == update)
        {
            /* Clear console for a clean telemetry display */
            system("cls");
            update = 0;
            
            /* Update sensor readings with random noise for simulation realism */
            SENS1 = rand_sens(80, 120);
            SENS2 = rand_sens(900, 1100);
            SENS3 = rand_sens(1900, 2100);
            
            /* Update the timer for the next 1s interval */
            last_time = time(NULL);

            /* Output current System State to the user */
            printf("--- EPS SYSTEM STATUS ---\n");
            printf("PARAM1:\t%d\n", PARAM1);
            printf("PARAM2:\t%d\n", PARAM2);
            printf("PARAM3:\t%d\n", PARAM3);
            printf("PARAM4:\t%d\n", PARAM4);

            printf("-------------------------\n");
            
            printf("SENS1 (Voltage):\t%d\n", SENS1);
            printf("SENS2 (Current):\t%d\n", SENS2);
            printf("SENS3 (Temp):\t\t%d\n", SENS3);

            printf("-------------------------\n");

            printf("LATEST DATA:\t%s\n", DATA);
        }
    }

    return 0;
}
