#ifndef RECORD_H
#define RECORD_H

#include "xoshiro256.h"
#include "lzhtable.h"
#include <stdio.h>

typedef enum record_type{
    NONE_RTYPE,
    RANDOM_RTYPE,
    FILE_RTYPE,
}RecordType;

#define FILE_READ_MODE   0b10000000
#define FILE_WRITE_MODE  0b01000000
#define FILE_APPEND_MODE 0b00100000
#define FILE_BINARY_MODE 0b00000010
#define FILE_PLUS_MODE   0b00000001

#define FILE_CAN_READ(_mode) (((_mode) & FILE_READ_MODE) || ((_mode) & FILE_PLUS_MODE))
#define FILE_CAN_WRITE(_mode) (((_mode) & FILE_WRITE_MODE) || ((_mode) & FILE_APPEND_MODE) || ((_mode) & FILE_PLUS_MODE))
#define FILE_CAN_APPEND(_mode) ((_mode) & FILE_APPEND_MODE)
#define FILE_IS_BINARY(_mode)((_mode) & FILE_BINARY_MODE)

typedef struct record{
    RecordType type;

    union{
        XOShiro256 xos256;
        struct {
            char mode;
            char *pathname;
            FILE *handler;
        }file;
    }content;

	LZHTable *attributes;
}Record;

#endif