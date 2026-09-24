#include "collision.h"
#include "effect_update.h"
#include "global.h"
#include "level_data.h"
#include "map.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "render.h"
#include "text_renderer.h"

/* Variables. */
/* Original: SLES_004.74 DAT_800CBB08, 46 resource IDs indexed by TEXINFO bytes.
 * Former native duplicates: hq_material_resources and hq_tx_resource. */
const uint32 g_hq_material_resources[46] = {0x06800, 0x06c00, 0x07000, 0x07400, 0x07800, 0x07c00, 0x08000, 0x29c00, 0x2a000, 0x2a400, 0x2a800, 0x2ac00, 0x2b000, 0x2b400, 0x18c00, 0x19000, 0x19400, 0x19800, 0x19c00, 0x1a000, 0x1a400, 0x1a800, 0x1ac00, 0x1b000, 0x1b400, 0x1b800, 0x1bc00, 0x1c000, 0x1fc00, 0x20000, 0x20400, 0x20800, 0x20c00, 0x21000, 0x21400, 0x21800, 0x21c00, 0x22000, 0x22400, 0x22800, 0x01400, 0x01800, 0x01c00, 0x2f000, 0x12800, 0x12c00};

/* Central storage for shared recovered game state.  Module-private and host
 * platform state remains static in its owning semantic module. */

char g_fmvSeen;

sint32 g_iterations;

/* Original: DAT_800D4010. */
OriginalModelDescriptor *g_model_descriptors;

sint32 g_skipPcinit = 1;

sint32 g_titleStrToggle;

/* Original: DAT_800CEBCC; previous-value register written by FUN_800B2058. */
sint32 g_video_mode;

/* Originals: DAT_800D3C98, DAT_800D3C9C, DAT_800D3C94. */
sint32 g_language_id = -1;

/* Originals: DAT_800D3CB8, DAT_800D3CBC. */
sint32 g_play_fmv = 1;

/* .sdata:800D3CC0 = 0x000010FA in SLES_004.74. */
sint32 g_default_model_scale = 0x10fa;

/* Original: DAT_800E7F68. Four-entry recent speech-sample ring. */
sint16 g_recent_speech_samples[4];

/* Originals: DAT_800D3D21, DAT_800D3D20. */
uint8 g_weapon_cheat_enabled;

uint8 g_invulnerability_cheat_enabled;

/* Original: DAT_800E7B5A. Objective-counter aliases live in global.h. */
sint16 g_previous_destruction_sound_variant;

/* Original: DAT_800D3DB0. */
uint16 g_hq_scroll_frame_divider;

/* Original: DAT_800D3DA2. */
sint16 g_hq_map_region = 1;

void *g_v2_rocket_initial_pose;

/* Original: DAT_800D3E68. */
/* Original: DAT_800D3E78. */
MISSION_TIMER_DISPLAY *g_mission_timer_display;

void *g_stage_archive;

/* Original: DAT_800D3EA4. */
void *g_runtime_scratch_buffer_a;

void *g_menu_text_pointer_table;

void *g_gunship_initial_pose;

/* Original: DAT_800D3EC0. */
uint32 g_previous_held_buttons;

void *g_model_render_frame;

void *g_input_record_cursor;

/* Original: DAT_800D3F64. */
uint8 g_terrain_variation_limit = 0x1f;

/* Original: DAT_800D3F80. */
FrameObjectPartial *g_next_frame_object;

/* Originals: DAT_800D3FB8, DAT_800D3FAC, DAT_800D3E48, DAT_800D3E44. */
void *g_mini_gyro_initial_pose;

sint16 g_mini_gyro_model_base;

void *g_mini_gyro_type_117_initial_pose;

sint16 g_mini_gyro_type_117_model_base;

/* Originals: DAT_800D4004, DAT_800D4000. */
void *g_overlay_initial_pose_slot_4004;

sint16 g_overlay_model_base_slot_4000;

char g_pause_button_latched;

STAGE_ENV *g_stage_env;

/* FUN_800AAEC8 builds this 0x3f-entry surface/material table from
 * TEXINFO.BIN and the executable table at 0x800CBB08. */
/* 0x800EA198..0x800EA657, ending immediately before DAT_800EA658.  MAPFLOOR
 * indexes this workspace with an unsigned byte, so entries through [255]
 * are architecturally reachable even when TEXINFO initializes fewer than 64. */
/* Original: DAT_800EA198. */
sint32 g_material_resource_table[304];

/* Original: DAT_800D4024. */
/* Original: DAT_800D4028. */
/* Original: DAT_800D402C. */
RENDER_FRAME *g_current_render_frame;

void *g_scuba_tug_initial_pose;

void *g_tank_initial_pose;

/* Original: DAT_800D3F7C. */
sint16 g_scuba_drifting_hazard_model_id;

void *DAT_800d3f84;

/* Original: DAT_800D3DD8. */
void *g_player_fire_script;

/* Original: DAT_800D405C. Removed unused DAT_800d405c_placeholder storage. */
uint8 *g_gpu_packet_cursor;

/* Original: DAT_800D411C. */
COLLISION_RESULT *g_current_collision_result;

/* Originals: DAT_800D4090, DAT_800D40B8. */
uint32 *g_prim_link_cursor;

void *g_auxiliary_object_list[2];

/* Original: DAT_800D40F0. */
void *g_overlay_initial_pose_slot_40f0;

uint32 g_frame_counter;

void *g_input_record_buffer;

/* Original: DAT_800D41DC. */
uint32 g_current_sprite_upload_resource;

void *g_tanya_initial_pose;

void *g_runtime_scratch_buffer_b;

/* 0x800D4218 is an eight-byte list anchor {head, tail}, not a scalar. */
/* Original: DAT_800D4228. */
/* Original: DAT_800D4218. */
void *g_ledge_trigger_list[2];

/* Original: DAT_800D4220. */
void *g_loading_image_buffer;

/* Original: DAT_800D4436. */
sint16 g_projectile_effect_cooldown;

/* Originals: DAT_800D3FA0, DAT_800D4158. */
sint16 g_red_signal_flare_clut;

sint16 g_white_signal_flare_clut;

/* Originals: DAT_800D4150, DAT_800D406C. */
sint16 g_truck_model_base;

sint16 g_gyro_model_base;

/* Original: DAT_800D3DDC. */
sint32 g_continuous_weapon_effect_rotation;

/* Original: DAT_800D40DC. */
MAP_FLOOR_CELL *g_map_floor_cells;

/* Original: DAT_800D41B8. */
MAP_MODEL_GROUP *g_map_model_groups;

/* Original: DAT_800D3FF0. */
EFFECT_DATA *g_effect_data;

/* Original: DAT_800D3FA8. */
void *g_map_model_attributes;

/* Originals: DAT_800D4D80, DAT_800D4D58, DAT_800D4DA8. */
POLY_FT4 g_player_prone_weapon_effect_prim;

POLY_FT4 g_player_weapon_effect_prim;

POLY_FT4 g_continuous_weapon_effect_prim;

/* Persistent collision work records used by FUN_80096EA0/FUN_80097128.
 * The original executable places the dynamic result at 0x800D4D30, the
 * static-map result at 0x800D4D44, and the two 32-entry touched-cell arrays
 * at 0x800E7F80/0x800E8000. */
/* Originals: DAT_800D4D30, DAT_800D4D44, DAT_800E7F80, DAT_800E8000. */
COLLISION_RESULT g_dynamic_collision_result;

COLLISION_RESULT g_static_collision_result;

sint32 g_dynamic_collision_cells[32];

sint32 g_static_collision_cells[32];

/* Original: DAT_800D3D22. */
uint8 g_level_select_cheat_enabled;

/* Original: DAT_800D3CF2. */
uint16 g_render_frame_index;

/* Original: DAT_800D3CE4. */
char g_input_playback_label[64];

/* Original: DAT_800E2DC8. */
sint32 g_vsync_count_this_frame;

/* Original: DAT_800FEAA4. */
uint8 g_hq_weapon_area_enabled;

/* Native pad packet used by FUN_800AEED4 */
/* Original: DAT_800E33D0. */
uint8 g_controller_packet[8];

/* Original: DAT_800E3398. */
sint32 g_vsync_count_total;

/* Originals: DAT_800E7928, DAT_800EBEF8, DAT_800EA170, DAT_800EA658. */
uint8 g_primary_objective_states[257];

uint8 g_secondary_objective_states[257];

uint8 g_action_trigger_states[32];

uint8 g_mission_message_slots[16];

/* Original: DAT_800E7F40. */
uint8 g_objective_counts[256];

/* Original: DAT_800D74D8. */
uint8 g_trigger_states[128];

static SpatialBucket spatial_bucket_storage[64];

static uint8 spatial_cell_map[65536];

static PLAYER player_state_storage;

/* Original: DAT_800D4174. */
SpatialBucket *g_spatial_buckets = spatial_bucket_storage;

/* Original: DAT_800D3E5C. */
uint8 *g_spatial_cell_bucket_indices = spatial_cell_map;

/* Original: DAT_800D3E70. */
uint8 *g_dynamic_collision_cell_anchors;

/* Original: DAT_800D3F74. */
uint8 *g_map_cell_light_values;

/* Original: DAT_800D4154. */
PLAYER *g_player = &player_state_storage;

/* Original: DAT_800E89F0. */
FrameObjectPartial g_frame_objects[FRAME_OBJECT_LIMIT];

static FrameObjectPartial *frame_object_lookup[FRAME_OBJECT_LIMIT + 1];

/* Original: DAT_800D3F60. */
FrameObjectPartial **g_frame_object_lookup = frame_object_lookup;

/* Original: DAT_800D3F24. */
sint32 g_map_width_cells = 1;

static EFFECT effect_records[256];

/* Original: DAT_800D4210. */
EFFECT *g_effects = effect_records;

/* Original: DAT_800CAFB4. */
EFFECT_HANDLER_ENTRY g_effect_handlers[256];

/* Original: DAT_800EDBF0. */
uint32 g_prim_link_workspace[0x2000];

/* Original: DAT_800EDCB8. */
RENDER_FRAME g_render_frame_buffer_0;

/* Original: DAT_800E5520. */
uint8 g_boss_health_bar[0x64];

/* Original: DAT_800D75D8. */
uint16 g_resource_clut_table[256];

/* Original: DAT_800D40D4. */
sint32 g_font_glyph_height;

/* Original: DAT_800D4178. */
FONT *g_font;

/* Originals: DAT_800D3ED0, DAT_800D3FE4, DAT_800D3F20. */
sint16 g_font_line_height, g_font_clut, g_font_glyph_columns;

/* Original: DAT_800D3CF0. */
sint16 g_text_color_index;

/* Memory-card directory ordering table is declared near the other port globals. */
/* Original: DAT_800D3F6C. */
sint32 g_text_pen_y;

/* Originals: DAT_800EA6C8, DAT_800EA6CC, DAT_800EA6CD, DAT_800EA6CE. */
POLY_FT4 g_shared_quad_prim;

/* Original: DAT_800ED8B8. */
POLY_FT3 g_shared_triangle_prim;

/* Original: DAT_800EA670. */
uint8 g_player_health_bar[128];

/* Original five-u16 VRAM-region records from 800C8260/800C8334. */
/* Original: DAT_800C8260. */
uint16 g_sprite_vram_regions_game[] = {0x140, 0, 0x100, 0xc0, 0, 0x180, 0, 0x100, 0xc0, 0, 0x1c0, 0, 0x100, 0xc0, 0, 0x200, 0, 0x100, 0xc0, 0, 0x240, 0, 0x100, 0x100, 0, 0x280, 0, 0x100, 0x100, 0, 0x2c0, 0, 0x100, 0x100, 0, 0x300, 0, 0x100, 0x100, 0, 0x340, 0, 0x100, 0x100, 0, 0x380, 0, 0x100, 0x100, 0, 0x3c0, 0, 0x100, 0x100, 0, 0x140, 0x100, 0x100, 0x100, 0, 0x180, 0x100, 0x100, 0x100, 0, 0x1c0, 0x100, 0x100, 0x100, 0, 0x200, 0x100, 0x100, 0x100, 0, 0x240, 0x100, 0x100, 0x100, 1, 0x2c0, 0x100, 0x100, 0x100, 1, 0x340, 0x100, 0x100, 0x100, 0, 0x380, 0x100, 0x100, 0x100, 0, 0x3c0, 0x100, 0x100, 0x100, 0, 0xffff};

/* Original: DAT_800C8334. */
uint16 g_sprite_vram_regions_hq[] = {0x140, 0, 0x100, 0xc0, 0, 0x180, 0, 0x100, 0xc0, 0, 0x1c0, 0, 0x100, 0xc0, 0, 0x200, 0, 0x100, 0xc0, 0, 0x240, 0, 0x100, 0x100, 0, 0x280, 0, 0x100, 0x100, 0, 0x2c0, 0, 0x100, 0x100, 0, 0x300, 0, 0x100, 0x100, 0, 0x340, 0, 0x100, 0x100, 0, 0x380, 0, 0x100, 0x100, 0, 0x3c0, 0, 0x100, 0x100, 0, 0x140, 0x100, 0x100, 0x100, 1, 0x1c0, 0x100, 0x100, 0x100, 0, 0x200, 0x100, 0x100, 0x100, 0, 0x240, 0x100, 0x100, 0x100, 1, 0x2c0, 0x100, 0x100, 0x100, 1, 0x340, 0x100, 0x100, 0x100, 0, 0x380, 0x100, 0x100, 0x100, 0, 0x3c0, 0x100, 0x100, 0x100, 0, 0xffff};

/* 0x800C8E0C..0x800C8E25: the exact 3x3 dynamic-light MapStamp consumed by
 * FUN_80095190.  This used to be an all-zero placeholder, so width/height
 * were both zero and muzzle flashes could never touch DAT_800D3F74. */
/* Original: DAT_800C8E0C. */
uint8 g_muzzle_flash_light_stamp[26] = {0xff, 0xff, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x0a, 0x00, 0x0f, 0x00, 0x0a, 0x00, 0x0f, 0x00, 0x14, 0x00, 0x0f, 0x00, 0x0a, 0x00, 0x0f, 0x00, 0x0a, 0x00};

void *DAT_800c8ac4;

/* Original: DAT_80082F71. Exact executable bytes: 13 43 41 52 47 41 4E 44 4F 00. */
const char g_loading_caption_spanish[] = "\x13"
                                         "CARGANDO";

/* Functions. */
/* Original HQ.BIN: DAT_800FE996. */
__declspec(dllexport) sint16 g_hq_session_timer;

/* Originals: DAT_800D2DFC, DAT_800D2E00, DAT_800D3110. */
__declspec(dllexport) CD_CALLBACK g_cd_ready_callback;

__declspec(dllexport) CD_CALLBACK g_cd_sync_callback;

__declspec(dllexport) CD_CALLBACK g_cd_data_callback;

/* Original: DAT_800E8120. */
__declspec(dllexport) uint8 g_cd_track_table[4];

/* Original: DAT_800E2DF0. Memory-card directory-to-grid mapping. */
__declspec(dllexport) uint8 g_memory_card_entry_map[256];

__declspec(dllexport) uint8 DAT_80000000;

sint32 g_debug_cheats_enabled;

sint32 g_debug_coordinates_visible;

/* Original: DAT_800D3CF4. */
sint32 g_vram_error_reporting_enabled;

/* Original: DAT_800D3CF8. Screenshot sequence number used by FUN_80090090. */
sint32 g_screenshot_index;

/* Original: DAT_800D3C90; value 1 selects the CD-ROM path in PAL. */
sint32 g_cd_file_io_enabled = 1;

/* Original: DAT_800D3CD8. */
sint32 g_pause_active;

/* Originals: DAT_800D3CAC, DAT_800D3CB0. */
sint32 g_stage_index;

sint32 g_next_stage_index;

sint32 g_hq_attract_timeout;

/* Originals: DAT_800D3CC4, DAT_800D3CC6, DAT_800D3CC0,
 * DAT_800D3CC8, DAT_800D3CCA. */
sint16 g_player_input_disabled;

sint16 g_target_indicator_enabled;

sint16 g_stage_background_resource;

sint16 g_foreground_sprite_clut_offset;

sint16 DAT_800d3ccc;

/* Originals: DAT_800D3D14, DAT_800D3D18, DAT_800D3D16, DAT_800D3D1A. */
sint16 g_attract_demo_index;

sint16 g_attract_demo_variant;

sint16 g_attract_mode;

sint16 g_next_attract_mode;

/* Originals: DAT_800D3D24, DAT_800D3D28. */
sint32 g_player_world_x;

sint32 g_player_world_y;

/* Original: DAT_800D3CDC. */
sint32 g_menu_selection_index;

/* Originals: DAT_800D3D78, DAT_800D3D7C, DAT_800D3D80,
 * DAT_800D3D84, DAT_800D3D88. */
sint32 g_sfx_playback_volume = 0x7f;

sint32 g_sfx_volume_setting = 0x7f;

sint32 g_music_volume_setting = 0x7f;

sint32 g_current_music_track = -1;

sint32 g_cd_audio_track_offset;

/* Originals: DAT_800D3D2C, DAT_800D3D30. */
sint32 g_player_world_z;

sint32 g_muzzle_light_pending;

/* Original: DAT_800D3D0C. */
sint16 g_model_clut_override;

/* Originals: DAT_800D3D34, DAT_800D3D38. */
sint32 g_muzzle_light_x_direction;

sint32 g_muzzle_light_z_direction;

/* Originals: DAT_800D3D3C, DAT_800D3D40, DAT_800D3D44. */
sint32 g_saved_player_world_x;

sint32 g_saved_player_world_y;

sint32 g_saved_player_world_z;

/* Original: DAT_800D3D9C. */
sint32 g_sound_handles_invalidated;

/* Originals: DAT_800D3D74, DAT_800D3D76. */
sint16 DAT_800d3d70;

sint16 DAT_800d3d72;

sint16 g_recent_speech_write_index;

sint16 g_ambient_speech_timer;

/* Original: DAT_800D3DD0. */
__declspec(dllexport) sint16 g_loading_image_index;

/* Originals: DAT_800D3D48, DAT_800D3DA4. */
sint32 g_camera_depth_offset_target;

sint32 g_scuba_stage_active;

/* Originals: DAT_800D3D68, DAT_800D4164. CD statistics at gp+0xE0 and gp+0x4DC. */
sint32 g_cd_sectors_read;

sint32 g_cd_read_command_count;

/* Original: DAT_800D3DB4. */
sint32 g_memory_card_warning_checked;

/* Original: DAT_800D3E40. */
sint32 g_stage_background_y;

/* Original: DAT_800D3DA0. */
sint16 g_hq_selected_map_node;

/* Original: DAT_800D3E58. */
/* Originals: DAT_800D3E4C, DAT_800D3E50. */
sint32 g_mips_frame_stack_pointer;

sint32 g_camera_manual_y_offset;

sint32 g_render_row_left_world_x;

/* Originals: DAT_800D3E54, DAT_800D3E60. */
sint16 g_v2_rocket_model_base;

sint32 g_same_stage_selected;

/* Original: DAT_800D3E6C. Former native duplicate: g_screenH. */
sint32 g_screen_height;

/* Original: DAT_800D3E8C. */
/* Original: DAT_800D3E90. */
sint16 g_display_shake_ticks;

/* Originals: DAT_800D3E74, DAT_800D3E80. */
sint16 g_clut_upload_x;

sint16 g_clut_upload_y;

/* Original: DAT_800D3E98. */
/* Original: DAT_800D3E94. */
sint32 g_primary_target_current_count;

sint32 g_previous_player_world_x;

/* Originals: DAT_800D3E9C, DAT_800D3EA0. */
sint32 g_previous_player_world_y;

sint32 g_previous_player_world_z;

/* Originals: DAT_800D3EAC, DAT_800D3EB4, DAT_800D3EB8. */
sint32 g_camera_world_x;

sint32 g_camera_world_y;

/* Original: DAT_800D3EBC. */
sint32 g_camera_world_z;

/* Originals: DAT_800D3ED2, DAT_800D3ED8. */
sint16 g_gunship_model_base;

/* Original: DAT_800D3EEC. */
/* Originals: DAT_800D3EE8, DAT_800D3EF0, DAT_800D3EF4, DAT_800D3EF8. */
sint32 g_background_brightness_bias;

sint32 g_scene_brightness_bias;

sint16 g_object_type_0x39_model_id;

sint32 g_secondary_target_current_count;

/* Originals: DAT_800D3F00, DAT_800D3F04, DAT_800D3F08. */
sint32 g_current_model_world_x;

sint32 g_current_model_world_y;

/* Original: DAT_800D3F0C. */
sint32 g_current_model_world_z;

sint16 g_target_indicator_model_id;

/* Original: DAT_800D3F2C. */
sint32 g_current_model_shade;

/* Original: DAT_800D3FEC. */
sint32 g_effect_wind_x;

/* Originals: DAT_800D3F28, DAT_800D3F38. */
sint32 g_current_model_rotation_x;

sint32 g_current_model_rotation_z;

/* Original: DAT_800D3F1C. */
sint32 g_room_activation_pending;

/* Originals: DAT_800D3F30, DAT_800D3F34, DAT_800D3F3C. */
sint32 g_camera_render_x;

sint32 g_camera_render_y;

sint32 g_camera_render_z;

/* Originals: DAT_800D3F40, DAT_800D3F44. */
sint32 g_shared_scratch_value;

sint32 g_mission_enemy_update_count;

/* Originals: DAT_800D3F50, DAT_800D3F58. */
/* Original: DAT_800D3F54. */
sint32 g_input_recording_mode;

sint32 g_primary_action_objective_present;

/* Original: DAT_800D3F70. */
__declspec(dllexport) sint32 g_cd_track_table_status;

/* Original: DAT_800D3F78. */
/* Original: DAT_800D3F5C. */
sint32 g_stage_background_disabled;

sint32 g_input_run_length;

/* Original: DAT_800D3F8C. */
/* Original: DAT_800D3F90. */
sint32 g_map_depth_cells;

sint32 g_screen_half_width;

/* Original: DAT_800D3F98. */

/* Original: DAT_800D3FCC. */
sint32 g_hq_world_map_active;

/* Originals: DAT_800D3FA4, DAT_800D3FB0. */
sint32 g_mission_timer_visible;

sint32 g_selected_weapon_slot;

/* Original: DAT_800D3FC8. */
/* Original: DAT_800D3FB4. */
sint32 g_effect_visibility_depth_cells;

sint32 g_pressed_buttons;

/* Original: DAT_800D3FC0. Size of the most recent CC member found by FUN_800A6ABC. */
sint32 g_archive_member_size;

/* Original: DAT_800D3FD2. */
/* Original: DAT_800D3FE0. */
sint16 g_display_screen_y;

/* Original: DAT_800D3FDC. */
sint16 g_model_packet_count;

/* Originals: DAT_800D3FFC, DAT_800D401C. */
sint32 g_vertical_cull_extent;

sint32 g_transient_prim_cursor;

/* Original: DAT_800D400C. */
sint16 g_player_sprite_clut;

/* Original: DAT_800D3FF4. */
sint32 g_collision_surface_resource;

sint32 g_primary_objective_count;

__declspec(dllexport) void *g_shared_result_pointer;

/* Host diagnostic counter; formerly exported as g_psx_flush_cache_boundary_calls. */
__declspec(dllexport) volatile uint32 g_psx_flush_cache_boundary_calls;

/* Original: DAT_800D3F4C. */
__declspec(dllexport) sint32 g_language_assets_preloaded;

/* Original: DAT_800D422C. */
__declspec(dllexport) void *g_sound_archive;

/* Originals: DAT_800D4030, DAT_800D4038. */
sint16 g_scuba_tug_model_base;

/* Originals: DAT_800D404E, DAT_800D4054. */
sint16 g_tank_model_base;

/* Originals: DAT_800D4058, DAT_800D3E88. */
sint32 g_collision_flags;

sint32 g_dynamic_collision_hit;

/* Original: DAT_800D4068. */
sint32 g_current_model_id;

sint32 DAT_800d4018;

/* Original: DAT_800D404C. */
sint16 g_robot_press_model_id;

/* Original: DAT_800D407C. */
/* Original: DAT_800D4060. */
sint32 g_font_glyph_width;

__declspec(dllexport) uint32 g_held_buttons;

/* Original: DAT_800D4078. */
sint16 g_model_count;

/* Original: DAT_800D4074. */
sint32 g_render_depth_bucket;

/* Originals: DAT_800D4080, DAT_800D40B0. */
sint32 g_depth_lighting_bias;

sint32 g_sprite_texture_cache_key;

/* Original: DAT_800D408C. */
sint32 g_render_row_back_projection_scale;

sint32 g_player_far_z_limit;

/* Original: DAT_800D40A4. */
sint32 g_action_transition_direction;

/* Originals: DAT_800D40BE, DAT_800D40C0, DAT_800D40C8. */
sint16 g_surface_impact_cooldown;

sint32 g_render_row_world_z;

/* Original: DAT_800D40D0. */
/* Original: DAT_800D40E0. */
sint32 g_mission_script;

sint32 g_mission_ending;

/* Original: DAT_800D40E4. */
sint16 g_radar_vehicle_model_base;

/* Original: DAT_800D40A8. */
sint32 g_sprite_subdivision_cooldown;

/* Original: DAT_800D40BC. */
sint16 g_bio_goo_model_id;

/* Originals: DAT_800D40E8, DAT_800D40EC. */
sint32 g_room_player_near_z;

sint32 g_room_player_far_z;

/* Originals: DAT_800D40F4, DAT_800D40FC. */
sint32 g_previous_camera_world_x;

sint32 g_previous_camera_world_y;

/* Originals: DAT_800D4108, DAT_800D410C. */
sint32 g_previous_camera_world_z;

sint32 g_scene_far_z;

/* Originals: DAT_800D4104, DAT_800D4118. */
sint32 g_current_model_translation_x;

sint32 g_current_model_translation_z;

/* Original: DAT_800D4110. */
/* Original: DAT_800D414C. */
sint32 g_secondary_objective_count;

sint32 g_primary_target_total_count;

/* Originals: DAT_800D4120, DAT_800D4124, DAT_800D4128, DAT_800D412C. */
sint32 g_camera_limit_left;

sint32 g_camera_limit_top;

sint32 g_camera_limit_near;

sint32 g_camera_limit_right;

/* Original: DAT_800D4138. */
sint32 g_model_vertical_cull_override;

/* Originals: DAT_800D4130, DAT_800D4134. */
sint32 g_camera_limit_bottom;

sint32 g_camera_limit_far;

/* Original: DAT_800D416C. */
/* Originals: DAT_800D4168, DAT_800D417C. */
sint32 g_initial_stack_pointer;

sint32 g_camera_target_y;

sint32 g_camera_depth_offset;

/* Original: DAT_800D41A0. */
/* Original: DAT_800D418C. */
sint32 g_input_run_value;

/* Original: DAT_800D41A4. */
sint16 g_stage_model_count;

/* Original: DAT_800D41A8. */
/* Original: DAT_800D41BC. */
__declspec(dllexport) OBJECT *g_object_list[2];

sint32 g_transient_prim_buffer;

/* Original: DAT_800D41D8. */
/* Original: DAT_800D41D0. */
sint32 g_secondary_target_total_count;

/* Original: DAT_800D41CC. */
sint32 g_required_mission_item_count;

/* Original: DAT_800D41D4. */
sint16 g_hit_flash_clut;

/* Original: DAT_800D4148. */
sint16 g_mechanoid_model_base;

/* Original: DAT_800D41F0. */
sint16 g_radar_target_model_id;

/* Originals: DAT_800D41E0, DAT_800D41E4. */
sint16 g_tanya_model_base;

/* Original: DAT_800D41EC. */
/* Original: DAT_800D41F8. */
sint32 g_screen_width;

sint32 g_player_near_z_limit;

/* Original: DAT_800D41C4. */
sint32 g_render_row_front_projection_scale;

/* Original: DAT_800D4204. */
/* Original: DAT_800D420C. */
sint32 g_current_model_rotation_y;

sint32 g_camera_horizontal_bias;

/* Original: DAT_800D415C. */
sint32 g_effect_count;

/* Originals: DAT_800D3E84, DAT_800D4160, DAT_800D40D8, DAT_800D4180. */
sint32 g_map_model_record_count;

sint32 g_map_depth_subcells;

sint32 g_map_width_subcells;

sint32 g_map_world_depth;

/* Originals: DAT_800D413C, DAT_800D4144, DAT_800D4140. */
sint32 g_player_displacement_x;

sint32 g_player_displacement_z;

sint32 g_player_displacement_y;

/* Original: DAT_800F4520. */
__declspec(dllexport) RENDER_FRAME g_render_frame_buffer_1;

/* Shared form of the repeated MIPS signed power-of-two division idiom:
 * negative values are biased by 2^shift-1 before the arithmetic shift. */
sint32 mips_div_pow2_trunc(sint32 value, uint32 shift)
{
    if (value < 0)
        value += (sint32)((1u << shift) - 1u);
    return value >> shift;
}

/* Also replaces the identical host helper tank.c:qa (no separate MIPS entry). */
sint32 abs_s32(sint32 value)
{
    return value < 0 ? -value : value;
}
