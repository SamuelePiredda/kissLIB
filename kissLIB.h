#ifndef KISSLIB_H
#define KISSLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define KISSLIB_VERSION "1.0.0"

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


/** Transport callback: write a single byte to the physical transport.
 *
 * Implementations should block or buffer as appropriate for the platform.
 * Parameters:
 *  - byte: data byte to send.
 */
typedef void (*kiss_write_fn)(uint8_t byte);

/** Transport callback: read `length` bytes into `data` from transport.
 *
 * The library calls this with small lengths (typically 1). Implementations
 * should attempt to return exactly `length` bytes (may block).
 * Parameters:
 *  - data: destination buffer.
 *  - length: number of bytes requested.
 */
typedef void (*kiss_read_fn)(uint8_t *data, uint16_t length);


/** KISS instance structure
 *
 * Fields:
 *  - buffer: user-provided working memory for encoding/decoding frames.
 *  - buffer_size: size of `buffer` in bytes.
 *  - index: current length of meaningful data in `buffer`.
 *  - write/read: user transport callbacks.
 *
 * Notes:
 *  - The library uses the caller's buffer; it does not allocate memory.
 *  - `index` represents a length (number of bytes stored), not a stream
 *    file offset.
 */
typedef struct 
{
    uint8_t *buffer;
    uint16_t buffer_size;
    uint16_t index;
    kiss_write_fn write;
    kiss_read_fn read;
} kiss_instance_t;


/** Initialize a `kiss_instance_t`.
 *
 * Parameters:
 *  - kiss: pointer to an instance structure to initialize.
 *  - buffer: caller-provided working buffer (must remain valid).
 *  - buffer_size: size of `buffer` in bytes.
 *  - write: transport write callback.
 *  - read: transport read callback.
 *
 * Returns: 0 on success or a KISS_ERR_* code on failure.
 */
int kiss_init(kiss_instance_t *kiss, uint8_t *buffer, uint16_t buffer_size, kiss_write_fn write, kiss_read_fn read);


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
 *
 * Returns: 0 on success, or an error code (invalid params or buffer overflow).
 */
int kiss_encode(kiss_instance_t *kiss, const uint8_t *data, uint16_t length);


/** Decode a frame stored in `kiss->buffer` into `output`.
 *
 * Parameters:
 *  - kiss: instance containing an encoded frame and `kiss->index` set.
 *  - output: buffer to receive decoded payload bytes.
 *  - output_length: pointer to receive number of decoded bytes.
 *  - header: optional pointer to receive the KISS header byte (may be NULL).
 *
 * Returns: 0 on success or a KISS_ERR_* code on failure.
 */
int kiss_decode(kiss_instance_t *kiss, uint8_t *output, uint16_t *output_length, uint8_t *header);


/** Send an encoded frame over the transport using the `write` callback.
 *
 * The function sends `kiss->index` bytes from `kiss->buffer`. The buffer
 * must already contain an encoded frame (e.g. created by `kiss_encode`).
 *
 * Returns: 0 on success or KISS_ERR_INVALID_PARAMS.
 */
int kiss_send_frame(kiss_instance_t *kiss);


/** Receive bytes from transport until a full KISS frame is assembled and
 * decode them into `output`.
 *
 * Parameters:
 *  - kiss: instance with working buffer and `read` callback.
 *  - output: buffer to receive decoded payload bytes.
 *  - output_length: pointer to receive number of decoded bytes.
 *
 * Behavior:
 *  - Calls `read(&byte,1)` repeatedly until a terminating FEND is seen
 *    after payload bytes were collected.
 *  - Un-escapes TFEND/TFESC sequences into original bytes.
 *
 * Returns: 0 on success or a KISS_ERR_* on error.
 */
int kiss_receive_frame(kiss_instance_t *kiss, uint32_t maxAttempts);










#ifdef __cplusplus
}
#endif

#endif /* KISSLIB_H */
