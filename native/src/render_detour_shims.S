#include <auroraaz/hook_runtime.h>

#if AZ_HOOK_RESIDENT_RETURN_ABI_VERSION != 1
#error "render detours require resident-return ABI v1"
#endif

#if AZ_HOOK_RESIDENT_EXIT_ADMISSION_DELTA != 0x20
#error "render detour resident-return layout mismatch"
#endif

    .text
    .align 2

    .globl az_rev1655_render_menu_detour_entry
az_rev1655_render_menu_detour_entry:
    /*
     * The admitted relay passes its resident active_entries address in r0.
     * The C bridge calls RenderMenu first and returns Aurora's result in r3.
     */
    mflr %r12
    stwu %r1, -112(%r1)
    stw %r0, 96(%r1)
    stw %r12, 104(%r1)
    bl az_rev1655_render_menu_detour_c
    stw %r3, 100(%r1)

    /* Restore all Aurora-visible state before leaving module text. */
    lwz %r11, 96(%r1)
    lwz %r3, 100(%r1)
    lwz %r12, 104(%r1)
    addi %r1, %r1, 112

    /*
     * Tail-handoff while active_entries is still nonzero. The hook runtime
     * owns the resident decrement/LR-restore/return epilogue.
     */
    addi %r0, %r11, -AZ_HOOK_RESIDENT_EXIT_ADMISSION_DELTA
    mtctr %r0
    bctr

    .globl az_rev1655_render_menu_direct_detour_entry
az_rev1655_render_menu_direct_detour_entry:
    /* Direct one-word hook: preserve Aurora's caller LR and result. */
    mflr %r12
    stwu %r1, -112(%r1)
    stw %r12, 104(%r1)
    bl az_rev1655_render_menu_detour_c
    stw %r3, 100(%r1)
    lwz %r12, 104(%r1)
    lwz %r3, 100(%r1)
    addi %r1, %r1, 112
    mtlr %r12
    blr

    .globl az_rev1655_font_end_detour_entry
az_rev1655_font_end_detour_entry:
    /* Pass the untouched Aurora caller LR as the C bridge's r4 argument. */
    mflr %r4
    stwu %r1, -112(%r1)
    stw %r0, 100(%r1)
    stw %r4, 104(%r1)
    bl az_rev1655_font_end_detour_c

    /* Restore all Aurora-visible state before leaving module text. */
    lwz %r11, 100(%r1)
    lwz %r12, 104(%r1)
    addi %r1, %r1, 112

    /* Resident return epilogue performs the sole admission decrement. */
    addi %r0, %r11, -AZ_HOOK_RESIDENT_EXIT_ADMISSION_DELTA
    mtctr %r0
    bctr

    .globl az_rev1655_font_end_direct_detour_entry
az_rev1655_font_end_direct_detour_entry:
    /* Pass the untouched Aurora caller LR to the existing C bridge. */
    mflr %r4
    stwu %r1, -112(%r1)
    stw %r4, 104(%r1)
    bl az_rev1655_font_end_detour_c
    lwz %r12, 104(%r1)
    addi %r1, %r1, 112
    mtlr %r12
    blr

/*
 * Exact Rev1655 trampolines. A normal call from the C bridge deliberately
 * makes the displaced mflr r12 capture the bridge return address. Aurora's
 * original function therefore returns to C, and C returns through the shim.
 */
    .globl az_rev1655_render_menu_original_fallback
az_rev1655_render_menu_original_fallback:
    mflr %r12
    lis %r11, 0x8235
    ori %r11, %r11, 0x8a0c
    mtctr %r11
    bctr

    .globl az_rev1655_font_end_original_fallback
az_rev1655_font_end_original_fallback:
    mflr %r12
    lis %r11, 0x8247
    ori %r11, %r11, 0xe394
    mtctr %r11
    bctr
