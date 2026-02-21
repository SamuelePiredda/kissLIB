#include "kissLIB.h"

/* Implementation of a minimal, well-documented KISS framing library.
 * ----------------
+* File-level notes:
 * - This library is intentionally small and depends on the caller to
 *   provide I/O callbacks and working memory. It does not allocate memory.
 * - Users should ensure their `read` callback behavior matches expectations
 *   (blocking vs non-blocking) since `kiss_receive_frame` calls `read` in a
 *   loop until a frame terminator is seen.
 * - The use of CRC32 or not should be used by the user. I suggest to always use or never 
 *   use CRC32 and to not change this during running. If you expect a noisy envoironment 
 *   always use CRC32 otherwise not.
 */



#ifdef ARDUINO 

// necessary lib to handle flash memory
#include <avr/pgmspace.h>


/* maximum padding block that the device can send before the frame */
static const uint8_t kiss_padding_block[KISS_MAX_PADDING] PROGMEM = {
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
};


/**
 * kiss_init_crc32_table
 * ----------------------- 
 * Initialize the CRC32 lookup table. IEEE 802.3 (0xEDB88320)
 */
static const uint32_t kiss_CRC32_Table[256] PROGMEM = {
    0x00000000, 0x77073096,  0xEE0E612C, 0x990951BA,   0x076DC419, 0x706AF48F,  0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4,  0xE0D5E91E, 0x97D2D988,   0x09B64C2B, 0x7EB17CBD,  0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2,  0xF3B97148, 0x84BE41DE,   0x1ADAD47D, 0x6DDDE4EB,  0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0,  0xFD62F97A, 0x8A65C9EC,   0x14015C4F, 0x63066CD9,  0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E,  0xD56041E4, 0xA2677172,   0x3C03E4D1, 0x4B04D447,  0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C,  0xDBBBC9D6, 0xACBCF940,   0x32D86CE3, 0x45DF5C75,  0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A,  0xC8D75180, 0xBFD06116,   0x21B4F4B5, 0x56B3C423,  0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808,  0xC60CD9B2, 0xB10BE924,   0x2F6F7C87, 0x58684C11,  0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106,  0x98D220BC, 0xEFD5102A,   0x71B18589, 0x06B6B51F,  0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934,  0x9609A88E, 0xE10E9818,   0x7F6A0DBB, 0x086D3D2D,  0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162,  0x856530D8, 0xF262004E,   0x6C0695ED, 0x1B01A57B,  0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950,  0x8BBEB8EA, 0xFCB9887C,   0x62DD1DDF, 0x15DA2D49,  0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE,  0xA3BC0074, 0xD4BB30E2,   0x4ADFA541, 0x3DD895D7,  0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC,  0xAD678846, 0xDA60B8D0,   0x44042D73, 0x33031DE5,  0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA,  0xBE0B1010, 0xC90C2086,   0x5768B525, 0x206F85B3,  0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998,  0xB0D09822, 0xC7D7A8B4,   0x59B33D17, 0x2EB40D81,  0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6,  0x03B6E20C, 0x74B1D29A,   0xEAD54739, 0x9DD277AF,  0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84,  0x0D6D6A3E, 0x7A6A5AA8,   0xE40ECF0B, 0x9309FF9D,  0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2,  0x1E01F268, 0x6906C2FE,   0xF762575D, 0x806567CB,  0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0,  0x10DA7A5A, 0x67DD4ACC,   0xF9B9DF6F, 0x8EBEEFF9,  0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E,  0x38D8C2C4, 0x4FDFF252,   0xD1BB67F1, 0xA6BC5767,  0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C,  0x36034AF6, 0x41047A60,   0xDF60EFC3, 0xA867DF55,  0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A,  0x256FD2A0, 0x5268E236,   0xCC0C7795, 0xBB0B4703,  0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28,  0x2BB45A92, 0x5CB36A04,   0xC2D7FFA7, 0xB5D0CF31,  0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226,  0x756AA39C, 0x026D930A,   0x9C0906A9, 0xEB0E363F,  0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14,  0x7BB12BAE, 0x0CB61B38,   0x92D28E9B, 0xE5D5BE0D,  0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242,  0x68DDB3F8, 0x1FDA836E,   0x81BE16CD, 0xF6B9265B,  0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70,  0x66063BCA, 0x11010B5C,   0x8F659EFF, 0xF862AE69,  0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE,  0x4E048354, 0x3903B3C2,   0xA7672661, 0xD06016F7,  0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC,  0x40DF0B66, 0x37D83BF0,   0xA9BCAE53, 0xDEBB9EC5,  0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A,  0x53B39330, 0x24B4A3A6,   0xBAD03605, 0xCDD70693,  0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8,  0x5D681B02, 0x2A6F2B94,   0xB40BBE37, 0xC30C8EA1,  0x5A05DF1B, 0x2D02EF8D
};








#else

/* maximum padding block that the device can send before the frame */
static const uint8_t kiss_padding_block[KISS_MAX_PADDING] = {
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
};

/**
 * kiss_init_crc32_table
 * ----------------------- 
 * Initialize the CRC32 lookup table. IEEE 802.3 (0xEDB88320)
 */
static const uint32_t kiss_CRC32_Table[256] = {
    0x00000000, 0x77073096,  0xEE0E612C, 0x990951BA,   0x076DC419, 0x706AF48F,  0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4,  0xE0D5E91E, 0x97D2D988,   0x09B64C2B, 0x7EB17CBD,  0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2,  0xF3B97148, 0x84BE41DE,   0x1ADAD47D, 0x6DDDE4EB,  0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0,  0xFD62F97A, 0x8A65C9EC,   0x14015C4F, 0x63066CD9,  0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E,  0xD56041E4, 0xA2677172,   0x3C03E4D1, 0x4B04D447,  0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C,  0xDBBBC9D6, 0xACBCF940,   0x32D86CE3, 0x45DF5C75,  0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A,  0xC8D75180, 0xBFD06116,   0x21B4F4B5, 0x56B3C423,  0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808,  0xC60CD9B2, 0xB10BE924,   0x2F6F7C87, 0x58684C11,  0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106,  0x98D220BC, 0xEFD5102A,   0x71B18589, 0x06B6B51F,  0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934,  0x9609A88E, 0xE10E9818,   0x7F6A0DBB, 0x086D3D2D,  0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162,  0x856530D8, 0xF262004E,   0x6C0695ED, 0x1B01A57B,  0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950,  0x8BBEB8EA, 0xFCB9887C,   0x62DD1DDF, 0x15DA2D49,  0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE,  0xA3BC0074, 0xD4BB30E2,   0x4ADFA541, 0x3DD895D7,  0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC,  0xAD678846, 0xDA60B8D0,   0x44042D73, 0x33031DE5,  0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA,  0xBE0B1010, 0xC90C2086,   0x5768B525, 0x206F85B3,  0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998,  0xB0D09822, 0xC7D7A8B4,   0x59B33D17, 0x2EB40D81,  0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6,  0x03B6E20C, 0x74B1D29A,   0xEAD54739, 0x9DD277AF,  0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84,  0x0D6D6A3E, 0x7A6A5AA8,   0xE40ECF0B, 0x9309FF9D,  0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2,  0x1E01F268, 0x6906C2FE,   0xF762575D, 0x806567CB,  0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0,  0x10DA7A5A, 0x67DD4ACC,   0xF9B9DF6F, 0x8EBEEFF9,  0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E,  0x38D8C2C4, 0x4FDFF252,   0xD1BB67F1, 0xA6BC5767,  0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C,  0x36034AF6, 0x41047A60,   0xDF60EFC3, 0xA867DF55,  0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A,  0x256FD2A0, 0x5268E236,   0xCC0C7795, 0xBB0B4703,  0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28,  0x2BB45A92, 0x5CB36A04,   0xC2D7FFA7, 0xB5D0CF31,  0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226,  0x756AA39C, 0x026D930A,   0x9C0906A9, 0xEB0E363F,  0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14,  0x7BB12BAE, 0x0CB61B38,   0x92D28E9B, 0xE5D5BE0D,  0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242,  0x68DDB3F8, 0x1FDA836E,   0x81BE16CD, 0xF6B9265B,  0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70,  0x66063BCA, 0x11010B5C,   0x8F659EFF, 0xF862AE69,  0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE,  0x4E048354, 0x3903B3C2,   0xA7672661, 0xD06016F7,  0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC,  0x40DF0B66, 0x37D83BF0,   0xA9BCAE53, 0xDEBB9EC5,  0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A,  0x53B39330, 0x24B4A3A6,   0xBAD03605, 0xCDD70693,  0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8,  0x5D681B02, 0x2A6F2B94,   0xB40BBE37, 0xC30C8EA1,  0x5A05DF1B, 0x2D02EF8D
};


#endif


#ifdef ARDUINO

/*
* you calculated the CRC32 for a first block of data
* now you want to add another block of data with the new CRC32 which takes into account the previous one
*/
uint32_t kiss_crc32_push(kiss_instance_t *const kiss, uint32_t prev_crc, const uint8_t *data, size_t len)
{
    if(NULL == kiss)
    {
        return KISS_ERR_INVALID_PARAMS;
    }

   
    uint32_t crc;

    if(0 == prev_crc)
    {
        crc = prev_crc ^ 0xFFFFFFFF;
    }
    else
    {
        crc = prev_crc;
    }

    for (size_t i = 0; i < len; i++) 
    {
        uint8_t lookupIndex = (uint8_t)(crc ^ data[i]); 
        uint32_t table_value = pgm_read_dword(&kiss_CRC32_Table[lookupIndex]);
        crc = (crc >> 8) ^ table_value;
    }
    return crc;
}

#else



static uint32_t kiss_crc32_push(kiss_instance_t *const kiss, uint32_t prev_crc, const uint8_t *const data, size_t len)
{
    if(NULL == kiss)
    {
        return KISS_ERR_INVALID_PARAMS;
    }

    uint32_t crc;

    if(0 == prev_crc)
    {
        crc = prev_crc ^ 0xFFFFFFFF;
    }
    else
    {
        crc = prev_crc;
    }

    for (size_t i = 0; i < len; i++) 
    {
        uint8_t byte = data[i];
        uint32_t lookupIndex = (crc ^ byte) & 0xFF;
        crc = (crc >> 8) ^ kiss_CRC32_Table[lookupIndex];
    }
    return crc;
}

#endif



int32_t kiss_init(kiss_instance_t *const kiss, uint8_t *const buffer, size_t buffer_size, uint8_t tx_delay, kiss_write_fn write, kiss_read_fn read, void *const context, uint8_t padding, uint8_t crc32)
{
    if (NULL == kiss || 0 == buffer_size || NULL == buffer)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(buffer_size < 3) 
    {
        return KISS_ERR_INVALID_PARAMS; 
    }
    if(padding > KISS_MAX_PADDING)
    {
        return KISS_ERR_PADDING_OVERFLOW;
    }

    /* initialize all the parameters */
    kiss->buffer = buffer;
    kiss->context = context;
    kiss->TXdelay = tx_delay;
    kiss->buffer_size = buffer_size;
    kiss->index = 0;
    kiss->write = write;
    kiss->read = read;
    kiss->Status = KISS_STATUS_NOTHING;
    kiss->padding = padding;
    if(0 == crc32)
    {
        kiss->CRC32 = 0;
    }
    else
    {
        kiss->CRC32 = 1;
    }


    return KISS_OK;
}





int32_t kiss_encode(kiss_instance_t *const kiss, const uint8_t *const data, size_t length, uint8_t header)
{
    /* check for parameters error or size of the buffer too small for the payload */
    if(NULL == kiss)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(NULL == kiss->buffer)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(kiss->buffer_size < 3) 
    {
        return KISS_ERR_BUFFER_OVERFLOW;
    }

    /* starting bytes of the frame */
    kiss->index = 0;
    kiss->buffer[kiss->index] = KISS_FEND;
    kiss->index++;

    if(KISS_FESC == header)
    {
        kiss->buffer[kiss->index] = KISS_FESC;
        kiss->index++;
        kiss->buffer[kiss->index] = KISS_TFESC;
        kiss->index++;
    }
    else if(KISS_FEND == header)
    {
        kiss->buffer[kiss->index] = KISS_FESC;
        kiss->index++;
        kiss->buffer[kiss->index] = KISS_TFEND;
        kiss->index++;
    }
    else
    {
        kiss->buffer[kiss->index] = header;
        kiss->index++;
    }

    /* adding payload data */
    for (size_t i = 0; i < length; i++)
    {
        uint8_t b = data[i];
        /* if it is a special character */
        if (KISS_FEND == b)
        {
            /* constantly check if there is enough space in the kiss buffer */
            if(kiss->index + 2 > kiss->buffer_size)
            {
                kiss->Status = KISS_STATUS_ERROR_STATE;
                return KISS_ERR_BUFFER_OVERFLOW;
            }
            /* add escape and special char */
            kiss->buffer[kiss->index] = KISS_FESC;
            kiss->index++;
            kiss->buffer[kiss->index] = KISS_TFEND;
            kiss->index++;
        }
        else if (KISS_FESC == b)
        {
            /* check if there is enough space in the kiss buffer */
            if(kiss->index + 2 > kiss->buffer_size)
            {
                kiss->Status = KISS_STATUS_ERROR_STATE;
                return KISS_ERR_BUFFER_OVERFLOW;
            }
            kiss->buffer[kiss->index] = KISS_FESC;
            kiss->index++;
            kiss->buffer[kiss->index] = KISS_TFESC;
            kiss->index++;
        }
        else
        {
            /* check again if there is enough space in the kiss buffer */
            if(kiss->index + 1 > kiss->buffer_size)
            {
                kiss->Status = KISS_STATUS_ERROR_STATE;
                return KISS_ERR_BUFFER_OVERFLOW;
            }
            /* add the byte in the buffer */
            kiss->buffer[kiss->index] = b;
            kiss->index++;
        }
    }

    if(1 == kiss->CRC32)
    {

        uint32_t crc = 0;
        crc = kiss_crc32_push(kiss, crc, &header, 1);
        crc = kiss_crc32_push(kiss, crc, data, length);
        crc = ~crc;

        uint8_t crc_b[4];
        crc_b[0] = (uint8_t)(crc & 0xFF);
        crc_b[1] = (uint8_t)((crc >> 8) & 0xFF);
        crc_b[2] = (uint8_t)((crc >> 16) & 0xFF);
        crc_b[3] = (uint8_t)((crc >> 24) & 0xFF);

        for(uint8_t i = 0; i < 4; i++)
        {
            if(KISS_FEND == crc_b[i])
            {
                if(kiss->index + 2 > kiss->buffer_size)
                {
                    kiss->Status = KISS_STATUS_ERROR_STATE;
                    return KISS_ERR_BUFFER_OVERFLOW;
                }
                kiss->buffer[kiss->index] = KISS_FESC;
                kiss->index++;
                kiss->buffer[kiss->index] = KISS_TFEND;
                kiss->index++;
            }
            else if(KISS_FESC == crc_b[i])
            {
                if(kiss->index + 2 > kiss->buffer_size)
                {
                    kiss->Status = KISS_STATUS_ERROR_STATE;
                    return KISS_ERR_BUFFER_OVERFLOW;
                }
                kiss->buffer[kiss->index] = KISS_FESC;
                kiss->index++;
                kiss->buffer[kiss->index] = KISS_TFESC;
                kiss->index++;                
            }
            else
            {
                if(kiss->index + 1 > kiss->buffer_size)
                {
                    kiss->Status = KISS_STATUS_ERROR_STATE;
                    return KISS_ERR_BUFFER_OVERFLOW;
                }    
                kiss->buffer[kiss->index] = crc_b[i];
                kiss->index++;           
            }

        }

    }

    /* Terminate frame with check and KISS_FEND byte*/
    if(kiss->index + 1 > kiss->buffer_size)
    {
        kiss->Status = KISS_STATUS_ERROR_STATE;
        return KISS_ERR_BUFFER_OVERFLOW;
    }
    kiss->buffer[kiss->index] = KISS_FEND;
    kiss->index++;

    /* we change the status to ready to transmit */
    kiss->Status = KISS_STATUS_TRANSMITTING;

    return KISS_OK;
}



int32_t kiss_push_encode(kiss_instance_t *const kiss, const uint8_t *const data, size_t length)
{
    
    /* control that parameters are good */
    if(NULL == kiss || NULL == data || 0 == length) 
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    
    if(KISS_STATUS_ERROR_STATE == kiss->Status)
    {
        return KISS_ERR_STATUS;
    }
    
    if(kiss->Status != KISS_STATUS_TRANSMITTING)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(0 == kiss->index)
    {
        return KISS_ERR_INVALID_PARAMS;
    }


    if(KISS_FEND == kiss->buffer[kiss->index-1])
    {
        kiss->index = kiss->index - 2;
    }
    else
    {
        kiss->Status = KISS_STATUS_ERROR_STATE;
        return KISS_ERR_INVALID_FRAME;
    }


    uint8_t crc_b[4];
    uint32_t crc = 0;

    for(uint8_t i = 0; i < 4; i++)
    {

        if(KISS_TFESC == kiss->buffer[kiss->index])
        {
            crc_b[i] = KISS_FESC;
            kiss->index = kiss->index - 2;
        }
        else if(KISS_TFEND == kiss->buffer[kiss->index])
        {
            crc_b[i] = KISS_FEND;
            kiss->index = kiss->index - 2;
        }
        else
        {
            crc_b[i] = kiss->buffer[kiss->index];
            kiss->index = kiss->index - 1;
        }
    }

    crc = ((uint32_t)crc_b[0] << 24) |
               ((uint32_t)crc_b[1] << 16) |
               ((uint32_t)crc_b[2] <<  8) |
               ((uint32_t)crc_b[3]);

    crc = ~crc;
    
    crc = kiss_crc32_push(kiss, crc, data, length);
    crc = ~crc;

    crc_b[0] = (uint8_t)(crc & 0xFF);
    crc_b[1] = (uint8_t)((crc >> 8) & 0xFF);
    crc_b[2] = (uint8_t)((crc >> 16) & 0xFF);
    crc_b[3] = (uint8_t)((crc >> 24) & 0xFF);

    kiss->index++;

    /* start putting at the end of the payload new data */
    for(size_t i = 0; i < length; i++)
    {
        /* if it is a fend special character */
        if(KISS_FEND == data[i])
        {
            if(kiss->index + 2 > kiss->buffer_size)
            {
                kiss->Status = KISS_STATUS_ERROR_STATE;
                return KISS_ERR_BUFFER_OVERFLOW;
            }
            kiss->buffer[kiss->index] = KISS_FESC;
            kiss->index++;
            kiss->buffer[kiss->index] = KISS_TFEND;
            kiss->index++;
        }
        /* if it is a fesc special character */
        else if(KISS_FESC == data[i])
        {
            if(kiss->index + 2 > kiss->buffer_size)
            {
                kiss->Status = KISS_STATUS_ERROR_STATE;
                return KISS_ERR_BUFFER_OVERFLOW;
            }
            kiss->buffer[kiss->index] = KISS_FESC;
            kiss->index++;
            kiss->buffer[kiss->index] = KISS_TFESC;
            kiss->index++;
        }
        /* otherwise just copy the data */
        else
        {
            if(kiss->index + 1 > kiss->buffer_size)
            {
                kiss->Status = KISS_STATUS_ERROR_STATE;
                return KISS_ERR_BUFFER_OVERFLOW;
            }
            kiss->buffer[kiss->index] = data[i];
            kiss->index++;
        }
    }


    for(uint8_t i = 0; i < 4; i++)
    {
        if(KISS_FEND == crc_b[i])
        {
            if(kiss->index + 2 > kiss->buffer_size)
            {
                kiss->Status = KISS_STATUS_ERROR_STATE;
                return KISS_ERR_BUFFER_OVERFLOW;             
            }
            kiss->buffer[kiss->index] = KISS_FESC;
            kiss->index++;
            kiss->buffer[kiss->index] = KISS_TFEND;
            kiss->index++;
        }
        else if(KISS_FESC == crc_b[i])
        {
            if(kiss->index + 2 > kiss->buffer_size)
            {
                kiss->Status = KISS_STATUS_ERROR_STATE;
                return KISS_ERR_BUFFER_OVERFLOW;             
            }
            kiss->buffer[kiss->index] = KISS_FESC;
            kiss->index++;
            kiss->buffer[kiss->index] = KISS_TFESC;
            kiss->index++;
        }
        else
        {
            if(kiss->index + 1 > kiss->buffer_size)
            {
                kiss->Status = KISS_STATUS_ERROR_STATE;
                return KISS_ERR_BUFFER_OVERFLOW;             
            }
            kiss->buffer[kiss->index] = crc_b[i];
            kiss->index++;
        }
    }


    /* close the frame again */
    if(kiss->index + 1 > kiss->buffer_size)
    {
        kiss->Status = KISS_STATUS_ERROR_STATE;
        return KISS_ERR_BUFFER_OVERFLOW;
    }
    kiss->buffer[kiss->index] = KISS_FEND;
    kiss->index++;
    
    return KISS_OK;
}



int32_t kiss_decode(kiss_instance_t *const kiss, uint8_t *const output, size_t output_max_size, size_t *const output_length, uint8_t *const header)
{
    /* check basic parameters */
    if (NULL == kiss || NULL == output || NULL == output_length)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if (kiss->Status != KISS_STATUS_RECEIVED) 
    {
        return KISS_ERR_STATUS;
    }

    /* pointers for fast access */
    const uint8_t *src = kiss->buffer;
    const uint8_t *src_end = kiss->buffer + kiss->index;
    uint8_t *dst = output;
    const uint8_t *dst_end = output + output_max_size;

    /* fast skip for padding */
    while (src < src_end && KISS_FEND == *src)
    {
        src++;
    }

    /* if buffer ended with only FEND */
    if (src >= src_end) 
    {
        *output_length = 0;
        kiss->Status = KISS_STATUS_ERROR_STATE;
        return KISS_ERR_INVALID_FRAME; 
    }

    /* first byte is always header, but it could be escaped  */
    uint8_t val = *src++;

    /* header escape management */
    if (KISS_FESC == val) 
    {
        if (src >= src_end) 
        {
            kiss->Status = KISS_STATUS_ERROR_STATE;
            return KISS_ERR_INVALID_FRAME;
        }
        val = *src++;
        if (KISS_TFEND == val) 
        {
            val = KISS_FEND;
        }
        else if (KISS_TFESC == val)
        {
            val = KISS_FESC;
        } 
        else 
        {
            /* illigal escape found */
            kiss->Status = KISS_STATUS_ERROR_STATE;
            return KISS_ERR_INVALID_FRAME;
        } 
    }

    /* if we find another FEND it means there is no payload, it is ok */
    else if (KISS_FEND == val)
    {
         *output_length = 0;
         return KISS_OK; 
    }

    /* Header */
    if (header) 
    {
        *header = val;
    }

    /* 3. MAIN LOOP (Payload) */
    while (src < src_end) 
    {
        uint8_t byte = *src++;

        /* final FEND */
        if (KISS_FEND == byte) 
        {
            /* exiting from loop */
            break; 
        }

        /* escape frequency found */
        if (KISS_FESC == byte) 
        {
            if (src >= src_end) 
            {
                kiss->Status = KISS_STATUS_ERROR_STATE;
                /* buffer ended before the frame was done */
                return KISS_ERR_INVALID_FRAME; 
            }
            
            /* read the next byte */
            byte = *src++;

            if (KISS_TFEND == byte)
            {
                byte = KISS_FEND;
            }
            else if (KISS_TFESC == byte) 
            {
                byte = KISS_FESC;
            }
            else 
            {
                /* the sequence was not valid */
                kiss->Status = KISS_STATUS_ERROR_STATE;
                return KISS_ERR_INVALID_FRAME;
            }
        }

        /* normal byte */
        if (dst >= dst_end)
        {
            return KISS_ERR_BUFFER_OVERFLOW;
        }
        
        *dst++ = byte;
    }

    /* final length read */
    *output_length = (size_t)(dst - output);

    if(1 == kiss->CRC32)
    {
        // Extract the received CRC (the last 4 bytes of the decoded payload)
        size_t payload_len = *output_length - 4;
        uint32_t received_crc = (uint32_t)output[payload_len] |
                                ((uint32_t)output[payload_len + 1] << 8) |
                                ((uint32_t)output[payload_len + 2] << 16) |
                                ((uint32_t)output[payload_len + 3] << 24);
        *output_length = payload_len;

        uint32_t calc_crc = 0;
        calc_crc = kiss_crc32_push(kiss, calc_crc, header, 1);
        calc_crc = kiss_crc32_push(kiss, calc_crc, output, payload_len);
        calc_crc = ~calc_crc;
        // Verify the calculated CRC of the payload against the received one
        if (calc_crc != received_crc)
        {        
            kiss->Status = KISS_STATUS_RECEIVED_ERROR;
            return KISS_ERR_CRC32_MISMATCH;
        }
    }   

    if(KISS_HEADER_ACK == *header)
    {
        kiss->frame_flag = KISS_FLAG_ACK;
    }
    else if(KISS_HEADER_NACK == *header)
    {
        kiss->frame_flag = KISS_FLAG_NACK;
    }
    else if(KISS_HEADER_PING == *header)
    {    
        kiss->frame_flag = KISS_FLAG_PING;
    }
    else
    {
        kiss->frame_flag = KISS_FLAG_NONE;
    }

    return KISS_OK;
}



int32_t kiss_send_frame(kiss_instance_t *const kiss)
{
    /* param check */
    if (NULL == kiss) 
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    /* check if the write callback function exists */
    if(NULL == kiss->write)
    {
        return KISS_ERR_CALLBACK_MISSING;
    }
    /* if we are not in the transmitting status it means there is nothing to transmit */
    if(kiss->Status != KISS_STATUS_TRANSMITTING)
    {
        return KISS_ERR_DATA_NOT_ENCODED;
    }

    int32_t err = KISS_OK;

    /* check if padding size is not too large */
    if(kiss->padding > KISS_MAX_PADDING)
    {
        return KISS_ERR_PADDING_OVERFLOW;
    }

    /* if kiss->padding is not zero we send some KISS_FEND padding bytes */
    if(kiss->padding > 0)
    {
        /* adding arduino block for extra memory reduction */
        #ifdef ARDUINO
            uint8_t chunk[KISS_MAX_PADDING];
            for(uint8_t i = 0; i < kiss->padding; i++)
            {
                chunk[i] = pgm_read_byte(&kiss_padding_block[i]);
            }
            err = kiss->write(kiss, chunk, kiss->padding);
        #else
            err = kiss->write(kiss, kiss_padding_block, kiss->padding);
        #endif

        if(err != KISS_OK)
        {
            kiss->Status = KISS_STATUS_ERROR_STATE;
            return err;
        }
    }

    /* write the frame */
    err = kiss->write(kiss, kiss->buffer, kiss->index);
    /* if no error */
    if(KISS_OK == err)
    {
        kiss->Status = KISS_STATUS_TRANSMITTED;
        return KISS_OK;
    }

    /* here we have an error */
    kiss->Status = KISS_STATUS_ERROR_STATE;
    return err;
}



int32_t kiss_encode_and_send(kiss_instance_t *const kiss, const uint8_t *const data, size_t length, uint8_t header)
{
    /* error container */
    int32_t err = KISS_OK;
    /* encoding the data */
    err = kiss_encode(kiss, data, length, header);
    /* check if the encoding went ok */
    if(err != KISS_OK)
    {
        return err;
    }
    /* sending the frame immediatly */
    return kiss_send_frame(kiss);
}



int32_t kiss_receive_frame(kiss_instance_t *const kiss, uint32_t maxAttempts)
{
    /* check if parameters are ok */
    if(NULL == kiss)
    {
        return KISS_ERR_INVALID_PARAMS;
    }

    /* the reading callback function must exist */
    if(NULL == kiss->read)
    {
        return KISS_ERR_CALLBACK_MISSING;
    }

    // Validate inputs
    if (NULL == kiss->buffer || 0 == maxAttempts)
    {
        return KISS_ERR_INVALID_PARAMS;
    }

    // we receive so we make sure that we start with index = 0
    kiss->index = 0;
    // we make sure that the status is receiving
    kiss->Status = KISS_STATUS_RECEIVING;
    // check if the frame is started or not
    uint8_t frame_started = 0;
    // error state of the read function (== 0 no error)
    int32_t err = KISS_OK;
    // frame size usage
    size_t new_index = 0;
    size_t new_read = 0;

    // Read bytes until a full frame is received
    for(uint32_t attempt = 0; attempt < maxAttempts; attempt++)
    {
        // try to read, the caller make sure that the read starts when something arrives 
        // if a frame starts in one read and ends in another read, the frame will be discarded
        err = kiss->read(kiss, &(kiss->buffer[new_index]), kiss->buffer_size, &(new_read));

        kiss->index += new_read;
        /* if the read function returns an error we stop the function and return the error */
        if(err != KISS_OK)
        {
            kiss->Status = KISS_STATUS_ERROR_STATE;
            return err;
        }
        
        /* we received something, hence we start searching for the frame inside */
        for(size_t i = new_index; i < kiss->index; i++)
        {

            /* if the frame is not started we go inside */
            if (!frame_started)
            {
                /* starting frame? */
                if (KISS_FEND == kiss->buffer[i])
                {
                    /* frame is started we copy at the start of the buffer, remember that
                    * in this case i >= new_index ALWAYS */
                    frame_started = 1;
                    kiss->buffer[new_index] = kiss->buffer[i];
                    new_index++;
                } 
            }
            else
            {
                /* if there are more C0 after the first one we just pass them since they are there for sync or padding */
                if(i > 0 && KISS_FEND == kiss->buffer[i] && new_index <= 1)
                {
                    /* do nothing, continue the cycle and ignore the C0 padding */
                }
                else
                {
                    /* we copy back the byte */
                    kiss->buffer[new_index] = kiss->buffer[i];
                    new_index++;
                
                    /* if we are here the frame is already started and we are searching for the ending byte */
                    if (KISS_FEND == kiss->buffer[i])
                    {
                        /* we have the ending byte so we set the received status */
                        kiss->Status = KISS_STATUS_RECEIVED;
                        /* we set the frame length */
                        kiss->index = new_index;

                        /* if the frame length is not enough to be valid return error state */
                        if(new_index < 3)
                        {
                            kiss->Status = KISS_STATUS_ERROR_STATE;                    
                            return KISS_ERR_INVALID_FRAME;
                        }

                        kiss->Status = KISS_STATUS_RECEIVED;
                        kiss->frame_flag = KISS_FLAG_NONE;
                        return KISS_OK;
                    } 
                }    
            }   
        }
    }
    /* if we arrive here it means no data is received */
    return KISS_ERR_NO_DATA_RECEIVED;
}



int32_t kiss_receive_and_decode(kiss_instance_t *const kiss, uint8_t *const output, size_t output_max_size, size_t *const output_length, uint32_t maxAttempts, uint8_t *const header)
{
    /* check if kiss is null*/
    if(NULL == kiss)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    /* buffer size cannot be zero */
    if(0 == kiss->buffer_size)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    /* check if output data pointer is null */
    if(NULL == output)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    /* check if the ouput length pointer is null */
    if(NULL == output_length)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    /* check if the maximum attempts are ok */
    if(0 == maxAttempts)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    
    // -----------------------

    /* error container */
    int32_t err = KISS_OK;
    /* try to receive a frame */
    err = kiss_receive_frame(kiss, maxAttempts);
    /* check if the frame has been received */
    if(err != KISS_OK)
    {
        return err;
    }
    /* decode the frame and return the output status */
    return kiss_decode(kiss, output, output_max_size, output_length, header);
}



int32_t kiss_set_TXdelay(kiss_instance_t *const kiss, uint8_t tx_delay)
{
    if (NULL == kiss || 0 == tx_delay)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(NULL == kiss->buffer)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(kiss->buffer_size < 4)
    {
        return KISS_ERR_BUFFER_OVERFLOW;
    }

    kiss->TXdelay = tx_delay;

    uint8_t payData = tx_delay;
    size_t len = 1;

    /* if we don't want CRC32 we enter here */
    return kiss_encode_and_send(kiss, &payData, len, KISS_HEADER_TX_DELAY);
    
}



int32_t kiss_set_speed(kiss_instance_t *const kiss, uint32_t BaudRate)
{
    if (NULL == kiss || 0 == BaudRate)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(NULL == kiss->buffer)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(kiss->buffer_size < 7)
    {
        return KISS_ERR_BUFFER_OVERFLOW;
    }

    uint8_t payData[4];
    size_t len = 4;

    payData[0] = (uint8_t)(BaudRate & 0xFF);
    payData[1] = (uint8_t)((BaudRate >> 8) & 0xFF);
    payData[2] = (uint8_t)((BaudRate >> 16) & 0xFF);
    payData[3] = (uint8_t)((BaudRate >> 24) & 0xFF);

    return kiss_encode_and_send(kiss, payData, len, KISS_HEADER_SPEED);

}



int32_t kiss_send_ack(kiss_instance_t *const kiss)
{
    if (NULL == kiss)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(NULL == kiss->buffer)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(kiss->buffer_size < 3)
    {
        return KISS_ERR_BUFFER_OVERFLOW;
    }

    return kiss_encode_and_send(kiss, NULL, 0, KISS_HEADER_ACK);
}



int32_t kiss_send_nack(kiss_instance_t *const kiss)
{
    if(NULL == kiss)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(NULL == kiss->buffer)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(kiss->buffer_size < 3) 
    {
        return KISS_ERR_BUFFER_OVERFLOW;
    }

    return kiss_encode_and_send(kiss, NULL, 0, KISS_HEADER_NACK);

}




int32_t kiss_send_ping(kiss_instance_t *const kiss)
{
    if(NULL == kiss)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(NULL == kiss->buffer)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    if(kiss->buffer_size < 3) 
    {
        return KISS_ERR_BUFFER_OVERFLOW;
    }

    return kiss_encode_and_send(kiss, NULL, 0, KISS_HEADER_PING);

}




int32_t kiss_set_param(kiss_instance_t *const kiss, uint16_t ID, const uint8_t *const param, size_t len)
{
    /* check if kiss instance is null or parameter pointer is null */
    if(NULL == kiss || NULL == param) 
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    /* check if buffer size is large enough */
    if(kiss->buffer_size < 5 + len) 
    {
        return KISS_ERR_BUFFER_OVERFLOW;
    }

    /* error container */
    int32_t err = KISS_OK;
    
    /* ID of the parameter to send as byte array */
    uint8_t id_[2] = {(uint8_t) ID, (uint8_t)(ID >> 8)};
    
    /* encode the parameter ID */
    err = kiss_encode(kiss, id_, 2, KISS_HEADER_SET_PARAM);
    /* check for errors */
    if(err != KISS_OK) 
    {
        return err;
    }  
    
    /* push encode the parameter */
    err = kiss_push_encode(kiss, param, len);
    /* check for errors */
    if(err != KISS_OK) 
    {
        return err;
    }


    /* status is transmitting */
    kiss->Status = KISS_STATUS_TRANSMITTING;

    /* send the frame and return the result */
    return kiss_send_frame(kiss);

}













int32_t kiss_request_param(kiss_instance_t *const kiss, uint16_t ID)
{
    if(NULL == kiss)
    {
        return KISS_ERR_INVALID_PARAMS;
    }

    /* check if buffer size is large enough */
    if(kiss->buffer_size < 5) 
    {
        return KISS_ERR_BUFFER_OVERFLOW;
    }

    /* error container */
    int32_t err = KISS_OK;
    
    /* ID of the parameter to send as byte array */
    uint8_t id_[2] = {(uint8_t) ID, (uint8_t)(ID >> 8)};

    /* encode the parameter ID */
    err = kiss_encode(kiss, id_, 2, KISS_HEADER_REQUEST_PARAM);

    /* check for errors */
    if(err != KISS_OK) 
    {
        return err;
    }
    
    /* status is transmitting */
    kiss->Status = KISS_STATUS_TRANSMITTING;

    /* send the frame and return the result */
    return kiss_send_frame(kiss);    
}



int32_t kiss_send_command(kiss_instance_t *const kiss, uint16_t command)
{
    /* checking if parameters are ok */
    if(NULL == kiss)
    {
        return KISS_ERR_INVALID_PARAMS;
    }
    /* create a byte array from the 2 bytes command */
    uint8_t cmd_b[2] = {(uint8_t) (command), (uint8_t) ((command) >> 8)};

    kiss->Status = KISS_STATUS_TRANSMITTING;

    /* encode and send the command */
    return kiss_encode_and_send(kiss, cmd_b, 2, KISS_HEADER_COMMAND);

}










#ifdef KISS_DEBUG

/* if the debug is active use this function to plot the kiss instance */
void kiss_debug(kiss_instance_t *const kiss)
{
    printf("------- STATUS -------\n");
    printf("Buffer size\t%d\n", kiss->buffer_size);
    printf("Buffer index\t%d\n", kiss->index);
    printf("Buffer TXdelay\t%d (%d ms)\n", kiss->TXdelay, kiss->TXdelay*10);
    printf("kiss status\t");
    switch(kiss->Status)
    {
        case KISS_STATUS_ERROR_STATE:
            printf("KISS_STATUS_ERROR_STATE");
            break;
        case KISS_STATUS_NOTHING:
            printf("KISS_STATUS_NOTHING");
            break;
        case KISS_STATUS_RECEIVED:
            printf("KISS_STATUS_RECEIVED");
            break;
        case KISS_STATUS_RECEIVED_ERROR:
            printf("KISS_STATUS_RECEIVED_ERROR");
            break;
        case KISS_STATUS_RECEIVING:
            printf("KISS_STATUS_RECEIVING");
            break;
        case KISS_STATUS_TRANSMITTED:
            printf("KISS_STATUS_TRANSMITTED");
            break;
        case KISS_STATUS_TRANSMITTING:
            printf("KISS_STATUS_TRANSMITTING");
            break;
        default:
            printf("none");
            break;
    }
    printf("\n");
    
    printf("BUFFER:\n");
    for(int32_t i = 0; i < kiss->index; i++)
    {
        printf("%02X ", kiss->buffer[i]);
    }
    printf("\n");
    printf("----------------------\n");
}


#endif
