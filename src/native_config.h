#ifndef NATIVE_CONFIG_H
#define NATIVE_CONFIG_H

/* Xcrt disables Release environment overrides */
#if defined(XPORT_XCRT)
    #define getenv(name) ((char *)0)
#endif

/* Preserve the existing Debug convenience default with an explicit build override */
#ifndef LEVEL_SELECT
    #if defined(_DEBUG)
        #define LEVEL_SELECT 1
    #else
        #define LEVEL_SELECT 0
    #endif
#endif

#endif
