#ifndef MODULE_API_OBJECT_H
#define MODULE_API_OBJECT_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"

/* Types. */
typedef void (*FUNC_OBJECT_UPDATE)(void *object);
typedef sint32 (*FUNC_OBJECT_STATUS_UPDATE)(void *object);
typedef void *(*FUNC_OBJECT_RESULT_UPDATE)(void *object);

typedef enum
{
    OBJECT_UPDATE_NOTIFY,
    OBJECT_UPDATE_STATUS
} OBJECT_UPDATE_KIND;

typedef struct OBJECT
{
    struct OBJECT *prev;
    struct OBJECT *next;
    OBJECT_UPDATE_KIND update_kind;

    union
    {
        FUNC_OBJECT_UPDATE update;
        FUNC_OBJECT_STATUS_UPDATE status_update;
    };
} OBJECT;

typedef struct
{
    OBJECT sentinel;
    OBJECT *tail;
} OBJECT_LIST;

extern __declspec(dllexport) OBJECT_LIST g_object_list;
extern OBJECT_LIST g_auxiliary_object_list;
extern OBJECT_LIST g_ledge_trigger_list;

/* BEGIN GENERATED MODULE API */
GDB_CALL void *destroyable_object_damage_and_reward(void *target_val, void *src_val);
GDB_CALL void *object_create(sint32 size, FUNC_OBJECT_UPDATE callback);
GDB_CALL void *object_find_next_by_type(void *after, sint16 object_type);
GDB_CALL void linked_list_initialize(OBJECT_LIST *list);
GDB_CALL void object_destroy(void *object);
GDB_CALL void object_list_update(void);
__declspec(noinline) void *object_destroy_all_by_type(sint16 type);
sint16 object_count_by_type(sint16 object_type);
void object_hit_flash_apply(void *object_val);
void object_embedded_hit_flash_apply(void *object_val);
void linked_list_append(OBJECT_LIST *list, void *object_val);
void linked_list_insert_after(OBJECT_LIST *list, void *object_val, void *after_val);
void linked_list_unlink(OBJECT_LIST *list, void *object_val);
/* END GENERATED MODULE API */

#endif
