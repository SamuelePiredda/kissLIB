#ifndef KISSLIB_H
#endif
#define KISSLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define KISSLIB_VERSION "1.0.0"

#define KISS_FEND 0xC0
#define KISS_FESC 0xDB
#define KISS_TFEND 0xDC
#define KISS_TFESC 0xDD



#define KISS_ERR_INVALID_PARAMS 1
#define KISS_ERR_INVALID_FRAME 2
#define KISS_ERR_BUFFER_OVERFLOW 3


typedef void (*kiss_write_fn)(uint8_t byte);
typedef void (*kiss_read_fn)(uint8_t *data, uint16_t length);


typedef struct 
{
    uint8_t *buffer;
    uint16_t buffer_size;
    uint16_t index;
    kiss_write_fn write;
    kiss_read_fn read;
} kiss_instance_t;


int kiss_init(kiss_instance_t *kiss, uint8_t *buffer, uint16_t buffer_size, kiss_write_fn write, kiss_read_fn read);
int kiss_encode(kiss_instance_t *kiss, const uint8_t *data, uint16_t length);
int kiss_decode(kiss_instance_t *kiss, uint8_t *output, uint16_t *output_length, uint8_t *header);
int kiss_send_frame(kiss_instance_t *kiss);
int kiss_receive_frame(kiss_instance_t *kiss, uint8_t *output, uint16_t *output_length);




#ifdef __cplusplus
}
#endif
