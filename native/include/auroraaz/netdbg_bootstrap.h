#ifndef AURORAAZ_NETDBG_BOOTSTRAP_H
#define AURORAAZ_NETDBG_BOOTSTRAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Starts the one-shot Rev1655 runtime bootstrap. This entry is safe to call
 * from Aurora's NetDbg logger dispatch: after the atomic claim it performs a
 * bounded, quiet exact-image check and pins the live key-7 wrapper before
 * returning to Aurora, then creates and resumes a verified system thread.
 * Hook setup and all logging happen on that worker.
 */
uint32_t AuroraAZNetDbgBootstrapStart(void);

/* NetDbg ordinal 4. The bootstrap passes this exact live export address to
 * the synchronous Rev1655 lifetime pin before returning to Aurora. */
uint32_t AuroraAZNetDbgWrite(const char *message);

#ifdef __cplusplus
}
#endif

#endif
