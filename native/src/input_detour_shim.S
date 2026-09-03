#include <auroraaz/hook_runtime.h>

#if AZ_HOOK_RESIDENT_RETURN_ABI_VERSION != 1
#error "input detour requires resident-return ABI v1"
#endif

#if AZ_HOOK_RESIDENT_EXIT_ADMISSION_DELTA != 0x20
#error "input detour resident-return layout mismatch"
#endif

    .text
    .align 2

    .globl az_rev1655_input_detour_entry
az_rev1655_input_detour_entry:
    /*
     * The resident hook relay increments its admission counter before it
     * branches here and passes the resident state address in volatile r0.
     * Save both values before calling C. After C returns, restore the caller
     * stack and branch to the resident exit epilogue. Only resident code
     * decrements the counter and returns to Aurora.
     */
    mflr %r6
    stwu %r1, -112(%r1)
    stw %r0, 100(%r1)
    stw %r6, 104(%r1)
    bl az_rev1655_input_detour_c

    lwz %r11, 100(%r1)  /* resident admission state */
    lwz %r12, 104(%r1)  /* Aurora's original caller LR */
    addi %r1, %r1, 112
    addi %r0, %r11, -AZ_HOOK_RESIDENT_EXIT_ADMISSION_DELTA
    mtctr %r0
    bctr

    .globl az_rev1655_input_direct_detour_entry
az_rev1655_input_direct_detour_entry:
    /*
     * The plugin image is linked within relative-branch reach of Aurora's
     * wrapper, so the observe-only gate needs no generated relay. Preserve
     * Aurora's caller LR across the C bridge and return directly to it.
     */
    mflr %r6
    stwu %r1, -112(%r1)
    stw %r6, 104(%r1)
    bl az_rev1655_input_detour_c
    lwz %r12, 104(%r1)
    addi %r1, %r1, 112
    mtlr %r12
    blr

/*
 * Dedicated Rev1655 trampoline. The input hook's displaced instruction is
 * mflr r12. This function is called normally by the C bridge, so Aurora's
 * wrapper returns to the C bridge and the bridge then returns to Aurora's
 * original caller.
 */
    .globl az_rev1655_input_original_fallback
az_rev1655_input_original_fallback:
    mflr %r12
    lis %r11, 0x8280
    ori %r11, %r11, 0x1d94
    mtctr %r11
    bctr
