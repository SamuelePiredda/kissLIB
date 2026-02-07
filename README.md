# kissLIB
small and portable KISS library written in C that can be used in any computer or embedded system. 
It is a small and easy protocol which allows you to communicate with other devices using any physical layer.



These are the two callback functions that the user must implement for writing and receiving, for whataver physical layer (I2C, UART etc..)
``` C
typedef int32_t (*kiss_write_fn)(kiss_instance_t *const kiss, const uint8_t *const data, size_t length);
typedef int32_t (*kiss_read_fn)(kiss_instance_t *const kiss, uint8_t *const buffer, 
                            size_t dataLen, size_t *const read);
```
Inside the kiss_instance_t structure you will find the pointer to the physical layer handler so that you can use whatever interface to read from and write to.



This is the struct containing the instance of the kiss communication protocol: the buffer array; buffer size; the current index or current amount of valid data in the buffer; the transmission delay after receiving; write/read callback functions; the current status of the link; context pointer with all the information about the physical layer.
```C
struct kiss_instance_t {
    uint8_t *buffer;
    size_t buffer_size;
    size_t index;
    uint8_t TXdelay;
    kiss_write_fn write;
    kiss_read_fn read;
    uint8_t Status;
    void *context;
    uint8_t padding;
};
```

The **buffer** pointer contains the buffer array that the user has created. This is done in order to allow user to use static or dynamic memory allocation as he wishes. The **buffer_size** contains the length of the buffer. The **index** parameter contains the length of the frame that is ready to be transmitted or that has been received, it should not be used by the user since all the kiss functions use it. The **TXdelay** is the delay between receiving and transmitting and it is a number between 0 and 255, (which should be multiply by 10 so it is a delay that ranges between 0 and 2550ms) this value is used by the user and it is never used by the library. The **write** and **read** functions are the callback functions that the user must code in order to transmit and receive from whatever physical link (please keep in mind that it is not a multi-point protocol so you need another layer on top if you want to use kiss for multi-point links e.g. CAN bus). The **Status** variable contains the current status of the kiss instance and should not be modified by the user, only read to be sure in what state the kiss intance is in. The **context** pointer is an extra pointer that the user can use pointing at useful structures (e.g. in HAL you can use UART_HandleTypeDef). The **padding** parameter is the amount of FEND byte to send before the real frame (it is a number between 0 and 32).



Start by creating the instance of kiss
```C
kiss_instance_t kiss_i;
```

Then call the initialization function with all the necessary parameters
```C
int32_t kiss_init(kiss_instance_t *const kiss, uint8_t *const buffer, uint16_t buffer_size, 
                uint8_t TXdelay, uint32_t BaudRate, kiss_write_fn write, 
                kiss_read_fn read, void *const context, uint8_t padding);
```
Each kiss_instance_t use one buffer for input/output. This buffer allocation is done by the user which can select the right amount of bytes to allocate to it. You can use static allocation or dynamic allocation.
```C
const size_t len = 1024;
uint8_t buffer_kiss[len];
```
If you plan to transmit packets that are X long, you have to create a buffer which is X + 2 (FEND) + 1 (header) + X (if you want to use CRC32 you need also to take into account +4 bytes for CRC32 at the end of the packet). This takes into account the worst case scenario when you have to transmit only special characters. For instance, if you want to transmit 256 bytes  per packet, please use at least 515 bytes as buffer, but in the example above 1024 bytes have been used in order to be in safe zone. If you use static allocation and you have buffer overflow you can't change the amount of memory allocated without changing the program.


If you want to send data, use the encode function to encode the data previous to sending
```C
int32_t kiss_encode(kiss_instance_t *const kiss, const uint8_t *const data, 
                size_t length, uint8_t header);
```

If you want to add more data after you have used the kiss_encode function use this function here:
```C
int32_t kiss_push_encode(kiss_instance_t *const kiss, const uint8_t *const data, size_t length);
```

After you have received a frame use this function to decode the data 
```C
int32_t kiss_decode(kiss_instance_t *const kiss, uint8_t *const output, size_t output_max_size, 
                size_t *const output_length, uint8_t *const header);

```

After the data that you want to send has been encoded use this function to send it
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

To quickly encode and send or receive and decode use the following functions
```C
int32_t kiss_encode_and_send(kiss_instance_t *const kiss, const uint8_t *const data, 
                        size_t length, uint8_t header);
int32_t kiss_receive_and_decode(kiss_instance_t *const kiss, uint8_t *const output, size_t output_max_size,
                        size_t *const output_length, uint32_t maxAttempts, uint8_t *const header);
```

The following function extract the parameter ID (2 bytes) and parameter new value (**MAXIMUM 254 bytes**) from a set_param request:
```C
int32_t kiss_extract_param(kiss_instance_t *const kiss, uint16_t *const ID, uint8_t *const param, 
                        size_t max_param_size, size_t *const param_length)
```

The following functions encode and decode the data with four more bytes for CRC32 at the end of the frame.
```C
int32_t kiss_decode_crc32(kiss_instance_t *const kiss, uint8_t *const output, 
                    size_t *const output_length, uint8_t *const header);
int32_t kiss_encode_crc32(kiss_instance_t *const kiss, const uint8_t *const data,
                    size_t length, const uint8_t header);
int32_t kiss_encode_send_crc32(kiss_instance_t *const kiss, const uint8_t *const data, 
                    size_t length, uint8_t header)
int32_t kiss_set_param_crc32(kiss_instance_t *const kiss, uint16_t ID, const uint8_t *const param, 
                    size_t len);
int32_t kiss_request_param_crc32(kiss_instance_t *const kiss, uint16_t ID, uint8_t *const output, 
                    size_t max_out_size, size_t *const output_length, uint32_t maxAttempts,
                    uint8_t expected_header);
int32_t kiss_send_command_crc32(kiss_instance_t *const kiss, uint16_t *command);
```


Add the **KISS_DEBUG** directive to have access to the function which prints out the instance for debug
 ```C
void kiss_debug(kiss_instance_t *const kiss)
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
int32_t write_callback(kiss_instance_t *const kiss, const uint8_t *const data, size_t length)
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
int32_t read_callback(kiss_instance_t *const kiss, uint8_t *const buffer, size_t dataLen, size_t *const readBytes)
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
int32_t kiss_err = KISS_OK;
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
if(kiss_err != KISS_OK)
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
if(kiss_err != KISS_OK)
{
    /* error handling */
}

/* sending the frame */
kiss_err = kiss_send_frame(&my_kiss);
if(kiss_err != KISS_OK)
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

if(KISS_OK == kiss_err)
{
    /* packet have been received */

    /* decoding the message */
    kiss_err = kiss_decode(&my_kiss, rx_buffer, KISS_BUFFER_SIZE, &rx_len, &rx_header);

    if(kiss_err != KISS_OK)
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
else if(KISS_ERR_NO_DATA_RECEIVED == kiss_err)
{
    /* no data received */
}
else
{
    /* handling any other error */
}
```


# Use cases for satellite

Typically in Cubesats the OBC performs four actions to other devices:
1. Send a command
2. Set other device parameter
3. Request other device parameter
4. Send data to the other device (typically this is for large amount of data, such as sending data to the radio for downlink)

This library has easy functions to work with for these four actions.

## 1. Sending a command

With this library you can send a command to the other device with one single function, without or with CRC32 verification.

```C
int32_t kiss_send_command(kiss_instance_t *const kiss, uint16_t *command);
```
This first function takes has parameters a kiss instance and the 2 bytes command to send. It encodes the data and send it. The header used is **KISS_HEADER_COMMAND** which can be used to the other device to quickly search if the data arrived is a command.
For instance, if you want to turn off a channel for the Electrical Power Subsystem you can simply write:
```C
kiss_eps_err = kiss_send_command(&kiss_eps_i, EPS_CH1_TURN_OFF);
```

From the other end, the EPS will do something like this:

```C

kiss_obc_err = kiss_receive_frame(&kiss_obc_i, 1);

if(KISS_OK == kiss_obc_err)
{
    /* decoding the message */
    kiss_obc_err = kiss_decode(&kiss_obc_i, rx_buffer, KISS_BUFFER_SIZE, &rx_len, &rx_header);

    if(kiss_obc_err != KISS_OK)
    {
        /* error handling */
    }
    else
    {
        /* message has been received */
        if(KISS_HEADER_COMMAND == rx_header)
        {
            /* the command from array to uint16_t */
            uint16_t cmd = (uint16_t)rx_buffer[0] | ( (uint16_t)(rx_buffer[1]) << 8 );
            /* switch case with the command */
            switch(cmd)
            {
                ....
                case EPS_CH1_TURN_OFF:
                    ....
                    /* add a send_ack to inform the OBC that the command has been received and performed */
                    break;
                ....
            }
        }
    }
}

```

If you want to add CRC32 use the crc32 functions to add more safety if you expect a noisy channel with high bit error rate. (e.g. UART, I2C)

## 2. Set other device parameter

In this case we want to set a parameter of the other device. For example we would like to change the current limiter in the EPS for channel 3. The OBC has the following definitions:

- EPS_PARAM_CH1_MAX_CURRENT 4325

The function is the following:
```C
int32_t kiss_set_param(kiss_instance_t *const kiss, uint16_t ID, 
                    const uint8_t *const param, size_t len);
```

The implementation example is the following:

```C
/* current limiting in mA, from 6000 to 1000 because we suspect the device in CH1 has some failures and sometimes draws too much current and reset */
uint16_t curr_lim_ma = 1000;

/* create a byte array from the uint16_t variable */
uint8_t bs[2] = {(uint8_t)curr_lim_ma, (uint8_t)(curr_lim_ma >> 8)};
kiss_eps_err = kiss_set_param(&kiss_eps_i, EPS_PARAM_CH1_MAX_CURRENT, bs, 2);

/* Then you can maybe wait for an ack to know that the EPS has changed the parameter */
```

The header used for this echange is **KISS_HEADER_SET_PARAM**.

From the other end, the EPS will do something like this:

```C

kiss_obc_err = kiss_receive_frame(&kiss_obc_i, 1);

if(KISS_OK == kiss_obc_err)
{
    /* decoding the message */
    kiss_obc_err = kiss_decode(&kiss_obc_i, rx_buffer, KISS_BUFFER_SIZE, &rx_len, &rx_header);

    if(kiss_obc_err != KISS_OK)
    {
        /* error handling */
    }
    else
    {
        /* message has been received */
        if(KISS_HEADER_SET_PARAM == rx_header)
        {
           /* ID container */
           uint16_t ID = 0;
           /* the value of the parameter, use 64 just for safety, but it will be a value of 1,2,4,8 bytes */
           uint8_t value[64];
           /* value length */
           size_t value_len = 0;
           /* extract the parameter */
           kiss_obc_err = kiss_extract_param(&kiss_obc_i, &ID, value, 64, &value_len);

           if(KISS_OK == kiss_obc_err)
           {
                /* switch case for the type of parameter change */
                switch(ID)
                {
                    case CH1_CURR_LIM:
                        if(value_len == 2)
                        {
                            /* new current limiting value */
                            CH1_CUR_LIM = (uint16_t)(value[0]) | ((uint16_t)(value[1]) << 8);
                        }
                        else
                        {
                            /* handle the error */
                        }
                        break;
                }
           }

        }
    }
}

```

You can also use the CRC32 versions.

## 3. Request other device parameter


We request a parameter from the device, for instance a temperature or battery voltage. The following function is used:
```C
int32_t kiss_request_param(kiss_instance_t *const kiss, uint16_t ID, uint8_t *const output, 
                    size_t max_out_size, size_t *const output_length, uint32_t maxAttempts);
```

The implementation from the OBC that requests the battery voltage to the EPS is the following:
```C
uint8_t bs[2];
size_t out_len;
/* it requests the battery voltage which is a uint16_t and contains the battery voltage in mV */
kiss_eps_err = kiss_request_param(&kiss_eps_i, EPS_PARAM_BP_mV, bs, 2, &out_len, KISS_HEADER_DATA(0));
if(KISS_OK == kiss_eps_err)
{
    if(out_len == 2)
    {
        /* update the telemetry */
        EPS_BP_mV = (uint16_t)bs[0] || ((uint16_t)bs[1] << 8);
    }
    else
    {
        /* error management */
    }
}
```

The implementation from the EPS side will be:
```C

kiss_obc_err = kiss_receive_frame(&kiss_obc_i, 1);

if(KISS_OK == kiss_obc_err)
{
    /* decoding the message */
    kiss_obc_err = kiss_decode(&kiss_obc_i, rx_buffer, KISS_BUFFER_SIZE, &rx_len, &rx_header);

    if(kiss_obc_err != KISS_OK)
    {
        /* error handling */
    }
    else
    {
        /* message has been received */
        if(KISS_HEADER_REQUEST_PARAM == rx_header)
        {
           /* extract the parameter, but only the ID */
           kiss_obc_err = kiss_extract_param(&kiss_obc_i, &ID, NULL, 0, NULL);

           if(KISS_OK == kiss_obc_err)
           {
                /* switch case for the type of parameter change */
                switch(ID)
                {
                    case VBAT_mV:
                        /* the HEADER_DATA in port 0 is used to respond to request of parameters */
                        kiss_obc_err = kiss_encode_and_send(&kiss_obc_i, (uint8_t*)&TEL_VBAT_mV, 2, KISS_HEADER_DATA(0));
                        if(kiss_obc_err != KISS_OK)
                        {
                            /* error handling */
                        }
                        break;
                }
           }

        }
    }
}



```



You can also use the CRC32 versions.

## 4. Send data to the other device

Already looked at it, use the **KISS_HEADER_DATA** to know what type of data is arriving and where to store it or send it. The Most Significant Hex must be 0 to identify it is a data header, and the Least Significant Hex is a value from 0 to F (0-15) to identify where to store/send this data.