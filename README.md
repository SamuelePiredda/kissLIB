# kissLIB
Small and portable KISS (Keep It Simple, Stupid) protocol library written in C, designed for embedded systems and computers.
It provides a lightweight framing layer to communicate reliably between devices over any physical interface (UART, I2C, SPI, etc.).

## Features
- **Portable**: Written in standard C (C99), compliant with MISRA-C 2012 guidelines where possible.
- **Agnostic**: Works with any physical layer by using user-defined callbacks.
- **Framing**: Handles FEND/FESC escaping and unescaping transparently.
- **CRC32**: Optional CRC32 integrity check.
- **Control Frames**: Built-in support for ACK, NACK, PING, Commands, and Parameters.

## Integration

### 1. Define Hardware Callbacks
Implement the read and write functions for your specific hardware.

```C
// Write callback: sends 'length' bytes from 'data' to the hardware
typedef int32_t (*kiss_write_fn)(kiss_instance_t *const kiss, const uint8_t *const data, size_t length);

// Read callback: reads up to 'dataLen' bytes into 'buffer', updating 'read' with the count
typedef int32_t (*kiss_read_fn)(kiss_instance_t *const kiss, uint8_t *const buffer, 
                            size_t dataLen, size_t *const read);
```
Inside the kiss_instance_t structure you will find the pointer to the physical layer handler so that you can use whatever interface to read from and write to.



This is the struct containing the instance of the kiss communication protocol: the buffer array; buffer size; the current index or current amount of valid data in the buffer; the transmission delay after receiving; write/read callback functions; the current status of the link; context pointer with all the information about the physical layer.
```C
struct kiss_instance_t {
    uint8_t *buffer;
    uint8_t header; 
    uint8_t TXdelay;
    uint8_t Status; 
    uint8_t padding; 
    uint8_t CRC32; 

    size_t buffer_size; 
    size_t index;  

    kiss_write_fn write; 
    kiss_read_fn read;    

    void *context; 
};
```

The **buffer** pointer contains the buffer array that the user has created. This is done in order to allow user to use static or dynamic memory allocation as he wishes. The **buffer_size** contains the length of the buffer. The **index** parameter contains the length of the frame that is ready to be transmitted or that has been received, it should not be used by the user since all the kiss functions use it. The **TXdelay** is the delay between receiving and transmitting and it is a number between 0 and 255, (which should be multiply by 10 so it is a delay that ranges between 0 and 2550ms) this value is used by the user and it is never used by the library. The **write** and **read** functions are the callback functions that the user must code in order to transmit and receive from whatever physical link (please keep in mind that it is not a multi-point protocol so you need another layer on top if you want to use kiss for multi-point links e.g. CAN bus). The **Status** variable contains the current status of the kiss instance and should not be modified by the user, only read to be sure in what state the kiss intance is in. The **context** pointer is an extra pointer that the user can use pointing at useful structures (e.g. in HAL you can use UART_HandleTypeDef). The **padding** parameter is the amount of FEND byte to send before the real frame (it is a number between 0 and 32). The **header** parameter contains the header frame that must be transmitted or that it has been decoded.


Start by creating the instance of kiss
```C
kiss_instance_t kiss_i;
```

Then call the initialization function with all the necessary parameters
```C
int32_t kiss_init(kiss_instance_t *const kiss, uint8_t *const buffer, size_t buffer_size, 
                    uint8_t TXdelay, kiss_write_fn write, kiss_read_fn read, 
                    void *const context, uint8_t padding, uint8_t crc32);
```
Each kiss_instance_t use one buffer for input/output. This buffer allocation is done by the user which can select the right amount of bytes to allocate to it. Use static allocation.
The CRC32 can be setted with '1' or the define **KISS_USE_CRC32**, use '0' or **KISS_NOTUSE_CRC32** if you don't want to add CRC32 at the end of the frame.
Remember that each byte is 2 byte long potentially due to the special bytes FEND and FESC. This means that the maximum payload data is significally smaller respect to the maximum frame size which is calculated by taking into account that potentially, every byte is an escape character and needs two bytes. This is extremely safe but necessary in embedded systems to avoid any possible error. 
The following table shows how many payload bytes can be fitted with and without CRC32:

| Buffer size      | Payload bytes (no CRC) | Payload bytes (with CRC) |
| :-----------:      | :-----------:            | :-----------:              |
| 16          | 6                  | 2                         |
| 32          | 14                   | 10                         |
| 64          | 30                   | 26                         |
| 128         | 62                   | 58                         |
| 256         | 126                   | 122                         |
| 512         | 254                   | 250                         |
| 1024        | 510                   | 506                        |


```C
const size_t len = 1024;
uint8_t buffer_kiss[len];
```
I suggest to use buffer size >= 128 bytes in order to have the best ratio payload bytes over buffer size and maximize the efficiency. In this example we have no RAM problem and we set a 1024 buffer size


If you want to send data, use the *kiss_push_data* to push data inside the kiss buffer
```C
int32_t kiss_push_data(kiss_instance_t *const kiss, const uint8_t *const data, size_t length);
```
Before sending the frame remember to set the header of the frame using the *kiss_set_header* function.
```C
int32_t kiss_set_header(kiss_instance_t *const kiss, uint8_t header);
```

After the data that you want to send has been pushed into the buffer, you can use *kiss_send_frame* function to send it
```C
int32_t kiss_send_frame(kiss_instance_t *const kiss);
```

Use this function to wait for a kiss frame arriving
```C
int32_t kiss_receive_frame(kiss_instance_t *const kiss, uint32_t maxAttempts);
```


These are quick functions for transmitting quick command frames
```C
int32_t kiss_set_TXdelay(kiss_instance_t *const kiss, uint8_t tx_delay);
int32_t kiss_set_speed(kiss_instance_t *const kiss, uint32_t BaudRate);
int32_t kiss_send_ack(kiss_instance_t *const kiss);
int32_t kiss_send_nack(kiss_instance_t *const kiss);
int32_t kiss_send_ping(kiss_instance_t *const kiss);
int32_t kiss_set_param(kiss_instance_t *const kiss, uint16_t ID, 
                    const uint8_t *const param, size_t len);
int32_t kiss_request_param(kiss_instance_t *const kiss, uint16_t ID, uint8_t *const output, 
                    size_t max_out_size, size_t *const output_length, uint32_t maxAttempts, 
                    uint8_t expected_header);
int32_t kiss_send_command(kiss_instance_t *const kiss, uint16_t *command);
```

All the function return *KISS_OK* (0) if the operation is successful, otherwise they return a non zero value that is the error code. The error codes are defined as follows:

```C
#define KISS_ERR_INVALID_PARAMS         1 /* the data passed to the function are not valid */
#define KISS_ERR_INVALID_FRAME          2 /* the frame received is not valid */
#define KISS_ERR_BUFFER_OVERFLOW        3 /* the data to send is too big for the buffer */
#define KISS_ERR_NO_DATA_RECEIVED       4 /* no data has been received within the maxAttempts */
#define KISS_ERR_DATA_NOT_ENCODED       5 /* the data received is not properly encoded */
#define KISS_ERR_CRC32_MISMATCH         6 /* the CRC32 of the received frame does not match the CRC32 locally calcualated */
#define KISS_ERR_CALLBACK_MISSING       7 /* the write or read callback function are missing, cannot read or write from the physical layer */
#define KISS_ERR_HEADER_ESCAPE          8 /* the header byte is an escape charachter and it cannot be used as a header */
#define KISS_ERR_STATUS                 9 /* the kiss instance is in an error status */
#define KISS_ERR_PADDING_OVERFLOW       10 /* the padding value is too big, it must be between 0 and 32 */
```

By changing the kiss instance CRC32 parameter all the functions will automatically use or not use the CRC32 so you can decide to use it or not on a frame by frame basis. The same thing applies for the padding parameter, you can change it at any time and the next frame will be sent with the new padding value.

The list of the predefined headers are the following:
```C
#define KISS_HEADER_DATA(port)      ((uint8_t)(port & 0x0F)) /* header for data frame with port number from 0 to 15 */
#define KISS_HEADER_TX_DELAY        0x10    /* header for setting the other device delay time */
#define KISS_HEADER_SPEED           0x60    /* header for setting the link speed (better wait for an ACK before changing the baud rate) */
#define KISS_HEADER_PING            0x80    /* header for ping frame */
#define KISS_HEADER_ACK             0xA0    /* header for ack frame */
#define KISS_HEADER_NACK            0xA5    /* header for nack frame */
#define KISS_HEADER_REQUEST_PARAM   0x40    /* header for request parameter frame */
#define KISS_HEADER_SET_PARAM       0x50    /* header for set parameter frame */
#define KISS_HEADER_COMMAND         0x70    /* header for command frame */
```