#ifndef MENU_H
#define MENU_H

#include "xport.h"
#include "psx.h"

enum
{
    MENU_ACTION = 0,
    MENU_LABEL = 1,
    MENU_SLIDER = 3,
    MENU_SPRITE = 4
};

typedef struct
{
    sint8 result;
    sint16 text_idx;
    uint8 kind;
    sint32 limit;
    sint32 choice;
    uint32 resource;
    sint32 *val;
    const char *const *choices;
} MENU_ENTRY;

extern MENU_ENTRY g_pause_menu[];
extern MENU_ENTRY g_level_pause_menu[];
extern MENU_ENTRY g_death_menu[];
extern MENU_ENTRY g_result_menu[];
extern MENU_ENTRY g_hq_menu[];
extern MENU_ENTRY g_hq_level_menu[];
extern MENU_ENTRY g_language_menu[];
extern sint32 g_menu_selection_idx;

GDB_CALL sint16 menu_run(MENU_ENTRY *entries, sint32 x, sint32 y);
sint16 language_menu_run(void);

#endif
