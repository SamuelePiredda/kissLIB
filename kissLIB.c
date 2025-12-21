#include "kissLIB.h"



int kiss_init(kiss_instance_t *kiss, uint8_t *buffer, uint16_t buffer_size, kiss_write_fn write, kiss_read_fn read)
{
    if (kiss == NULL || buffer == NULL || buffer_size == 0 || write == NULL || read == NULL)
        return KISS_ERR_INVALID_PARAMS; // Invalid parameters

    kiss->buffer = buffer;
    kiss->buffer_size = buffer_size;
    kiss->index = 0;
    kiss->write = write;
    kiss->read = read;

    return 0; // Success
}


int kiss_encode(kiss_instance_t *kiss, const uint8_t *data, uint16_t length)
{
    if (kiss == NULL || data == NULL || length == 0) return KISS_ERR_INVALID_PARAMS;
    if (length * 2 + 2 > kiss->buffer_size) return KISS_ERR_BUFFER_OVERFLOW; // Not enough buffer space

    kiss->buffer[0] = KISS_FEND;
    kiss->buffer[1] = 0x00;
    kiss->index = 2;

    for (uint16_t i = 0; i < length; i++)
    {
        if (data[i] == KISS_FEND)
        {
            kiss->buffer[kiss->index++] = KISS_FESC;
            kiss->buffer[kiss->index++] = KISS_TFEND;
        }
        else if (data[i] == KISS_FESC)
        {
            kiss->buffer[kiss->index++] = KISS_FESC;
            kiss->buffer[kiss->index++] = KISS_TFESC;
        }
        else
        {
            kiss->buffer[kiss->index++] = data[i];
        }
    }

    kiss->buffer[kiss->index++] = KISS_FEND;

    return 0; // Success
}

int kiss_decode(kiss_instance_t *kiss, uint8_t *output, uint16_t *output_length, uint8_t *header)
{
    if (kiss == NULL || output == NULL || output_length == NULL) return KISS_ERR_INVALID_PARAMS;

    uint16_t out_index = 0;
    uint8_t escape = 0;

    for (uint16_t i = 0; i < kiss->index; i++)
    {
        uint8_t byte = kiss->buffer[i];

        if(i == 0 && byte != KISS_FEND)
            return KISS_ERR_INVALID_FRAME; // Frame must start with FEND

        if(i == kiss->index - 1 && byte != KISS_FEND)
            return KISS_ERR_INVALID_FRAME; // Frame must end with FEND

        if(i == 1)
        {
            if(header != NULL)
                *header = byte; // Store header byte
            continue; // Skip header byte
        }

        if (byte == KISS_FEND)
        {
            continue; // Ignore FEND
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
                if (byte == KISS_TFEND)
                {
                    output[out_index++] = KISS_FEND;
                }
                else if (byte == KISS_TFESC)
                {
                    output[out_index++] = KISS_FESC;
                }
                else
                {
                    return KISS_ERR_INVALID_FRAME; // Invalid escape sequence
                }
                escape = 0;
            }
            else
            {
                output[out_index++] = byte;
            }
        }
    }

    kiss->index = out_index;
    *output_length = out_index;
    return 0; // Success
}


int kiss_send_frame(kiss_instance_t *kiss)
{
    if (kiss == NULL || kiss->write == NULL) return KISS_ERR_INVALID_PARAMS;

    for (uint16_t i = 0; i < kiss->index; i++)
    {
        kiss->write(kiss->buffer[i]);
    }
}

int kiss_receive_frame(kiss_instance_t *kiss, uint8_t *output, uint16_t *output_length)
{
    if (kiss == NULL || kiss->read == NULL || output == NULL || output_length == NULL)
        return KISS_ERR_INVALID_PARAMS;

    uint8_t byte;
    uint16_t out_index = 0;
    uint8_t escape = 0;

    for(int i = 0; i < kiss->buffer_size; i++)
    {
        kiss->read(&byte, 1);

        if (byte == KISS_FEND)
        {
            if (out_index > 0)
                break; // End of frame
            else
                continue; // Ignore leading FEND
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
                if (byte == KISS_TFEND)
                    output[out_index++] = KISS_FEND;
                else if (byte == KISS_TFESC)
                    output[out_index++] = KISS_FESC;
                else
                    return KISS_ERR_INVALID_FRAME; // Invalid escape sequence
                escape = 0;
            }
            else
                output[out_index++] = byte;
        }

        if (out_index >= kiss->buffer_size)
            return KISS_ERR_BUFFER_OVERFLOW; // Output buffer overflow
    }

    kiss->index = out_index;
    *output_length = out_index;
    return 0; // Success
}

