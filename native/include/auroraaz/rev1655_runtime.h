#ifndef AURORAAZ_REV1655_RUNTIME_H
#define AURORAAZ_REV1655_RUNTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Hardware milestones are selected explicitly at the module boundary.  Add a
 * later stage here only after its preceding hardware gate has passed; do not
 * silently widen INPUT_OBSERVE into a consuming or rendering runtime.
 */
typedef enum AzRev1655RuntimeStage {
    AZ_REV1655_RUNTIME_STAGE_DISABLED = 0,
    AZ_REV1655_RUNTIME_STAGE_INPUT_OBSERVE
} AzRev1655RuntimeStage;

typedef enum AzRev1655RuntimeState {
    AZ_REV1655_RUNTIME_STOPPED = 0,
    AZ_REV1655_RUNTIME_STARTING,
    AZ_REV1655_RUNTIME_RUNNING,
    AZ_REV1655_RUNTIME_STOPPING,
    AZ_REV1655_RUNTIME_CLOSED
} AzRev1655RuntimeState;

typedef enum AzRev1655RuntimeResult {
    AZ_REV1655_RUNTIME_OK = 0,
    AZ_REV1655_RUNTIME_BAD_STAGE,
    AZ_REV1655_RUNTIME_BUSY,
    AZ_REV1655_RUNTIME_CLOSED_RESULT,
    AZ_REV1655_RUNTIME_IMAGE_UNMAPPED,
    AZ_REV1655_RUNTIME_IMAGE_REJECTED,
    AZ_REV1655_RUNTIME_SITE_REJECTED,
    AZ_REV1655_RUNTIME_WRONG_INPUT_SITE,
    AZ_REV1655_RUNTIME_ARENA_FAILED,
    AZ_REV1655_RUNTIME_THREAD_STARTUP_REJECTED,
    AZ_REV1655_RUNTIME_THREAD_CREATE_FAILED,
    AZ_REV1655_RUNTIME_HOOK_INSTALL_FAILED,
    AZ_REV1655_RUNTIME_DETOUR_STAGE_FAILED
} AzRev1655RuntimeResult;

/*
 * Selects one reviewed runtime milestone.  INPUT_OBSERVE installs only the
 * Rev1655 input-wrapper hook and is idempotent while already running.  The
 * initial STOPPED instance is one-shot: shutdown closes the input bridge and
 * a fresh module load is required before another start.
 */
AzRev1655RuntimeResult az_rev1655_runtime_start(
    AzRev1655RuntimeStage stage);

/*
 * Synchronously requests fail-closed shutdown and returns only after the
 * target instruction is restored, resident admissions and bridge calls have
 * drained, telemetry has been drained, and the worker has exited.
 */
void az_rev1655_runtime_shutdown(void);

AzRev1655RuntimeState az_rev1655_runtime_state(void);
AzRev1655RuntimeStage az_rev1655_runtime_stage(void);
uint32_t az_rev1655_runtime_observations_logged(void);

const char *az_rev1655_runtime_result_name(AzRev1655RuntimeResult result);

#ifdef __cplusplus
}
#endif

#endif
