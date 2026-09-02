#ifndef AURORAAZ_NETDBG_BOOTSTRAP_H
#define AURORAAZ_NETDBG_BOOTSTRAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Starts the one-shot Rev1655 runtime bootstrap. This entry is safe to call
 * from Aurora's NetDbg logger dispatch: it performs only an atomic claim and
 * creates and resumes a verified system thread. Image validation, hook setup,
 * and all logging happen on that worker.
 */
uint32_t AuroraAZNetDbgBootstrapStart(void);

#ifdef __cplusplus
}
#endif

#endif
