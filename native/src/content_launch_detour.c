#include <auroraaz/content_launch_detour.h>
#include <auroraaz/rev1655_runtime.h>

typedef char AzContentLaunchContinuationMustMatch[
    AZ_REV1655_CONTENT_LAUNCH_CONTINUE_ADDRESS ==
        AZ_REV1655_CONTENT_LAUNCH_ADDRESS + sizeof(unsigned int) ? 1 : -1];

void az_rev1655_content_launch_detour_c(void)
{
    /* Start cleanup at the earliest proven launch boundary, but never block
     * Aurora's UI/launcher thread. The worker restores every hook and exits
     * while ContentLauncher performs its normal information-gathering pass. */
    az_rev1655_runtime_request_shutdown();
}
