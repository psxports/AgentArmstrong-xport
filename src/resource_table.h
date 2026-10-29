#ifndef RESOURCE_TABLE_H
#define RESOURCE_TABLE_H

#include "xport.h"

/* Original C87C8 bank table ends at C8BC8; IDs retain ten frame bits */
enum
{
    RESOURCE_SLOT_COUNT = 256
};

typedef struct
{
    uint8 *data;
    uint32 size;
} RESOURCE_DATA;

typedef enum
{
    RESOURCE_BORROW,
    RESOURCE_COPY
} RESOURCE_STORAGE;

const RESOURCE_DATA *resource_table_data(uint32 resource_id);
/* A non-null frame includes its complete four-byte dimensions/pivot header */
uint8 *animation_frame_resource(uint32 resource_id);
void resource_table_register(uint32 resource_id, void *data, uint32 size, RESOURCE_STORAGE storage);

#endif
