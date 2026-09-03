#ifndef AURORAAZ_CONTENT_LAUNCH_DETOUR_H
#define AURORAAZ_CONTENT_LAUNCH_DETOUR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Exact Aurora 0.7b.2 Rev1655 ContentLauncher entry-hook contract. */
#define AZ_REV1655_CONTENT_LAUNCH_ADDRESS 0x82294DD0u
#define AZ_REV1655_CONTENT_LAUNCH_CONTINUE_ADDRESS 0x82294DD4u
#define AZ_REV1655_CONTENT_LAUNCH_FIRST_INSTRUCTION 0x7D8802A6u

/* Assembly entry installed over ContentLauncher's displaced mflr r12. */
void az_rev1655_content_launch_direct_detour_entry(void);

/* C bridge called by content_launch_detour_shim.S; public for host tests. */
void az_rev1655_content_launch_detour_c(void);

#ifdef __cplusplus
}
#endif

#endif
