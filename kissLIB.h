#ifndef KISSLIB_H
#define KISSLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define KISSLIB_VERSION "2.0.0"


/*
* This lib is the most generic possible but it tries to reduce the RAM usage for Arduino.
* Author: Samuele Piredda
* trying to be compliant to MISRA-C 2012
*/





#define KISS_LSB(x) ( (uint8_t)(x) )
#define KISS_MSB(x) ( (uint8_t)((x) >> 8) )
#define KISS_BYTE_TO_UINT16(b0, b1) ( (uint16_t)(b0) | ((uint16_t)(b1) << 8) )  
#define KISS_BYTE_TO_UINT32(b0, b1, b2, b3) ( (uint32_t)(b0) | ((uint32_t)(b1) << 8) | ((uint32_t)(b2) << 16) | ((uint32_t)(b3) << 24))


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
#define KISS_ERR_HEADER_ESCAPE 8
#define KISS_ERR_STATUS 9
#define KISS_ERR_PADDING_OVERFLOW 10

#define KISS_OK 0   

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
 * - KISS_HEADER_REQUEST_PARAM: control frame to request a parameter. 0x40
 * - KISS_HEADER_SET_PARAM: control frame to set a parameter. 0x50
 * - KISS_HEADER_COMMAND: control frame to send a command. 0x70
 * - Additional control frame types may be defined in the future.
 */
#define KISS_HEADER_DATA(port) ((uint8_t)(port & 0x0F))
#define KISS_HEADER_TX_DELAY 0x10
#define KISS_HEADER_SPEED 0x60
#define KISS_HEADER_PING 0x80
#define KISS_HEADER_ACK 0xA0
#define KISS_HEADER_NACK 0xA5
#define KISS_HEADER_REQUEST_PARAM 0x40
#define KISS_HEADER_SET_PARAM 0x50
#define KISS_HEADER_COMMAND 0x70






#define KISS_MAX_PADDING 32





typedef struct kiss_instance_t kiss_instance_t;



/** 
 * @brief Implementations should block or buffer as appropriate for the platform.
 *  @param kiss kiss instance, inside the instnace there is the context variable for using specific physical layers
 *  @param data payload data array to be written
 *  @param length payload data array length to be written
 *  @retval KISS_OK(0) if everything went good
 *  @retval Any other number for error
 */
typedef int32_t (*kiss_write_fn)(kiss_instance_t *const kiss, const uint8_t *const data, size_t length);

/**
 * @brief The library calls this with small lengths (typically 1). Implementations should attempt to return exactly `length` bytes (may block).
 *  @param kiss kiss instance, inside the instance there is the context variable for using specific physical layers
 *  @param buffer buffer array where the data arrived is written
 *  @param dataLen maximum data length of the buffer array
 *  @param read real number of bytes read in the buffer
 *  @retval KISS_OK(0) if everything went good
 *  @retval Any other number for error
 */
typedef int32_t (*kiss_read_fn)(kiss_instance_t *const kiss, uint8_t *const buffer, size_t dataLen, size_t *const read);



/**
 * @brief this structure contains the entire kiss instance that has been created for each link
 */
struct kiss_instance_t {
    uint8_t *buffer; /**< user-provided working memory for encoding/decoding frames.  */
    size_t buffer_size; /**< size of `buffer` in bytes. */
    size_t index; /**< current length of meaningful data in `buffer`. */
    uint8_t TXdelay; /**<  transmit delay in uints 0-255 -> from 10 to 2550 ms.  */
    kiss_write_fn write; /**< user transport write callback */
    kiss_read_fn read; /**< user transport read callback */
    uint8_t Status; /**< current frame status (KISS_NOTHING, KISS_TRANSMITTING, etc). */
    void *context; /**< context used in the write/read functions (for instance: context for UART, I2C, SPI, etc..) */
    uint8_t padding; /**< padding number is the number of FEND bytes to write before actually starting sending the frame. Typically used for synch */
};








/** 

 * @brief Compute the CRC32 checksum for the given data.
 *  @param kiss kiss instance
 *  @param data pointer to input data
 *  @param len length of input data in bytes
 * @returns CRC32 checksum
 */
uint32_t kiss_crc32(kiss_instance_t *const kiss, const uint8_t *const data, size_t len);



/**
* @brief Compute CRC32 from a previous crc32 computed, it will returned the crc32 without final XOR so you must compute the XOR in the return.
* @param kiss kiss instance
* @param prev_crc previously calculated CRC32 (no final XOR)
* @param data more data to compute CRC32
* @param len length of the data
* @returns returns the CRC32 calculated
*/
uint32_t kiss_crc32_push(kiss_instance_t *const kiss, uint32_t prev_crc, const uint8_t *const data, size_t len);


/**
 * @brief Verify the CRC32 checksum for the given data.
 * @param kiss kiss instance
 * @param data data where check is needed   
 * @param len length of the data
 * @param expected_crc expected CRC32 of the data
 *  @retval 1 if checksum matches
 *  @retval 0 otherwise
 */
int32_t kiss_verify_crc32(kiss_instance_t *const kiss, const uint8_t *const data, size_t len, uint32_t expected_crc);













/** 
 * @brief Initialize a kiss_instance_t.
 *  @param kiss pointer to an instance structure to initialize.
 *  @param buffer caller-provided working buffer (must remain valid).
 *  @param buffer_size size of `buffer` in bytes.
 *  @param TXdelay transmit delay in milliseconds (10 to 2550).
 *  @param write transport write callback.
 *  @param read transport read callback.
 *  @param context user-defined context passed to read/write callbacks.
* @return Any number of errors or KISS_OK(0) if everything went ok
 */
int32_t kiss_init(kiss_instance_t *const kiss, uint8_t *const buffer, size_t buffer_size, uint8_t TXdelay, kiss_write_fn write, kiss_read_fn read, void *const context, uint8_t padding);


/** 
 * @brief Encode `length` bytes from `data` into the instance working buffer.
 *  @param kiss initialized instance.
 *  @param data payload to encode.
 *  @param length payload length in bytes.
 *  @param header KISS header byte to use.
* @return Any number of errors or KISS_OK(0) if everything went ok
 */
int32_t kiss_encode(kiss_instance_t *const kiss, const uint8_t *const data, size_t length, uint8_t header);



/**
 * @brief push more data inside an already encoded payload
 * @param kiss kiss instance
 * @param data data to add at the end of the payload 
 * @param length size of the data to add, if all are added the value is not changed.
* @return Any number of errors or KISS_OK(0) if everything went ok
 */
int32_t kiss_push_encode(kiss_instance_t *const kiss, const uint8_t *const data, size_t length);



/** 
 * @brief Decode a frame stored in `kiss->buffer` into `output`.
*  @param kiss instance containing an encoded frame and `kiss->index` set.
*  @param output buffer to receive decoded payload bytes.
*  @param output_max_size maximum size of the output buffer.
*  @param output_length pointer to receive number of decoded bytes.
*  @param header optional pointer to receive the KISS header byte (may be NULL).
* @return Any number of errors or KISS_OK(0) if everything went ok
*/
int32_t kiss_decode(kiss_instance_t *const kiss, uint8_t *const output, size_t output_max_size, size_t *const output_length, uint8_t *const header);


/** 
* @brief Send an encoded frame over the transport using the `write` callback.
* @retval KISS_OK(0) on success 
* @retval KISS_ERR_INVALID_PARAMS if inputs are invalid
* @retval generic error code from transport write function on failure
*/
int32_t kiss_send_frame(kiss_instance_t *const kiss);





/**
* @brief Encode `length` bytes from `data` into the instance working buffer and send it.
* @param kiss initialized instance.
* @param data payload to encode.
* @param length payload length in bytes.
* @param header KISS header byte to use.
* @retval KISS_OK(0) on success
* @retval KISS_ERR_INVALID_PARAMS for bad inputs
* @retval KISS_ERR_BUFFER_OVERFLOW if the provided working buffer is too small
* @retval generic error code from kiss_send_frame on failure
*/
int32_t kiss_encode_and_send(kiss_instance_t *const kiss, const uint8_t *const data, size_t length, uint8_t header);







/** 
* @brief Receive bytes from transport until a full KISS frame is assembled and decode them into `output`.
*  @param kiss instance with working buffer and `read` callback.
*  @param maxAttempts maximum number of attempts of reading bytes
* @returns an error or KISS_OK(0) if everything went ok
* @retval KISS_ERR_INVALID_PARAMS for bad inputs
* @retval KISS_ERR_INVALID_FRAME for bad escape sequences
* @retval KISS_ERR_BUFFER_OVERFLOW if decoded data exceeds `kiss->buffer_size`
* @retval KISS_ERR_NO_DATA_RECEIVED if no complete frame is received within maxAttempts
*/
int32_t kiss_receive_frame(kiss_instance_t *const kiss, uint32_t maxAttempts);



/** 
* @brief Receive a KISS frame and decode it into `output`.
*  @param kiss instance with working buffer and `read` callback.
*  @param output buffer to receive decoded payload bytes.
*  @param output_max_size maximum size of the output buffer
*  @param output_length pointer to receive number of decoded bytes.
*  @param maxAttempts maximum number of read attempts before giving up.
*  @param header optional pointer to receive the KISS header byte (may be NULL).
* @returns an error or KISS_OK(0) if everything went ok
* @retval 0 on success
* @retval KISS_ERR_INVALID_PARAMS for bad inputs
* @retval KISS_ERR_INVALID_FRAME for bad escape sequences
* @retval KISS_ERR_BUFFER_OVERFLOW if decoded data exceeds `kiss->buffer_size`
* @retval KISS_ERR_NO_DATA_RECEIVED if no complete frame is received within maxAttempts
* @retval generic error code from transport read function on failure
*/
int32_t kiss_receive_and_decode(kiss_instance_t *const kiss, uint8_t *const output, size_t output_max_size, size_t *const output_length, uint32_t maxAttempts, uint8_t *const header);





/**
* @brief Set the TX delay on the KISS device by sending a control frame. The delay is specified in milliseconds (10ms to 2550ms).
* @param kiss initialized instance.
* @param tx_delay delay in milliseconds (10 to 2550).
* @return Any number of errors or KISS_OK(0) if everything went ok
*/
int32_t kiss_set_TXdelay(kiss_instance_t *const kiss,  uint8_t tx_delay);

/** 
* @brief Set the speed on the KISS device by sending a control frame. The speed is specified in bits per second.
* @param kiss initialized instance.
* @param BaudRate speed in bits per second.
* @return Any number of errors or KISS_OK(0) if everything went ok
*/
int32_t kiss_set_speed(kiss_instance_t *const kiss, uint32_t BaudRate);



/**
* @brief Send an ACK control frame.
* @param kiss: initialized instance.
* @return Any number of errors or KISS_OK(0) if everything went ok
*/
int32_t kiss_send_ack(kiss_instance_t *const kiss);


/**
* @brief Send a NACK control frame.
* @param kiss initialized instance.
* @return Any number of errors or KISS_OK(0) if everything went ok
*/
int32_t kiss_send_nack(kiss_instance_t *const kiss);




/** 
* @brief Send a PING control frame. The device that has been pinged must respond with an ACK.
* @param kiss initialized instance.
* @return Any number of errors or KISS_OK(0) if everything went ok
*/
int32_t kiss_send_ping(kiss_instance_t *const kiss);




/**
* @brief Send a parameter to the other device with specific header (not 0)
* @param kiss initialized instance
* @param ID 2 bytes for the ID of the param
* @param param byte array witht the parameter to send
* @param len number of bytes to send
* @return Any number of errors or KISS_OK(0) if everything went ok
*/
int32_t kiss_set_param(kiss_instance_t *const kiss, uint16_t ID, const uint8_t *const param, size_t len);


/**
* @brief Send a parameter to the other device with specific header (not 0) with CRC32
* @param kiss initialized instance
* @param ID 2 bytes for the ID of the param
* @param param byte array witht the parameter to send
* @param len number of bytes to send
* @param header header to set. 00 is a generic data packet, you can implement a specific header like 0x05 (which means it contains a parameter)
* @return Any number of errors or KISS_OK(0) if everything went ok
*/
int32_t kiss_set_param_crc32(kiss_instance_t *const kiss, uint16_t ID, const uint8_t *const param, size_t len);




/**
 * @brief Send a parameter request to the other device requesting for a parameter.
 * @param kiss: initialized instance
 * @param ID: 2 bytes for the ID of the param to request
 * @returns: Any number of errors or KISS_OK(0) if everything went ok
 */
int32_t kiss_request_param(kiss_instance_t *const kiss, uint16_t ID);



/**
 * @brief Send a parameter request to the other device with CRC32 and wait for the response with CRC32 verification.
 * @param kiss: initialized instance
 * @param ID: 2 bytes for the ID of the param to request
 * @param header: header byte to use for the request
 * @param output: buffer to receive the parameter value
 * @param max_out_size: maximum size of the output buffer
 * @param output_length: pointer to receive the actual length of the output data
 * @param maxAttempts: maximum number of attempts to wait for the response
 * @param expected_header: expected header byte in the response frame
 * @returns: Any number of errors or KISS_OK(0) if everything went ok
 */
int32_t kiss_request_param_crc32(kiss_instance_t *const kiss, uint16_t ID, uint8_t *const output, size_t max_out_size, size_t *const output_length, uint32_t maxAttempts, uint8_t expected_header);



/**
 * @brief Send a command to the other device. The command is a 2 bytes value.
 * @param kiss: initialized instance
 * @param command: pointer to the 2 bytes command to send
 * @returns: Any number of errors or KISS_OK(0) if everything went ok
 */
int32_t kiss_send_command(kiss_instance_t *const kiss, uint16_t command);


/**
 * @brief Send a command to the other device with CRC32. The command is a 2 bytes value.
 * @param kiss: initialized instance
 * @param command: pointer to the 2 bytes command to send
 * @returns: Any number of errors or KISS_OK(0) if everything went ok
 */
int32_t kiss_send_command_crc32(kiss_instance_t *const kiss, uint16_t *command);





/**
 * @brief Encode `length` bytes from `data` into the instance working buffer with CRC32.
 * @param kiss initialized instance.
 * @param data payload to encode.
 * @param length payload length in bytes. the variable is updated with the real length written in the buffer
 * @param header KISS header byte to use.
 * @return Any number of errors or KISS_OK(0) if everything went ok
 */
int32_t kiss_encode_crc32(kiss_instance_t *const kiss, const uint8_t *const data, size_t length, uint8_t header);





/**
 * @brief Encode the data, put CRC32 at the end of the frame and then send it
 * @param kiss initialization instance
 * @param data data payload array to encapsulate, encode and send
 * @param length length of the payload array to send, the length is changed with the real bytes that have been sent
 * @param header header byte
 * @return Any number of errors or KISS_OK(0) if everything went ok
 */
int32_t kiss_encode_send_crc32(kiss_instance_t *const kiss, const uint8_t *const data, size_t length, uint8_t header);


/** 
 * @brief Decode an encoded frame that is present in `kiss->buffer`. The caller must set `kiss->index` to the number of bytes in the buffer that belong to the encoded frame prior to calling this function.
 *  @param kiss instance containing encoded frame data
 *  @param output buffer to receive decoded payload
 *  @param max_out_size is the maximum output buffer size
 *  @param output_length pointer to variable that will receive decoded length
 *  @param header optional pointer to receive the KISS header byte (may be NULL)
 * @return Any number of errors or KISS_OK(0) if everything went ok
 */
int32_t kiss_decode_crc32(kiss_instance_t *const kiss, uint8_t *const output, size_t max_out_size, size_t *const output_length, uint8_t *const header);



/**
 * @brief If the header of the fram is a set parameter header, this function extracts the parameter ID and value from the frame.
 * @param kiss: instance containing encoded frame data
 * @param ID: pointer to variable that will receive the parameter ID (2 bytes)  
 * @param param: buffer to receive the parameter value
 * @param max_param_size: maximum size of the param buffer
 * @param param_length: pointer to variable that will receive the actual length of the parameter value
  * @returns: Any number of errors or KISS_OK(0) if everything went ok
 */
int32_t kiss_extract_param(kiss_instance_t *const kiss, uint16_t *const ID, uint8_t *const param, size_t max_param_size, size_t *const param_length);




#ifdef KISS_DEBUG

/* if the debug is active use this function to plot the kiss instance */
void kiss_debug(kiss_instance_t *const kiss);


#endif




#ifdef __cplusplus
}
#endif

#endif /* KISSLIB_H */
