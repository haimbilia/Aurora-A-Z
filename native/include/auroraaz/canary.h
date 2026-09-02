#ifndef AURORAAZ_CANARY_H
#define AURORAAZ_CANARY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AZ_CANARY_STATUS_NOT_ATTEMPTED 0xFFFFFFFFu

#define AZ_CANARY_MONITOR_STOPPED 0u
#define AZ_CANARY_MONITOR_STARTING 1u
#define AZ_CANARY_MONITOR_RUNNING 2u
#define AZ_CANARY_MONITOR_STOPPING 3u

#define AZ_CANARY_START_ORDINAL_ENTRY 1u
#define AZ_CANARY_START_CREATE_PENDING 2u
#define AZ_CANARY_START_CREATE_RETURNED 3u
#define AZ_CANARY_START_RESUME_RETURNED 4u
#define AZ_CANARY_START_COMPLETE 5u
#define AZ_CANARY_START_ALREADY_ACTIVE 6u
#define AZ_CANARY_START_WORKER_ENTERED 7u

typedef struct AzCanaryStartSnapshot {
    uint32_t phase;
    uint32_t state;
    uint32_t ex_create_thread_status;
    uint32_t nt_resume_thread_status;
} AzCanaryStartSnapshot;

typedef void (*AzCanaryStartObserver)(
    const AzCanaryStartSnapshot *snapshot,
    void *context);

uint32_t AuroraAZCanaryGetMonitorState(void);

uint32_t AuroraAZCanaryStartMonitor(
    AzCanaryStartObserver observer,
    void *observer_context);

void AuroraAZCanaryStopMonitor(void);

#ifdef __cplusplus
}
#endif

#endif
