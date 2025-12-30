#ifndef KISSLIB_H
#define KISSLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#define KISSLIB_VERSION "2.0.0"



/** KISS protocol special byte values
 *
 * KISS frames are delimited by the FEND byte. If the payload contains
 * FEND or FESC they must be escaped using FESC followed by a transposed
 * value (TFEND/TFESC). The library implements simple escaping/unescaping.
 */
/* Frame End: marks the start/end of a KISS frame */
#define KISS_FEND 0xC0
/* Frame Escape: prefix used to escape special payload bytes */
#define KISS_FESC 0xDB
/* Transposed Frame End: replacement for FEND when escaped */
#define KISS_TFEND 0xDC
/* Transposed Frame Escape: replacement for FESC when escaped */
#define KISS_TFESC 0xDD


/** Error codes returned by library functions
 *
 * - KISS_ERR_INVALID_PARAMS: a NULL pointer or invalid size was supplied.
 * - KISS_ERR_INVALID_FRAME: an unexpected byte sequence or escape was found.
 * - KISS_ERR_BUFFER_OVERFLOW: an operation would exceed the provided buffer.
 */
#define KISS_ERR_INVALID_PARAMS 1
#define KISS_ERR_INVALID_FRAME 2
#define KISS_ERR_BUFFER_OVERFLOW 3
#define KISS_ERR_NO_DATA_RECEIVED 4
#define KISS_ERR_DATA_NOT_ENCODED 5
#define KISS_ERR_CRC32_MISMATCH 6
#define KISS_ERR_CALLBACK_MISSING 7

/**
 * KISS frame direction indicators
 * - KISS_NOTHING: no frame activity.
 * - KISS_TRANSMITTING: frame has been encoded, ready to transmit
 * - KISS_TRANSMITTED: frame has been sent.
 * - KISS_RECEIVING: frame is being received.
 * - KISS_RECEIVED: frame has been received.
 * - KISS_RECEIVED_ERROR: error occurred during frame reception.
 */
#define KISS_STATUS_NOTHING 0x00           // No frame activity
#define KISS_STATUS_TRANSMITTING 0x01      // Frame is ready to be transmitted
#define KISS_STATUS_TRANSMITTED 0x02       // Frame has been transmitted
#define KISS_STATUS_RECEIVING 0x03         // Frame is ready to be received
#define KISS_STATUS_RECEIVED 0x04          // Frame has been received
#define KISS_STATUS_RECEIVED_ERROR 0x05    // Frame received error
#define KISS_STATUS_ERROR_STATE 0x06      // General error state




/** KISS header byte values
 *
 * The KISS header byte follows the initial FEND in a frame and indicates
 * the type of frame (data or control) and the port number.
 * - KISS_HEADER_DATA: data frame (port 0) for normal payloads.
 * - KISS_HEADER_CONTROL_TX_DELAY: control frame to set TX delay. 0x10
 * - KISS_HEADER_CONTROL_SPEED: control frame to set baud rate. 0x60
 * - KISS_HEADER_PING: control frame for ping requests. 0x80
 * - KISS_HEADER_ACK: control frame for acknowledgments. 0xA0
 * - KISS_HEADER_NACK: control frame for negative acknowledgments. 0xC0
 * - Additional control frame types may be defined in the future.
 */
#define KISS_HEADER_DATA(port) ((port & 0x0F) << 4 | 0x00)
#define KISS_HEADER_TX_DELAY 0x10
#define KISS_HEADER_SPEED 0x60
#define KISS_HEADER_PING 0x80
#define KISS_HEADER_ACK 0xA0
#define KISS_HEADER_NACK 0xA5




typedef struct kiss_instance_t kiss_instance_t;



/** Transport callback: write a single byte to the physical transport.
 *
 * Implementations should block or buffer as appropriate for the platform.
 * Parameters:
 *  - kiss: kiss instance, inside the instnace there is the context variable for using specific physical layers
 *  - data: payload data array to be written
 *  - length: payload data array length to be written
 * Returns:
 *  - 0 if everything went good
 *  - Any other number for error
 */
typedef int (*kiss_write_fn)(kiss_instance_t *kiss, uint8_t *data, size_t length);

/** Transport callback: read `length` bytes into `data` from transport.
 *
 * The library calls this with small lengths (typically 1). Implementations
 * should attempt to return exactly `length` bytes (may block).
 * Parameters:
 *  - kiss: kiss instance, inside the instance there is the context variable for using specific physical layers
 *  - buffer: buffer array where the data arrived is written
 *  - dataLen: maximum data length of the buffer array
 *  - read: real number of bytes read in the buffer
 * Returns:
 *  - 0 if everything went good
 *  - Any other number for error
 */
typedef int (*kiss_read_fn)(kiss_instance_t *kiss, uint8_t *buffer, size_t dataLen, size_t *read);





/** KISS instance structure
 *
 * Fields:
 *  - buffer: user-provided working memory for encoding/decoding frames.
 *  - buffer_size: size of `buffer` in bytes.
 *  - index: current length of meaningful data in `buffer`.
 *  - TXdelay: transmit delay in milliseconds (10 to 2550).
 *  - speed: configured baud rate in bits per second.
 *  - write/read: user transport callbacks.
 *  - Status: current frame status (KISS_NOTHING, KISS_TRANSMITTING, etc).
 *  - context: context used in the write/read functions (for instance: context for UART, I2C, SPI, etc..)
 *
 * Notes:
 *  - The library uses the caller's buffer; it does not allocate memory.
 *  - `index` represents a length (number of bytes stored), not a stream
 *    file offset.
 */
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








/** 
 * kiss_crc32
 * -----------------------
 * Compute the CRC32 checksum for the given data.
 *
 * Parameters:
 *  - data: pointer to input data
 *  - len: length of input data in bytes
 *
 * Returns:
 *  - CRC32 checksum
 */
uint32_t kiss_crc32(kiss_instance_t *kiss, const uint8_t *data, size_t len);

/**
 * kiss_verify_crc32
 * -----------------------
 * Verify the CRC32 checksum for the given data.
 *
 * Parameters:
 *  - data: pointer to input data
 *  - len: length of input data in bytes
 *  - expected_crc: expected CRC32 checksum
 *
 * Returns:
 *  - 1 if checksum matches, 0 otherwise
 */
int kiss_verify_crc32(kiss_instance_t *kiss, const uint8_t *data, size_t len, uint32_t expected_crc);













/** Initialize a `kiss_instance_t`.
 *
 * Parameters:
 *  - kiss: pointer to an instance structure to initialize.
 *  - buffer: caller-provided working buffer (must remain valid).
 *  - buffer_size: size of `buffer` in bytes.
 *  - TXdelay: transmit delay in milliseconds (10 to 2550).
 *  - write: transport write callback.
 *  - read: transport read callback.
 *  - context: user-defined context passed to read/write callbacks.
 *
 * Returns: 0 on success or a KISS_ERR_* code on failure.
 */
int kiss_init(kiss_instance_t *kiss, uint8_t *buffer, size_t buffer_size, uint8_t TXdelay, kiss_write_fn write, kiss_read_fn read, void* context, uint8_t padding);


/** Encode `length` bytes from `data` into the instance working buffer.
 *
 * Behavior:
 *  - Writes: FEND, header (0x00), escaped payload, FEND into `kiss->buffer`.
 *  - Updates `kiss->index` to the encoded size.
 *
 * Parameters:
 *  - kiss: initialized instance.
 *  - data: payload to encode.
 *  - length: payload length in bytes.
 *  - header: KISS header byte to use.
 *
 * Returns: 0 on success, or an error code (invalid params or buffer overflow).
 */
int kiss_encode(kiss_instance_t *kiss, uint8_t *data, size_t length, const uint8_t header);



/**
 * kiss_push_encode
 * ------------------
 * push more data inside an already encoded payload
 * Parameters:
 * - kiss: kiss instance
 * - data: data to add at the end of the payload 
 * - length: size of the data to add, if all are added the value is not changed.
 * Returns:
 * - 0, if everything is ok
 * - Any other number from KISS_ERR_xxx if an error occoured
 */
int kiss_push_encode(kiss_instance_t *kiss, uint8_t *data, size_t length);



/** Decode a frame stored in `kiss->buffer` into `output`.
 *
 * Parameters:
 *  - kiss: instance containing an encoded frame and `kiss->index` set.
 *  - output: buffer to receive decoded payload bytes.
 *  - output_max_size: maximum size of the output buffer.
 *  - output_length: pointer to receive number of decoded bytes.
 *  - header: optional pointer to receive the KISS header byte (may be NULL).
 *
 * Returns: 0 on success or a KISS_ERR_* code on failure.
 */
int kiss_decode(kiss_instance_t *kiss, uint8_t *output, size_t output_max_size, size_t *output_length, uint8_t *header);


/** Send an encoded frame over the transport using the `write` callback.
 *
 * The function sends `kiss->index` bytes from `kiss->buffer`. The buffer
 * must already contain an encoded frame (e.g. created by `kiss_encode`).
 *
 * Returns: 
 * - 0 on success 
 * - KISS_ERR_INVALID_PARAMS if inputs are invalid
 * - generic error code from transport write function on failure
 */
int kiss_send_frame(kiss_instance_t *kiss);





/**
 * kiss_encode_and_send
 * ----------------------
 * Encode `length` bytes from `data` into the instance working buffer and send it.
 * * Behavior:
 * - Calls kiss_encode to encode the data into kiss->buffer.
 * - Calls kiss_send_frame to send the encoded frame.
 * Parameters:
 * - kiss: initialized instance.
 * - data: payload to encode.
 * - length: payload length in bytes.
 * - header: KISS header byte to use.
 * Returns: 
 * - 0 on success
 * - KISS_ERR_INVALID_PARAMS for bad inputs
 * - KISS_ERR_BUFFER_OVERFLOW if the provided working buffer is too small
 * - generic error code from kiss_send_frame on failure
 */
int kiss_encode_and_send(kiss_instance_t *kiss, uint8_t *data, size_t length, uint8_t header);







/** Receive bytes from transport until a full KISS frame is assembled and
 * decode them into `output`.
 *
 * Parameters:
 *  - kiss: instance with working buffer and `read` callback.
 *  - maxAttempts: maximum number of attempts of reading bytes
 * Behavior:
 *  - Calls the `read` callback repeatedly to read bytes.
 *  - Assembles bytes into `kiss->buffer` until a full frame is received.
 *
 * Returns: 
 * - 0 on success 
 * - KISS_ERR_INVALID_PARAMS for bad inputs
 * - KISS_ERR_INVALID_FRAME for bad escape sequences
 * - KISS_ERR_BUFFER_OVERFLOW if decoded data exceeds `kiss->buffer_size`
 * - KISS_ERR_NO_DATA_RECEIVED if no complete frame is received within maxAttempts
 * - generic error code from transport read function on failure
 */
int kiss_receive_frame(kiss_instance_t *kiss, uint32_t maxAttempts);



/** 
 * kiss_receive_and_decode
 * -----------------------
 * Receive a KISS frame and decode it into `output`.
 *
 * Parameters:
 *  - kiss: instance with working buffer and `read` callback.
 *  - output: buffer to receive decoded payload bytes.
 *  - output_max_size: maximum size of the output buffer
 *  - output_length: pointer to receive number of decoded bytes.
 *  - maxAttempts: maximum number of read attempts before giving up.
 *  - header: optional pointer to receive the KISS header byte (may be NULL).
 *
 * Returns:
 * - 0 on success
 * - KISS_ERR_INVALID_PARAMS for bad inputs
 * - KISS_ERR_INVALID_FRAME for bad escape sequences
 * - KISS_ERR_BUFFER_OVERFLOW if decoded data exceeds `kiss->buffer_size`
 * - KISS_ERR_NO_DATA_RECEIVED if no complete frame is received within maxAttempts
 * - generic error code from transport read function on failure
 */
int kiss_receive_and_decode(kiss_instance_t *kiss, uint8_t *output, size_t output_max_size, size_t *output_length, uint32_t maxAttempts, uint8_t *header);





/**
 * kiss_set_TXdelay
 * -----------------
 * Set the TX delay on the KISS device by sending a control frame.
 * The delay is specified in milliseconds (10ms to 2550ms).
 * Parameters:
 * - kiss: initialized instance.
 * - tx_delay: delay in milliseconds (10 to 2550).
 * Returns:
 * - 0 on success
 * - KISS_ERR_INVALID_PARAMS if inputs are invalid
 * - generic error code from kiss_send_frame on failure
 */
int kiss_set_TXdelay(kiss_instance_t *kiss,  uint8_t tx_delay);

/** 
 * kiss_set_speed
 * -----------------   
 * Set the speed on the KISS device by sending a control frame.
 * The speed is specified in bits per second.
 * Parameters:
 * - kiss: initialized instance.
 * - BaudRate: speed in bits per second.
 * Returns:
 * - 0 on success
 * - KISS_ERR_INVALID_PARAMS if inputs are invalid
 * - generic error code from kiss_send_frame on failure
 */
int kiss_set_speed(kiss_instance_t *kiss, uint32_t BaudRate);



/**
 * kiss_send_ack
 * -----------------   
 * Send an ACK control frame.
 * Parameters:
 * - kiss: initialized instance.
 * Returns:
 * - 0 on success
 * - KISS_ERR_INVALID_PARAMS if inputs are invalid
 * - generic error code from kiss_send_frame on failure
 */
int kiss_send_ack(kiss_instance_t *kiss);


/**
 * kiss_send_nack
 * -----------------   
 * Send a NACK control frame.
 * Parameters:
 * - kiss: initialized instance.
 * Returns:
 * - 0 on success
 * - KISS_ERR_INVALID_PARAMS if inputs are invalid
 * - generic error code from kiss_send_frame on failure
 */
int kiss_send_nack(kiss_instance_t *kiss);




/** 
 * kiss_send_ping
 * -----------------
 * Send a PING control frame. The device that has been pinged must respond with an ACK
 * Parameters:
 * - kiss: initialized instance.
 * Returns:
 * - 0 on success
 * - KISS_ERR_INVALID_PARAMS if inputs are invalid
 * - generic error code from kiss_send_frame on failure
 */
int kiss_send_ping(kiss_instance_t *kiss);



/**
 * kiss_encode_crc32
 * -----------------------
 * Encode `length` bytes from `data` into the instance working buffer with CRC32.
 * * Behavior:
 *  - Writes: FEND, header (0x00), escaped payload, CRC32, FEND into `kiss->buffer`.
 *  - Updates `kiss->index` to the encoded size.
 * Parameters:
 * - kiss: initialized instance.
 * - data: payload to encode.
 * - length: payload length in bytes. the variable is updated with the real length written in the buffer
 * - header: KISS header byte to use.
 * Returns: 0 on success, or an error code (invalid params or buffer overflow).
 */
int kiss_encode_crc32(kiss_instance_t *kiss, uint8_t *data, size_t length, uint8_t header);





/**
 * kiss_encode_send_crc32
 * -------------------------
 * Encode the data, put CRC32 at the end of the frame and then send it
 * Behavior:
 * - Encode the data and put CRC32 of the data at the end of the frame.
 * - Write the frame in the link
 * Parameters:
 * - kiss: initialization instance
 * - data: data payload array to encapsulate, encode and send
 * - length: length of the payload array to send, the length is changed with the real bytes that have been sent
 * - header: header byte
 * Returns:
 * 0 on success, or an error code not equal to zero
 */
int kiss_encode_send_crc32(kiss_instance_t *kiss, uint8_t *data, size_t length, uint8_t header);


/** 
 * kiss_decode_crc32
 * -----------------------
 * Decode an encoded frame that is present in `kiss->buffer`.
 * The caller must set `kiss->index` to the number of bytes in the buffer
 * that belong to the encoded frame prior to calling this function.
 *
 * Parameters:
 *  - kiss: instance containing encoded frame data
 *  - output: buffer to receive decoded payload
 *  - max_out_size: is the maximum output buffer size
 *  - output_length: pointer to variable that will receive decoded length
 *  - header: optional pointer to receive the KISS header byte (may be NULL)
 *
 * Returns:
 *  - 0 on success
 *  - KISS_ERR_INVALID_PARAMS for bad pointers
 *  - KISS_ERR_INVALID_FRAME for malformed frames or bad escape sequences
 */
int kiss_decode_crc32(kiss_instance_t *kiss, uint8_t *output, size_t max_out_size, size_t *output_length, uint8_t *header);





#ifdef KISS_DEBUG

/* if the debug is active use this function to plot the kiss instance */
void kiss_debug(kiss_instance_t *kiss);


#endif




#ifdef __cplusplus
}
#endif

#endif /* KISSLIB_H */
