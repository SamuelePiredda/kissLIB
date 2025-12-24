# kissLIB
small and portable KISS library for C that can be used for embedded.



These are the two callback functions that the user must implement for writing and receiving, for whataver physical layer (I2C, UART etc..)
``` C
typedef void (*kiss_write_fn)(kiss_instance_t *kiss, uint8_t *data, size_t length);
typedef void (*kiss_read_fn)(kiss_instance_t *kiss, uint8_t *buffer, size_t dataLen, size_t *read);
```


This is the struct containing the instance of the kiss communication protocol: the buffer array; buffer size; the current index or current amount of valid data in the buffer; the transmission delay after receiving; the baud rate if needed; write/read callback functions; the current status of the link; context pointer with all the information about the physical layer.
```C
typedef struct 
{
    uint8_t *buffer;
    uint16_t buffer_size;
    size_t index;
    uint8_t TXdelay;
    uint32_t speed;
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
int kiss_init(kiss_instance_t *kiss, uint8_t *buffer, uint16_t buffer_size, uint8_t TXdelay, uint32_t BaudRate, kiss_write_fn write, kiss_read_fn read, void *context);
```

If you want to send data, use the encode function to encode the data previous to sending
```C
int kiss_encode(kiss_instance_t *kiss, const uint8_t *data, uint16_t length, const uint8_t header);
```

After you have received a frame use this function to decode the data 
```C
int kiss_decode(kiss_instance_t *kiss, uint8_t *output, uint16_t *output_length, uint8_t *header);
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
int kiss_encode_and_send(kiss_instance_t *kiss, const uint8_t *data, uint16_t length, const uint8_t header)
int kiss_receive_and_decode(kiss_instance_t *kiss, uint8_t *output, uint16_t *output_length, uint32_t maxAttempts, uint8_t *header)
```


