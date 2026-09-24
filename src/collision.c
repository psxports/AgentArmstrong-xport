#include "collision.h"
#include "global.h"

/* Functions. */
/* Object AABB collision dispatcher, original FUN_80098588. */

static sint32 ranges_overlap(sint32 a, sint32 a_size, sint32 b, sint32 b_size)
{
    return b <= a + a_size && a <= b + b_size;
}

void object_collisions_dispatch(void)
{
    COLLISION *source;
    COLLISION *target;

    for (source = g_object_list[0]->next; source != 0; source = source->next)
    {
        sint32 source_x, source_y, source_z;
        if (source->sends_mask == 0)
            continue;

        source_x = source->x + source->box_x;
        source_y = source->y + source->box_y;
        source_z = source->z + source->box_z;

        for (target = g_object_list[0]->next; target != 0; target = target->next)
        {
            sint32 target_x, target_y, target_z;
            if ((source->sends_mask & target->receives_mask) == 0)
                continue;

            target_x = target->x + target->box_x;
            target_y = target->y + target->box_y;
            target_z = target->z + target->box_z;
            if (!ranges_overlap(source_x, source->width, target_x, target->width) || !ranges_overlap(source_y, source->height, target_y, target->height) || !ranges_overlap(source_z, source->depth, target_z, target->depth))
                continue;

            if ((source->sends_mask & 2) != 0)
                target->callback_14(target, source);
            if ((source->sends_mask & 0xc1) != 0)
                target->callback_18(target, source);
        }
    }
}

/* Original: FUN_8009875C. */
GDB_CALL void collision_box_set(COLLISION *object, sint32 width, sint32 height, sint32 depth)
{
    object->width = width << 8;
    object->height = height << 8;
    object->depth = depth << 8;
    object->box_x = -(width / 2) * 0x100;
    object->box_y = -height * 0x100;
    object->box_z = -(depth / 2) * 0x100;
}

/* Direct FUN_800987B4 damage/impact counter callback. */
sint16 object_damage_apply(FLASHABLE *target, FLASHABLE *source)
{
    sint16 value = (sint16)((uint16)target->field_78 - (uint16)source->field_78);
    sint16 type = source->collision.object_type;
    target->field_78 = value;
    if (type == 10 || type == 0x64)
    {
        target->flash_clut_ticks = 1;
        source->field_78 = 0;
    }
    else
    {
        target->flash_clut_ticks = (sint16)(g_frame_counter & 1);
        source->collision.receives_mask |= 0x20;
    }
    return value < 1;
}
