#ifndef MODULE_API_OBJECT_H
#define MODULE_API_OBJECT_H

#include <stddef.h>

#include "app.h"
#include "collision.h"

/* Types. */
typedef struct
{
    void *previous;               /* +0x00 */
    void *next;                   /* +0x04 */
    uint32 field_08;              /* +0x08 */
    FUNC_COLLISION_UPDATE update; /* +0x0C */
} OBJECT;

#if defined(AP_32BIT)
typedef char ObjectNode_size_10[sizeof(OBJECT) == 0x10 ? 1 : -1];
typedef char ObjectNode_update_at_0c[offsetof(OBJECT, update) == 0x0c ? 1 : -1];
#endif

/* BEGIN GENERATED MODULE API */
GDB_CALL void *destroyable_object_damage_and_reward(COLLISION *target, COLLISION *source);
GDB_CALL void *object_create(sint32 size, FUNC_COLLISION_UPDATE callback);
GDB_CALL void *object_find_next_by_type(void *after, sint16 object_type);
GDB_CALL void linked_list_initialize(void *anchor_value);
GDB_CALL void object_destroy(void *object);
GDB_CALL void object_list_update(void);
__declspec(noinline) void *object_destroy_all_by_type(sint16 type);
sint16 object_count_by_type(sint16 object_type);
void object_hit_flash_apply(FLASHABLE *object);
void linked_list_append(void *anchor_value, void *object_value);
void linked_list_insert_after(void *anchor_value, OBJECT *object, OBJECT *after);
void linked_list_unlink(void *anchor_value, OBJECT *object);
/* END GENERATED MODULE API */

#endif
