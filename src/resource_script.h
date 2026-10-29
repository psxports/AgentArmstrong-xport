#ifndef RESOURCE_SCRIPT_H
#define RESOURCE_SCRIPT_H

#include "xport.h"
#include "psx.h"
#include "model.h"

typedef enum
{
    RESOURCE_END = -1,
    RESOURCE_ARCHIVE = -2,
    RESOURCE_FILE = -3,
    RESOURCE_TEXTURE = -4,
    RESOURCE_INCLUDE = -5,
    RESOURCE_MODELS = -6,
    RESOURCE_MATERIALS = -7,
    RESOURCE_MODEL_ARCHIVE = -8,
    RESOURCE_OVERLAY = -9,
    RESOURCE_RANGE = 0
} RESOURCE_OP;

typedef struct RESOURCE_COMMAND RESOURCE_COMMAND;

struct RESOURCE_COMMAND
{
    RESOURCE_OP op;
    const char *path;
    const RESOURCE_COMMAND *nested;
    sint16 *model_base;
    sint16 model_count;
    sint32 material[3];
    MODEL_POSE_RESOURCE *pose;
    uint32 resource;
    sint32 first, last;
};

/* A null script performs no work */
GDB_CALL void resource_script_execute(const RESOURCE_COMMAND *script);

#endif
