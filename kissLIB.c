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
 * kiss_init_crc32_table
 * -----------------------
 * Initialize the CRC32 lookup table.
 */
uint32_t kiss_CRC32_Table[256];






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
int kiss_init(kiss_instance_t *kiss, uint8_t *buffer, size_t buffer_size, uint8_t tx_delay, kiss_write_fn write, kiss_read_fn read, void* context)
{
    if (kiss == NULL || buffer_size == 0)
        return KISS_ERR_INVALID_PARAMS;

    kiss->buffer = buffer;
    kiss->context = context;
    kiss->TXdelay = tx_delay;
    kiss->buffer_size = buffer_size;
    kiss->index = 0;
    kiss->write = write;
    kiss->read = read;
    kiss->Status = KISS_NOTHING;

    kiss_init_crc32_table();

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
int kiss_encode(kiss_instance_t *kiss, uint8_t *data, size_t *length, const uint8_t header)
{
    if (kiss == NULL || data == NULL || length == 0) return KISS_ERR_INVALID_PARAMS;
    if(kiss->buffer_size < 3) return KISS_ERR_BUFFER_OVERFLOW;

    /*  Start of frame with header
        if header is NULL use default 0x00 header for data
        if header is not NULL use the provided header value from the caller
    */
    kiss->buffer[0] = KISS_FEND;
    kiss->buffer[1] = header;
    kiss->index = 2;

    for (size_t i = 0; i < *length; i++)
    {
        uint8_t b = data[i];
        if (b == KISS_FEND)
        {
            if(kiss->index + 2 > kiss->buffer_size)
                return KISS_ERR_BUFFER_OVERFLOW;
            kiss->buffer[kiss->index++] = KISS_FESC;
            kiss->buffer[kiss->index++] = KISS_TFEND;
        }
        else if (b == KISS_FESC)
        {
            if(kiss->index + 2 > kiss->buffer_size)
                return KISS_ERR_BUFFER_OVERFLOW;
            kiss->buffer[kiss->index++] = KISS_FESC;
            kiss->buffer[kiss->index++] = KISS_TFESC;
        }
        else
        {
            if(kiss->index + 1 > kiss->buffer_size)
                return KISS_ERR_BUFFER_OVERFLOW;
            kiss->buffer[kiss->index++] = b;
        }
    }

    /* Terminate frame */
    if(kiss->index + 1 > kiss->buffer_size)
        return KISS_ERR_BUFFER_OVERFLOW;
    kiss->buffer[kiss->index++] = KISS_FEND;
    *length = kiss->index;
    kiss->Status = KISS_TRANSMITTING;

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
int kiss_decode(kiss_instance_t *kiss, uint8_t *output, size_t *output_length, uint8_t *header)
{
    if (kiss == NULL || output == NULL || output_length == NULL) return KISS_ERR_INVALID_PARAMS;

    size_t out_index = 0;
    uint8_t escape = 0;

    for (size_t i = 0; i < kiss->index; i++)
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
    kiss->index = 0; /* reset index after decoding */
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
    if (kiss == NULL || kiss->write == NULL) 
        return KISS_ERR_INVALID_PARAMS;
    if(kiss->Status != KISS_TRANSMITTING)
        return KISS_ERR_DATA_NOT_ENCODED;

    int err = 0;
    err = kiss->write(kiss, kiss->buffer, kiss->index);
    if(err == 0)
    {
        kiss->Status = KISS_TRANSMITTED;
        return 0;
    }
    else
    {
        kiss->Status = KISS_ERROR_STATE;
        return err;
    }
}


/**
 * kiss_encode_and_send
 * ----------------------
 * Encode `length` bytes from `data` into the instance working buffer and send it.
 * * Behavior:
 *  - Calls kiss_encode to encode the data into kiss->buffer.
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
int kiss_encode_and_send(kiss_instance_t *kiss, uint8_t *data, size_t *length, uint8_t header)
{
    int err = 0;
    err = kiss_encode(kiss, data, length, header);
    if(err != 0)
    {
        return err;
    }
    return kiss_send_frame(kiss);
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
    // Validate inputs
    if (kiss == NULL || kiss->read == NULL || kiss->buffer == NULL)
        return KISS_ERR_INVALID_PARAMS;

    uint8_t byte;
    kiss->index = 0;
    kiss->Status = KISS_RECEIVING;
    int frame_started = 0;
    int err = 0;

    // Read bytes until a full frame is received
    for(int attempt = 0; attempt < maxAttempts; attempt++)
    {
        err = kiss->read(kiss, &byte, kiss->buffer_size, &(kiss->index));

        if(err != 0)
        {
            kiss->Status = KISS_ERROR_STATE;
            return err;
        }

        kiss->index = 0;
        
        for(size_t i = 0; i < kiss->index; i++)
        {
            byte = kiss->buffer[i];

            if (!frame_started)
            {
                if (byte == KISS_FEND)
                {
                    kiss->buffer[kiss->index++] = byte;                    
                    frame_started = 1;
                }
                continue; 
            }

            // overflow check
            if (kiss->index >= kiss->buffer_size)
            {
                kiss->Status = KISS_RECEIVED_ERROR;
                return KISS_ERR_BUFFER_OVERFLOW;
            }
            
            // Store byte arrived in buffer with index increment
            kiss->buffer[kiss->index++] = byte;

            if (byte == KISS_FEND)
            {
                if (kiss->index == 2) 
                {
                    kiss->index = 1; 
                }
                else 
                {
                    kiss->Status = KISS_RECEIVED;
                    kiss->index++;
                    return 0;
                }
            }

        }

        return KISS_ERR_INVALID_FRAME;
    }

    return KISS_ERR_NO_DATA_RECEIVED;
}




/** 
 * kiss_receive_and_decode
 * -----------------------
 * Receive a KISS frame and decode it into `output`.
 *
 * Parameters:
 *  - kiss: instance with working buffer and `read` callback.
 *  - output: buffer to receive decoded payload bytes.
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
int kiss_receive_and_decode(kiss_instance_t *kiss, uint8_t *output, size_t *output_length, uint32_t maxAttempts, uint8_t *header)
{
    int err = 0;
    err = kiss_receive_frame(kiss, maxAttempts);
    if(err != 0)
    {
        return err;
    }       
    
    return kiss_decode(kiss, output, output_length, header);
}







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
 */
int kiss_set_TXdelay(kiss_instance_t *kiss, uint8_t tx_delay)
{
    if (kiss == NULL || tx_delay == 0)
        return KISS_ERR_INVALID_PARAMS;

    kiss->TXdelay = tx_delay*10;

    kiss->buffer[0] = KISS_FEND;
    kiss->buffer[1] = KISS_HEADER_TX_DELAY;
    kiss->buffer[2] = tx_delay;
    kiss->buffer[3] = KISS_FEND;
    kiss->index = 4;
    kiss->Status = KISS_TRANSMITTING;

    return kiss_send_frame(kiss);
}

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
 */
int kiss_set_speed(kiss_instance_t *kiss, uint32_t BaudRate)
{
    if (kiss == NULL || BaudRate == 0)
        return KISS_ERR_INVALID_PARAMS;

    kiss->buffer[0] = KISS_FEND;
    kiss->buffer[1] = KISS_HEADER_SPEED;
    kiss->buffer[2] = (uint8_t)(BaudRate & 0xFF);
    kiss->buffer[3] = (uint8_t)((BaudRate >> 8) & 0xFF);
    kiss->buffer[4] = (uint8_t)((BaudRate >> 16) & 0xFF);
    kiss->buffer[5] = (uint8_t)((BaudRate >> 24) & 0xFF);
    kiss->buffer[6] = KISS_FEND;
    kiss->index = 7;
    kiss->Status = KISS_TRANSMITTING;

    return kiss_send_frame(kiss);
}

/**
 * kiss_send_ack
 * -----------------   
 * Send an ACK control frame.
 * Parameters:
 * - kiss: initialized instance.
 * Returns:
 * - 0 on success
 * - KISS_ERR_INVALID_PARAMS if inputs are invalid
 */
int kiss_send_ack(kiss_instance_t *kiss)
{
    if (kiss == NULL)
        return KISS_ERR_INVALID_PARAMS;

    kiss->buffer[0] = KISS_FEND;
    kiss->buffer[1] = KISS_HEADER_ACK;
    kiss->buffer[2] = KISS_FEND;
    kiss->index = 3;
    kiss->Status = KISS_TRANSMITTING;

    return kiss_send_frame(kiss);
}


/**
 * kiss_send_nack
 * -----------------   
 * Send a NACK control frame.
 * Parameters:
 * - kiss: initialized instance.
 * Returns:
 * - 0 on success
 * - KISS_ERR_INVALID_PARAMS if inputs are invalid
 */
int kiss_send_nack(kiss_instance_t *kiss)
{
    if (kiss == NULL)
        return KISS_ERR_INVALID_PARAMS;

    kiss->buffer[0] = KISS_FEND;
    kiss->buffer[1] = KISS_HEADER_NACK;
    kiss->buffer[2] = KISS_FEND;
    kiss->index = 3;
    kiss->Status = KISS_TRANSMITTING;

    return kiss_send_frame(kiss);
}


/** 
 * kiss_send_ping
 * -----------------
 * Send a PING control frame.
 * Parameters:
 * - kiss: initialized instance.
 * Returns:
 * - 0 on success
 * - KISS_ERR_INVALID_PARAMS if inputs are invalid
 */
int kiss_send_ping(kiss_instance_t *kiss)
{
    if (kiss == NULL)
        return KISS_ERR_INVALID_PARAMS;

    kiss->buffer[0] = KISS_FEND;
    kiss->buffer[1] = KISS_HEADER_PING;
    kiss->buffer[2] = KISS_FEND;
    kiss->index = 3;
    kiss->Status = KISS_TRANSMITTING;

    return kiss_send_frame(kiss);
}


/**< Initialize the CRC32 lookup table.
 * -----------------------
 * kiss_init_crc32_table
 * -----------------------
 * Initialize the CRC32 lookup table.   
 */
void kiss_init_crc32_table()
{
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) 
    {
        uint32_t crc = i;
        for (uint32_t j = 8; j > 0; j--) 
            if (crc & 1) 
                crc = (crc >> 1) ^ polynomial;
            else 
                crc >>= 1;
        kiss_CRC32_Table[i] = crc;
    }   
}

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
uint32_t kiss_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) 
    {
        uint8_t byte = data[i];
        uint32_t lookupIndex = (crc ^ byte) & 0xFF;
        crc = (crc >> 8) ^ kiss_CRC32_Table[lookupIndex];
    }
    return crc ^ 0xFFFFFFFF;
}

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
int kiss_verify_crc32(const uint8_t *data, size_t len, uint32_t expected_crc)
{
    return (kiss_crc32(data, len) == expected_crc) ? 1 : 0;
}




uint32_t kiss_encode_crc32(kiss_instance_t *kiss, uint8_t *data, size_t *length, const uint8_t header)
{
    int kiss_err = 0;
    kiss_err = kiss_encode(kiss, data, length, header);
    if(kiss_err != 0)
    {
        return kiss_err;
    }

    kiss->index -= 1; // Remove the trailing FEND for CRC32 appending

    uint32_t crc = kiss_crc32(data, *length);

    for (int i = 0; i < 4; i++)
    {
        uint8_t b = (crc >> (i * 8)) & 0xFF;
        if (b == KISS_FEND)
        {
            if(kiss->index + 2 > kiss->buffer_size)
                return KISS_ERR_BUFFER_OVERFLOW;
            kiss->buffer[kiss->index++] = KISS_FESC;
            kiss->buffer[kiss->index++] = KISS_TFEND;
        }
        else if (b == KISS_FESC)
        {
            if(kiss->index + 2 > kiss->buffer_size)
                return KISS_ERR_BUFFER_OVERFLOW;
            kiss->buffer[kiss->index++] = KISS_FESC;
            kiss->buffer[kiss->index++] = KISS_TFESC;
        }
        else
        {
            if(kiss->index + 1 > kiss->buffer_size)
                return KISS_ERR_BUFFER_OVERFLOW;
            kiss->buffer[kiss->index++] = b;
        }
    }

    /* Terminate frame */
    if(kiss->index + 1 > kiss->buffer_size)
        return KISS_ERR_BUFFER_OVERFLOW;
    kiss->buffer[kiss->index++] = KISS_FEND;
    *length = kiss->index;
    kiss->Status = KISS_TRANSMITTING;

    return 0;
}





int kiss_decode_crc32(kiss_instance_t *kiss, uint8_t *output, size_t *output_length, uint8_t *header)
{
    int err = kiss_decode(kiss, output, output_length, header);
    if (err != 0)
    {
        return err;
    }



    if (!kiss_verify_crc32(output, *output_length - 4, 
        (uint32_t)(output[*output_length - 4] | 
                   (output[*output_length - 3] << 8) | 
                   (output[*output_length - 2] << 16) | 
                   (output[*output_length - 1] << 24))))
    {
        *output_length = *output_length - 4; // Exclude CRC32 bytes
        kiss->index = 0; /* reset index after decoding */
        kiss->Status = KISS_RECEIVED_ERROR;
        return KISS_ERR_CRC32_MISMATCH;
    }

    *output_length = *output_length - 4; // Exclude CRC32 bytes
    kiss->index = 0; /* reset index after decoding */
    kiss->Status = KISS_RECEIVED;

    return 0;
}

