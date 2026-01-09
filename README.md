# kissLIB
small and portable KISS library written in C that can be used in any computer or embedded system. 
It is a small and easy protocol which allows you to communicate with other devices using any physical layer.



These are the two callback functions that the user must implement for writing and receiving, for whataver physical layer (I2C, UART etc..)
``` C
typedef void (*kiss_write_fn)(kiss_instance_t *kiss, uint8_t *data, size_t length);
typedef void (*kiss_read_fn)(kiss_instance_t *kiss, uint8_t *buffer, size_t dataLen, size_t *read);
```
Inside the kiss_instance_t structure you will find the pointer to the physical layer handler so that you can use whatever interface to read from and write to.



This is the struct containing the instance of the kiss communication protocol: the buffer array; buffer size; the current index or current amount of valid data in the buffer; the transmission delay after receiving; write/read callback functions; the current status of the link; context pointer with all the information about the physical layer.
```C
typedef struct 
{
    uint8_t *buffer;
    size_t buffer_size;
    size_t index;
    uint8_t TXdelay;
    kiss_write_fn write;
    kiss_read_fn read;
    uint8_t Status;
    void *context;
} kiss_instance_t;
```

Start by creating the instance of kiss
```C
kiss_instance_t kiss_i;
```

Then call the initialization function with all the necessary parameters
```C
int kiss_init(kiss_instance_t *kiss, uint8_t *buffer, uint16_t buffer_size, 
                uint8_t TXdelay, uint32_t BaudRate, kiss_write_fn write, 
                kiss_read_fn read, void *context, uint8_t padding);
```
Each kiss_instance_t use one buffer for input/output. This buffer allocation is done by the user which can select the right amount of bytes to allocate to it. You can use static allocation or dynamic allocation.
```C
const size_t len = 1024;
uint8_t buffer_kiss[len];
```
If you plan to transmit packets that are X long, you have to create a buffer which is X + 2 (FEND) + 1 (header) + X (if you want to use CRC32 you need also to take into account +4 bytes for CRC32 at the end of the packet). This takes into account the worst case scenario when you have to transmit only special characters. For instance, if you want to transmit 256 bytes  per packet, please use at least 515 bytes as buffer, but in the example above 1024 bytes have been used in order to be in safe zone. If you use static allocation and you have buffer overflow you can't change the amount of memory allocated without changing the program.


If you want to send data, use the encode function to encode the data previous to sending
```C
int kiss_encode(kiss_instance_t *kiss, const uint8_t *data, 
            size_t length, const uint8_t header);
```

If you want to add more data after you have used the kiss_encode function use this function here:
```C
int kiss_push_encode(kiss_instance_t *kiss, uint8_t *data, size_t length)
```

After you have received a frame use this function to decode the data 
```C
int kiss_decode(kiss_instance_t *kiss, uint8_t *output, size_t output_max_size
            size_t *output_length, uint8_t *header);
```

After the data that you want to send has been encoded use this function to send it
```C
int kiss_send_frame(kiss_instance_t *kiss);
```

Use this function to wait for a kiss frame arriving
```C
int kiss_receive_frame(kiss_instance_t *kiss, uint32_t maxAttempts);
```


These are quick functions for transmitting quick command frames
```C
int kiss_set_TXdelay(kiss_instance_t *kiss, uint8_t tx_delay);
int kiss_set_speed(kiss_instance_t *kiss, uint32_t BaudRate);
int kiss_send_ack(kiss_instance_t *kiss);
int kiss_send_nack(kiss_instance_t *kiss);
int kiss_send_ping(kiss_instance_t *kiss);
int kiss_send_param(kiss_instance_t *kiss, uint16_t ID, 
                    uint8_t *param, size_t len, uint8_t header)
```

To quickly encode and send or receive and decode use the following functions
```C
int kiss_encode_and_send(kiss_instance_t *kiss, const uint8_t *data, 
            uint16_t length, const uint8_t header)
int kiss_receive_and_decode(kiss_instance_t *kiss, uint8_t *output, size_t output_max_size,
            size_t *output_length, uint32_t maxAttempts, uint8_t *header)
```

The following functions encode and decode the data with four more bytes for CRC32 at the end of the frame.
```C
int kiss_decode_crc32(kiss_instance_t *kiss, uint8_t *output, 
                    size_t *output_length, uint8_t *header);
int kiss_encode_crc32(kiss_instance_t *kiss, uint8_t *data,
                    size_t length, const uint8_t header);
int kiss_encode_send_crc32(kiss_instance_t *kiss, uint8_t *data, 
                    size_t length, uint8_t header)
int kiss_send_param_crc32(kiss_instance_t *kiss, uint16_t ID,
                    uint8_t *param, size_t len, uint8_t header)

```


Add the **#define KISS_DEBUG** directive to have access to the function which prints out the instance for debug
 ```C
void kiss_debug(kiss_instance_t *kiss)
```



# How to implement the library, simple example

This example guides you step-by-step through the library integration, from definiing the callbacks to transmitting and receiving data.


You start by adding the include instruction, no other includes are required
```C
#include "kissLIB.h"
```

## 1. Define hardware callbacks

First, tell the library how to send and read bytes from your hardware (e.g. UART, I2C etc..). Define two functions that match the kiss_write_fn and kiss_read_fn signatures.
```C
// Write callback example (TX)
// sends 'length' bytes from 'data' to the hardware
int write_callback(kiss_instance_t *kiss, uint8_t *data, size_t length)
{
    /* All code for sending bytes
    * if you need specific object you can call kiss->context pointer object
    * return a number not zero if there is an error otherwise return 0
    */
    return 0;
}

// Read callback example (RX)
// Reads up to 'dataLen' bytes and stores them in 'buffer'
// updates 'readBytes' with the actual number of bytes read.
int read_callback(kiss_instance_t *kiss, uint8_t *buffer, size_t dataLen, size_t *readBytes)
{
    /* 
    * All code for receiving bytes
    * please put a maximum waiting time
    * update readBytes to the real number of bytes read
    */
    return 0;
}
```

## 2. Create the buffer and instance

The library does not use dynamic memory allocation in order to avoid any memory leaks or bug. You must provide a "working buffer" that 
the library will use to build packets (adding FEND, FESC, CRC, etc..)

```C
#define KISS_BUFFER_SIZE 1024

// buffer and instance allocation
uint8_t kiss_work_buffer[KISS_BUFFER_SIZE];
kiss_instance_t my_kiss;
// kiss errors will be allocated inside this variable
int kiss_err;
```


## 3. Initialization

Call **kiss_init** to configure the instance with the buffer and callbacks defined in step 1.

```C
kiss_err = kiss_init(&my_kiss,
                     kiss_work_buffer,  // buffer allocated
                     KISS_BUFFER_SIZE,  // its size
                     1,                 // TX delay (1 = 10ms)
                     write_callback,    // write callback function
                     read_callback,     // read callback function
                     NULL,              // context (optional, useful for HAL drivers)
                     0                  // padding (usually 0)
                    );
if(kiss_err != 0)
{
    /* error handling */
}
```

## 4. Sending data (encoding + sending)

To send data, first encode the payload into the internal buffer (**kiss_encode**) and then physically send it (**kiss_send_frame**).


```C
char *msg = "Hello,World!";
/* 1. encode the message into the internal buffer
*  KISS_HEADER_DATA(0) indicates a data packet on port 0
*/
kiss_err = kiss_encode(&my_kiss, (uint8_t*)msg, strlen(msg), KISS_HEADER_DATA(0));
if(kiss_err != 0)
{
    /* error handling */
}

/* sending the frame */
kiss_err = kiss_send_frame(&my_kiss);
if(kiss_err != 0)
{
    /* error handling */
}
```

## 5. Receiving Data (Receive & Decode)

When you are waiting for something to arrive you can use the **kiss_receive_frame** function. It reads
the bytes via the callback, assembles the frame and make it ready for decoding.

```C
/* receiving buffer */
uint8_t rx_buffer[KISS_BUFFER_SIZE];
/* length of data received */
size_t rx_len = 0;          
/* header of the frame received */
uint8_t rx_header;


/* try to receive with a maximum attempts */
kiss_err = kiss_receive_frame(&my_kiss, 1);

if(kiss_err == 0)
{
    /* packet have been received */

    /* decoding the message */
    kiss_err = kiss_decode(&my_kiss, rx_buffer, KISS_BUFFER_SIZE, &rx_len, &rx_header);

    if(kiss_err != 0)
    {
        /* error handling */
    }
    else
    {
        /* message has been received */
        /* printing data just to show what has been received */
        printf("Header:\t%02X\n", rx_header);
        printf("Bytes received\n");
        for(size_t i = 0; i < rx_len; i++)
            printf("%02X ", rx_buffer[i]);
        printf("\n---------\n");
    }

}
else if(kiss_err == KISS_ERR_NO_DATA_RECEIVED)
{
    /* no data received */
}
else
{
    /* handling any other error */
}
```
