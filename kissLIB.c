#include "kissLIB.h"

/* Implementation of a minimal, well-documented KISS framing library.
 *
+* File-level notes:
 * - This library is intentionally small and depends on the caller to
 *   provide I/O callbacks and working memory. It does not allocate memory.
 * - Users should ensure their `read` callback behavior matches expectations
 *   (blocking vs non-blocking) since `kiss_receive_frame` calls `read` in a
 *   loop until a frame terminator is seen.
 */

/**
 * kiss_init
 * ----------
 * Initialize a `kiss_instance_t` with the provided buffer and callbacks.
 *
 * Parameters:
 *  - kiss: Pointer to the instance to initialize. Must not be NULL.
 *  - buffer: Caller-supplied working buffer used for encoding/decoding.
 *  - buffer_size: Size of `buffer` in bytes (must be > 0).
 *  - write: Transport write callback used by `kiss_send_frame`.
 *  - read: Transport read callback used by `kiss_receive_frame`.
 *
 * Returns:
 *  - 0 on success
 *  - KISS_ERR_INVALID_PARAMS if inputs are invalid
 */
int kiss_init(kiss_instance_t *kiss, uint8_t *buffer, uint16_t buffer_size, kiss_write_fn write, kiss_read_fn read)
{
    if (kiss == NULL || buffer == NULL || buffer_size == 0 || write == NULL || read == NULL)
        return KISS_ERR_INVALID_PARAMS;

    kiss->buffer = buffer;
    kiss->buffer_size = buffer_size;
    kiss->index = 0;
    kiss->write = write;
    kiss->read = read;

    return 0;
}


/**
 * kiss_encode
 * -----------
 * Encode application payload into a KISS frame stored in `kiss->buffer`.
 * The encoded format is:
 *   FEND | header(1 byte) | escaped payload | FEND
 *
 * Escaping rules (per this implementation):
 *   - FEND -> FESC TFEND
 *   - FESC -> FESC TFESC
 *
 * Parameters:
 *  - kiss: initialized instance
 *  - data: pointer to payload bytes
 *  - length: payload length in bytes
 *
 * Returns:
 *  - 0 on success
 *  - KISS_ERR_INVALID_PARAMS for bad inputs
 *  - KISS_ERR_BUFFER_OVERFLOW if the provided working buffer is too small
 */
int kiss_encode(kiss_instance_t *kiss, const uint8_t *data, uint16_t length)
{
    if (kiss == NULL || data == NULL || length == 0) return KISS_ERR_INVALID_PARAMS;
    /* Worst-case expansion: every byte could become two bytes when escaped,
       plus two FEND bytes and one header byte. */
    if ((uint32_t)length * 2 + 3  > kiss->buffer_size) return KISS_ERR_BUFFER_OVERFLOW;

    /* Start of frame */
    kiss->buffer[0] = KISS_FEND;
    kiss->buffer[1] = 0x00; /* default header */
    kiss->index = 2;

    for (uint16_t i = 0; i < length; i++)
    {
        uint8_t b = data[i];
        if (b == KISS_FEND)
        {
            kiss->buffer[kiss->index++] = KISS_FESC;
            kiss->buffer[kiss->index++] = KISS_TFEND;
        }
        else if (b == KISS_FESC)
        {
            kiss->buffer[kiss->index++] = KISS_FESC;
            kiss->buffer[kiss->index++] = KISS_TFESC;
        }
        else
        {
            kiss->buffer[kiss->index++] = b;
        }
    }

    /* Terminate frame */
    kiss->buffer[kiss->index++] = KISS_FEND;

    return 0;
}


/**
 * kiss_decode
 * -----------
 * Decode an encoded frame that is present in `kiss->buffer`.
 * The caller must set `kiss->index` to the number of bytes in the buffer
 * that belong to the encoded frame prior to calling this function.
 *
 * Parameters:
 *  - kiss: instance containing encoded frame data
 *  - output: buffer to receive decoded payload
 *  - output_length: pointer to variable that will receive decoded length
 *  - header: optional pointer to receive the KISS header byte (may be NULL)
 *
 * Returns:
 *  - 0 on success
 *  - KISS_ERR_INVALID_PARAMS for bad pointers
 *  - KISS_ERR_INVALID_FRAME for malformed frames or bad escape sequences
 */
int kiss_decode(kiss_instance_t *kiss, uint8_t *output, uint16_t *output_length, uint8_t *header)
{
    if (kiss == NULL || output == NULL || output_length == NULL) return KISS_ERR_INVALID_PARAMS;

    uint16_t out_index = 0;
    uint8_t escape = 0;

    for (uint16_t i = 0; i < kiss->index; i++)
    {
        uint8_t byte = kiss->buffer[i];

        /* Validate frame start/end positions */
        if (i == 0 && byte != KISS_FEND)
            return KISS_ERR_INVALID_FRAME;

        if (i == kiss->index - 1 && byte != KISS_FEND)
            return KISS_ERR_INVALID_FRAME;

        /* The second byte is the per-frame header (skip into 'header') */
        if (i == 1)
        {
            if (header != NULL) *header = byte;
            continue;
        }

        if (byte == KISS_FEND)
        {
            /* Skip stray frame delimiters found inside provided buffer */
            continue;
        }
        else if (byte == KISS_FESC)
        {
            escape = 1;
            continue;
        }
        else
        {
            if (escape)
            {
                if (byte == KISS_TFEND) output[out_index++] = KISS_FEND;
                else if (byte == KISS_TFESC) output[out_index++] = KISS_FESC;
                else return KISS_ERR_INVALID_FRAME; /* unknown escape */
                escape = 0;
            }
            else
            {
                output[out_index++] = byte;
            }
        }
    }

    *output_length = out_index;
    return 0;
}


/**
 * kiss_send_frame
 * ---------------
 * Send `kiss->index` bytes from `kiss->buffer` using the write callback.
        /* Do NOT modify the instance's working buffer or its `index` here.
         * The caller provided the encoded data in `kiss->buffer` and may rely
         * on `kiss->index` remaining unchanged. Only return decoded data via
         * `output` and `output_length`.
        
        output_length = out_index;
        return 0;
 *  - KISS_ERR_INVALID_PARAMS if instance or callback is NULL
 */
int kiss_send_frame(kiss_instance_t *kiss)
{
    if (kiss == NULL || kiss->write == NULL) return KISS_ERR_INVALID_PARAMS;

    for (uint16_t i = 0; i < kiss->index; i++)
    {
        kiss->write(kiss->buffer[i]);
    }

    return 0;
}


/**
 * kiss_receive_frame
 * ------------------
 * Read bytes from the transport until a terminating FEND is received and
 * decode into `output`.
 *
 * Notes:
 *  - This routine calls the user `read` callback repeatedly with length=1.
 *  - `read` must provide data reliably (blocking or buffering as needed).
 *
 * Returns:
 *  - 0 on success
 *  - KISS_ERR_INVALID_PARAMS for bad inputs
 *  - KISS_ERR_INVALID_FRAME for bad escape sequences
 *  - KISS_ERR_BUFFER_OVERFLOW if decoded data exceeds `kiss->buffer_size`
 */
int kiss_receive_frame(kiss_instance_t *kiss, uint32_t maxAttempts)
{
    if (kiss == NULL || kiss->read == NULL || kiss->buffer == NULL)
        return KISS_ERR_INVALID_PARAMS;

    uint8_t byte;
    kiss->index = 0;
    int frame_started = 0;

    for(int attempt = 0; attempt < maxAttempts; attempt++)
    {
        kiss->read(&byte, 1);

        if (!frame_started)
        {
            if (byte == KISS_FEND)
            {
                kiss->buffer[kiss->index++] = byte;
                frame_started = 1;
            }
            continue; 
        }

        if (kiss->index >= kiss->buffer_size)
        {
            return KISS_ERR_BUFFER_OVERFLOW;
        }
        
        kiss->buffer[kiss->index++] = byte;

        if (byte == KISS_FEND)
        {
            if (kiss->index == 2) 
            {
                kiss->index = 1; 
            }
            else 
            {
                return 0;
            }
        }
    }
}

