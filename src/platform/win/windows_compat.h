#ifndef WINDOWS_COMPAT_H
#define WINDOWS_COMPAT_H

#include <windows.h>

/* Avoid collision with the PsyQ LoadImage API. */
#ifdef LoadImage
    #undef LoadImage
#endif

#endif
