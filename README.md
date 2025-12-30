# kissLIB
small and portable KISS library for C that can be used for embedded.



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
            size_t *length, const uint8_t header);
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
                    size_t *length, const uint8_t header);
int kiss_encode_send_crc32(kiss_instance_t *kiss, uint8_t *data, 
                    size_t *length, uint8_t header)
```


# Example on how to implement the library


You start by adding the include instruction, no other includes are required
```C
#include "kissLIB.h"
```

Then, the core of the library is the **kiss_instance_t** which contains all the information used for the kiss comunication.
Create the instance
```C
...
kiss_instance_t kissI;
...
```
To use the kiss instance you need two buffers. One is used by the kiss instance and the other one is to get the data when a frame is received.
```C
uint8_t buffer_kissI[256];
uint8_t kissI_out[128];
uint8_t kissI_out_index = 0;
```
The **buffer** array is used by the instance while the **kissI_out** is used to store output payload data when received. It is important to know in advance the maximum length of the frame and use some safe coefficent. If you expect **64** data bytes for each frame, 3 more bytes are needed for frame encapsulation, and the worst case scenario is that all 64 bytes are special, hence they become 128 bytes. The worst case scenario is 131 bytes. If use padding bytes at the start the length is even greater. So in this case **256** bytes are more than enough.

Then you can initialize a header variable which will be set when a frame arrives.
```C
uint8_t kissI_header;
```

In the setup function or region, initialize the kiss instance with the init function.
```C
...
kiss_init(&kissI, buffer_kissI, 256, 1, write, read, context, 0);
...
```
The parameters passed are the kiss instance; the buffer used by it; the maximum length of the buffer; the transmit delay after the 0-255 in milliseconds (which is multiplied by 10, so 0-2550 ms); write callback function written by the user; read callback function written by the user; context needed for writing/reading if needed (for example if the instnace of I2C or UART is needed by the callback functions); the number of padding/sync bytes to send before the frame.

