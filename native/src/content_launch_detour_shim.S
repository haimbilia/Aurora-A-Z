    .text
    .align 2

    .globl az_rev1655_content_launch_direct_detour_entry
az_rev1655_content_launch_direct_detour_entry:
    /* Preserve Aurora's two constructor arguments and its caller LR while
     * the C bridge starts non-blocking plugin cleanup. */
    mflr %r12
    stwu %r1, -112(%r1)
    stw %r3, 92(%r1)
    stw %r4, 96(%r1)
    stw %r12, 104(%r1)
    bl az_rev1655_content_launch_detour_c

    /* Emulate the displaced mflr r12 and resume at the original prologue's
     * second instruction. The worker restores the hook asynchronously. */
    lwz %r3, 92(%r1)
    lwz %r4, 96(%r1)
    lwz %r12, 104(%r1)
    addi %r1, %r1, 112
    lis %r11, 0x8229
    ori %r11, %r11, 0x4dd4
    mtctr %r11
    bctr
