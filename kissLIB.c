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



static int CRC_INIT_FLAG = 0;

/**
 * kiss_init_crc32_table
 * -----------------------
 * Initialize the CRC32 lookup table.
 */
static uint32_t kiss_CRC32_Table[256];






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
    kiss->Status = KISS_STATUS_NOTHING;

    if(CRC_INIT_FLAG == 0)
    {
        kiss_init_crc32_table();
        CRC_INIT_FLAG = 1;
    }

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
    if(*length + 3 > kiss->buffer_size) return KISS_ERR_BUFFER_OVERFLOW;
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
    kiss->Status = KISS_STATUS_TRANSMITTING;

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
int kiss_decode(kiss_instance_t *kiss, uint8_t *output, size_t output_max_size, size_t *output_length, uint8_t *header)
{
    if (kiss == NULL || output == NULL || output_length == NULL) return KISS_ERR_INVALID_PARAMS;

    size_t out_index = 0;
    uint8_t escape = 0;

    for (size_t i = 0; i < kiss->index; i++)
    {
        if(out_index >= output_max_size)
            return KISS_ERR_BUFFER_OVERFLOW;

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
    if (kiss == NULL || kiss->write == NULL) 
        return KISS_ERR_INVALID_PARAMS;
    if(kiss->Status != KISS_STATUS_TRANSMITTING)
        return KISS_ERR_DATA_NOT_ENCODED;

    int err = 0;
    err = kiss->write(kiss, kiss->buffer, kiss->index);
    if(err == 0)
    {
        kiss->Status = KISS_STATUS_TRANSMITTED;
        return 0;
    }
    else
    {
        kiss->Status = KISS_STATUS_ERROR_STATE;
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
    kiss->Status = KISS_STATUS_RECEIVING;
    int frame_started = 0;
    int err = 0;

    // Read bytes until a full frame is received
    for(int attempt = 0; attempt < maxAttempts; attempt++)
    {
        err = kiss->read(kiss, kiss->buffer, kiss->buffer_size, &(kiss->index));

        if(err != 0)
        {
            kiss->Status = KISS_STATUS_ERROR_STATE;
            return err;
        }
        
        for(size_t i = 0; i < kiss->index; i++)
        {
            byte = kiss->buffer[i];

            if (!frame_started)
            {
                if (byte == KISS_FEND)
                    frame_started = 1;
                continue; 
            }
           
            if (byte == KISS_FEND)
            {
                kiss->Status = KISS_STATUS_RECEIVED;
                kiss->index = i;
                return 0;
            }        
        }
        kiss->index = 0;
        return KISS_ERR_NO_DATA_RECEIVED;
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
int kiss_receive_and_decode(kiss_instance_t *kiss, uint8_t *output, size_t output_max_size, size_t *output_length, uint32_t maxAttempts, uint8_t *header)
{
    int err = 0;
    err = kiss_receive_frame(kiss, maxAttempts);
    if(err != 0)
    {
        return err;
    }       
    
    return kiss_decode(kiss, output, output_max_size, output_length, header);
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
    if(kiss->buffer_size < 4)
        return KISS_ERR_BUFFER_OVERFLOW;

    kiss->TXdelay = tx_delay;

    uint8_t payData = tx_delay;
    size_t len = 1;

    return kiss_encode_and_send(kiss, &payData, &len, KISS_HEADER_TX_DELAY);
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
    if(kiss->buffer_size < 7)
        return KISS_ERR_BUFFER_OVERFLOW;

    uint8_t payData[4];
    size_t len = 4;

    payData[0] = (uint8_t)(BaudRate & 0xFF);
    payData[1] = (uint8_t)((BaudRate >> 8) & 0xFF);
    payData[2] = (uint8_t)((BaudRate >> 16) & 0xFF);
    payData[3] = (uint8_t)((BaudRate >> 24) & 0xFF);

    return kiss_encode_and_send(kiss, payData, &len, KISS_HEADER_SPEED);
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
    if(kiss->buffer_size < 3)
        return KISS_ERR_BUFFER_OVERFLOW;


    kiss->buffer[0] = KISS_FEND;
    kiss->buffer[1] = KISS_HEADER_ACK;
    kiss->buffer[2] = KISS_FEND;
    kiss->index = 3;
    kiss->Status = KISS_STATUS_TRANSMITTING;

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
    kiss->Status = KISS_STATUS_TRANSMITTING;

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
    kiss->Status = KISS_STATUS_TRANSMITTING;

    return kiss_send_frame(kiss);
}


/**< Initialize the CRC32 lookup table.
 * -----------------------
 * kiss_init_crc32_table
 * -----------------------
 * Initialize the CRC32 lookup table.   
 */
static void kiss_init_crc32_table()
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




int kiss_encode_crc32(kiss_instance_t *kiss, uint8_t *data, size_t *length, uint8_t header)
{
    // Check for null pointers or invalid input length
    if (kiss == NULL || data == NULL || length == NULL || *length == 0) 
        return KISS_ERR_INVALID_PARAMS;

    // Safety check: ensure the internal buffer size matches the provided max_out_size
    if (*length > kiss->buffer_size) 
        return KISS_ERR_BUFFER_OVERFLOW;

    // 1. Calculate CRC32 on ORIGINAL data before any KISS encoding
    uint32_t crc = kiss_crc32(data, *length);

    // 2. Perform standard KISS encoding (updates *length with encoded size)
    int err = kiss_encode(kiss, data, length, header);
    if (err != 0) return err;

    // 3. Remove the trailing FEND to inject CRC bytes
    if (kiss->index < 1) return KISS_ERR_INVALID_FRAME;
    kiss->index--;

    // 4. Add the 4 CRC bytes (Little Endian) with KISS escaping logic
    for (int i = 0; i < 4; i++)
    {
        uint8_t b = (crc >> (i * 8)) & 0xFF;
        
        if (b == KISS_FEND)
        {
            if (kiss->index + 2 >= kiss->buffer_size) return KISS_ERR_BUFFER_OVERFLOW;
            kiss->buffer[kiss->index++] = KISS_FESC;
            kiss->buffer[kiss->index++] = KISS_TFEND;
        }
        else if (b == KISS_FESC)
        {
            if (kiss->index + 2 >= kiss->buffer_size) return KISS_ERR_BUFFER_OVERFLOW;
            kiss->buffer[kiss->index++] = KISS_FESC;
            kiss->buffer[kiss->index++] = KISS_TFESC;
        }
        else
        {
            if (kiss->index + 1 >= kiss->buffer_size) return KISS_ERR_BUFFER_OVERFLOW;
            kiss->buffer[kiss->index++] = b;
        }
    }

    // 5. Re-add the final frame terminator (FEND)
    if (kiss->index + 1 > kiss->buffer_size) return KISS_ERR_BUFFER_OVERFLOW;
    kiss->buffer[kiss->index++] = KISS_FEND;

    // 6. Update the final length of the encoded frame
    *length = kiss->index;
    kiss->Status = KISS_STATUS_TRANSMITTING;

    return 0;
}




int kiss_decode_crc32(kiss_instance_t *kiss, uint8_t *output, size_t max_out_size, size_t *output_length, uint8_t *header)
{
    if (kiss == NULL || output == NULL || output_length == NULL)
        return KISS_ERR_INVALID_PARAMS;

    // 1. Decode the KISS frame into the output buffer
    // Note: ensure your base kiss_decode also respects max_out_size
    int err = kiss_decode(kiss, output, max_out_size, output_length, header);
    if (err != 0) return err;

    // 2. Safety check: the decoded data must fit in max_out_size 
    // (Already partially checked by kiss_decode, but we verify again for CRC)
    if (*output_length > max_out_size)
    {
        kiss->Status = KISS_STATUS_RECEIVED_ERROR;
        return KISS_ERR_BUFFER_OVERFLOW;
    }

    // 3. A valid CRC32 frame must contain at least 4 bytes for the checksum
    if (*output_length < 4)
    {
        kiss->Status = KISS_STATUS_RECEIVED_ERROR;
        return KISS_ERR_INVALID_FRAME;
    }

    // 4. Extract the received CRC (the last 4 bytes of the decoded payload)
    size_t payload_len = *output_length - 4;
    uint32_t received_crc = (uint32_t)output[payload_len] |
                            ((uint32_t)output[payload_len + 1] << 8) |
                            ((uint32_t)output[payload_len + 2] << 16) |
                            ((uint32_t)output[payload_len + 3] << 24);
    *output_length = payload_len;
    // 5. Verify the calculated CRC of the payload against the received one
    if (!kiss_verify_crc32(output, payload_len, received_crc))
    {
        
        kiss->Status = KISS_STATUS_RECEIVED_ERROR;
        return KISS_ERR_CRC32_MISMATCH;
    }

    kiss->Status = KISS_STATUS_RECEIVED;

    return 0;
}






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
 */
int kiss_encode_send_crc32(kiss_instance_t *kiss, uint8_t *data, size_t *length, uint8_t header)
{
    int err = kiss_encode_crc32(kiss, data, length, header);
    if(err != 0)
    {
        kiss->Status = KISS_STATUS_ERROR_STATE;
        return err;
    }    

    return kiss_send_frame(kiss);
}