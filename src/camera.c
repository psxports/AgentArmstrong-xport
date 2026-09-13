#include "app.h"
#include "camera.h"
#include "global.h"
#include "object.h"
#include "player.h"

/* Functions. */
/* Direct fixed-point decompilation of FUN_8009C0C8. */

static sint32 approach(sint32 current, sint32 target, sint32 maximum)
{
    sint32 distance = target - current;
    if (distance > maximum)
        distance = maximum;
    if (distance < -maximum)
        distance = -maximum;
    return current + distance;
}

/* Original: FUN_8009C0C8. */
void camera_update(sint32 player_x, sint32 player_y, sint32 player_z, sint32 mode, sint32 unchanged_direction_frames)
{
    sint32 delta, step, target_x, target_y, target_z;
    uint32 horizontal = mode == 2 ? 1u : mode == 6 ? ~0u : 0u;
    uint32 player_camera_flags = g_player->sampled_buttons;
    sint32 diagonal_target = ((uint32)(mode - 3) < 2 || mode == 5) ? 0x40 : 0;
    (void)unchanged_direction_frames;
    if (g_hq_world_map_active != 0)
        return;
    if (g_camera_depth_offset < diagonal_target + g_camera_depth_offset_target)
        g_camera_depth_offset += 2;
    if (g_camera_depth_offset > diagonal_target + g_camera_depth_offset_target)
        g_camera_depth_offset -= 2;

    delta = player_x - g_previous_player_world_x;
    if (delta == 0)
        delta = (sint32)horizontal;
    target_x = player_x;
    if (horizontal != 0)
    {
        if (delta < 0)
            target_x += g_camera_horizontal_bias - 0x6000;
        if (delta > 0)
            target_x += g_camera_horizontal_bias + 0x6000;
    }
    /* 8009C1B0..8009C1E4: camera X can always advance by 0x200 more
     * than the player's displacement this frame. */
    step = (delta < 0 ? -delta : delta) + 0x200;
    delta = g_camera_world_x - target_x;
    if (delta < 0)
        delta = -delta;
    if (step > delta)
        step = delta;
    g_camera_world_x = approach(g_camera_world_x, target_x, step);
    if (g_previous_camera_world_z == -1)
        g_camera_world_x = player_x;

    target_x = g_camera_world_x;
    if (g_camera_limit_left != -1)
    {
        sint32 boundary = g_camera_limit_left + g_screen_half_width * 0x100 - 0x4000;
        if (g_camera_world_x < boundary)
        {
            target_x = boundary;
            if (g_previous_camera_world_z != -1)
            {
                delta = g_camera_world_x - boundary;
                if (delta < 0)
                    delta = -delta;
                target_x = g_camera_world_x + (delta > 0x800 ? 0x800 : delta);
            }
        }
    }
    g_camera_world_x = target_x;
    target_x = g_camera_world_x;
    if (g_camera_limit_right != -1)
    {
        sint32 boundary = g_camera_limit_right - g_screen_half_width * 0x100 + 0x4000;
        if (boundary < g_camera_world_x)
        {
            target_x = boundary;
            if (g_previous_camera_world_z != -1)
            {
                delta = g_camera_world_x - boundary;
                if (delta < 0)
                    delta = -delta;
                target_x = g_camera_world_x - (delta > 0x800 ? 0x800 : delta);
            }
        }
    }
    g_camera_world_x = target_x;

    target_z = player_z - 0x15e00 - g_camera_depth_offset * 0x100;
    target_y = player_y - 0x7800;
    if (g_previous_camera_world_z != -1)
    {
        if (target_z < g_camera_limit_near)
            target_z = g_camera_limit_near;
        if (target_z > g_camera_limit_far)
            target_z = g_camera_limit_far;
        g_camera_world_z = approach(g_camera_world_z, target_z, 0x500);
        player_z = (g_map_depth_cells * 0xc0 - g_map_depth_subcells) * 0x100;
        if (g_camera_world_z < player_z)
            g_camera_world_z = player_z;
        target_y = g_camera_target_y;
    }
    else
    {
        g_camera_world_z = target_z;
    }
    g_camera_target_y = target_y;

    target_y = player_y - (g_scuba_stage_active != 0 ? 0x6000 : 0x8000) + g_camera_manual_y_offset;
    /* Original FUN_8009C0C8 uses two one-sided constraints, forming a
     * 0x1c00-tall vertical dead zone.  A bidirectional approach here made the
     * camera follow sub-pixel MAPFLOOR height changes and flicker by a pixel. */
    if (g_camera_target_y < target_y)
        g_camera_target_y = approach(g_camera_target_y, target_y, 0x300);
    g_previous_player_world_y = target_y + 0x1c00;
    if (g_camera_target_y > g_previous_player_world_y)
        g_camera_target_y = approach(g_camera_target_y, g_previous_player_world_y, 0x300);

    if (g_camera_limit_bottom != -1)
    {
        if (g_previous_camera_world_z == -1)
            g_camera_target_y = g_camera_limit_bottom;
        else if (g_camera_limit_bottom < g_camera_target_y)
            g_camera_target_y = approach(g_camera_target_y, g_camera_limit_bottom, 0x800);
    }
    if (g_camera_limit_top != -1)
    {
        if (g_previous_camera_world_z == -1)
            g_camera_target_y = g_camera_limit_top;
        else if (g_camera_target_y < g_camera_limit_top)
            g_camera_target_y = approach(g_camera_target_y, g_camera_limit_top, 0x800);
    }

    if ((player_camera_flags & 10) == 0)
    {
        if (g_camera_manual_y_offset > 0)
            g_camera_manual_y_offset -= 0x800;
        if (g_camera_manual_y_offset < 0)
            g_camera_manual_y_offset += 0x800;
    }
    else
    {
        if (player_camera_flags & 8)
            g_camera_manual_y_offset -= 0x800;
        if (player_camera_flags & 2)
            g_camera_manual_y_offset += 0x800;
        if (g_camera_manual_y_offset > 0x6000)
            g_camera_manual_y_offset = 0x6000;
        if (g_camera_manual_y_offset < -0x4000)
            g_camera_manual_y_offset = -0x4000;
    }
    if (g_previous_camera_world_z == -1)
    {
        g_previous_camera_world_x = g_camera_world_x;
        g_previous_camera_world_y = g_camera_target_y;
        g_previous_camera_world_z = g_camera_world_z;
    }
    g_camera_world_y = g_camera_target_y;
    g_camera_render_x = g_camera_world_x < 0 ? (g_camera_world_x + 0xff) >> 8 : g_camera_world_x >> 8;
    g_camera_render_y = g_camera_world_y < 0 ? (g_camera_world_y + 0xff) >> 8 : g_camera_world_y >> 8;
    g_camera_render_z = g_camera_world_z < 0 ? (g_camera_world_z + 0xff) >> 8 : g_camera_world_z >> 8;
    g_previous_player_world_z = player_z;
    g_previous_player_world_x = player_x;
    /* 8009C704: the final delay-slot store preserves the player's current Y
     * for the next frame's camera/effect calculations. */
    g_previous_player_world_y = player_y;
}

/* Direct FUN_800901B8. */
void camera_shake_start(void)
{
    g_display_shake_ticks = 0x2c;
}

/* Direct translation of 0x800A8264..0x800A8298. */
/* Original: FUN_800A8264. */
void camera_depth_limit_set(EFFECT *effect)
{
    if ((sint16)effect->values[0] == 0)
        g_player_far_z_limit = effect->z;
    else
        g_player_near_z_limit = effect->z;
}
