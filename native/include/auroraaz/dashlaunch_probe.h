#ifndef AURORAAZ_DASHLAUNCH_PROBE_H
#define AURORAAZ_DASHLAUNCH_PROBE_H
#include <stdint.h>
typedef struct AzDashlaunchProbe {
    uint32_t magic;
    uint32_t version;
    uint32_t calls;
    uint32_t last_reason;
    uint32_t attach_seen;
    uint32_t module;
} AzDashlaunchProbe;
extern volatile AzDashlaunchProbe g_az_dashlaunch_probe;
#endif
