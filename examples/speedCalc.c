#include "../kissLIB.h"
#include "../kissLIB.c"
#include <stdio.h>
#include <windows.h>


// File to write raw KISS frames
FILE *f;

// Write callback function for KISS framing 
int write(kiss_instance_t *kiss, uint8_t *data, size_t dataLen)
{
    fwrite(data, 1, dataLen, f);
    return 0;
}



int main()
{
    LARGE_INTEGER freq, start, end; // For high-resolution timing
    double elapsedTime; // Time in seconds
    #define buff_size 524288 // Maximum KISS frame size

    // Run multiple tests to average results
    for(int j = 0; j < 5; j++)
    {
        f = fopen("speedTest.txt", "w");    // Open file to write raw KISS frames

        QueryPerformanceFrequency(&freq);   // Get timer frequency
        QueryPerformanceCounter(&start);    // Start timer

        // Initialize KISS instance
        kiss_instance_t kiss;       

        // Prepare buffer with sample data
        uint8_t buffer[buff_size];
        for(int i = 0; i < buff_size; i++)
            buffer[i] = (uint8_t)(i % 256);
        uint8_t sending[buff_size];
        for(int i = 0; i < buff_size; i++)
            sending[i] = (uint8_t)(i % 256);

        // Initialize KISS instance
        kiss_init(&kiss, buffer, buff_size, 1, write, NULL, NULL, 0);

        // error storage variable
        int err;

        // Put all code to be timed here

        size_t frameSize = 0;
        size_t totBytes = 0;
        
        // Send data until 3 GB is reached
        while (totBytes < 3e9)
        {
            // Prepare frame, with a bit of variation since with encoding the size may increase up to 2x
            frameSize = (buff_size - 8)/2;
            err = kiss_encode(&kiss, sending, &frameSize, KISS_HEADER_DATA(0));
            totBytes += frameSize;
            if (err != 0)
            {
                printf("Error in kiss_encode_and_send: %d\n", err);
                return err;
            }     
            err = kiss_send_frame(&kiss);
            if (err != 0)
            {
                printf("Error in kiss_encode_and_send: %d\n", err);
                return err;
            }   
            
        }

        
        // End of code to be timed

        QueryPerformanceCounter(&end);

        fclose(f);

        // Calculate and print the results of the simulation 

        elapsedTime = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;
        printf("Test %d:\n", j+1);
        printf("Frequency: %lld\n", freq.QuadPart);
        printf("Elapsed time: %f seconds\n", elapsedTime);
        printf("Total bytes sent: %f Mb\n", (double)(totBytes)/1e6);
        printf("Throughput: %f Mbps\n", totBytes / elapsedTime / 1e6);
    }
    return 0;
}