#ifndef MAP_RENDER_H
#define MAP_RENDER_H

#include "map.h"

typedef struct MAP_RENDER_STATE
{
    sint32 object_shade;
    sint32 depth_anchor_z, back_scale, front_scale, depth_light_bias;
    sint32 background_brightness, brightness, far_z, vertical_extent, effect_depth;
    sint32 spr_subdiv, background_disabled, background_y;
    sint16 background_resource, foreground_clut;
} MAP_RENDER_STATE;

extern MAP_RENDER_STATE g_map_render;
void render_map_scene(void);

#endif
