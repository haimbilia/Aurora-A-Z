    .text
    .align 2

    .globl az_rev1655_module_settings_direct_detour_entry
az_rev1655_module_settings_direct_detour_entry:
    /* At Aurora's existing completion tail, r30 is still key 7 only for the
     * NetDbg row. Queue our popup, then let Aurora complete its own task. */
    cmpwi %r30, 7
    bne .Laz_module_settings_resume
    mflr %r12
    stwu %r1, -112(%r1)
    stw %r12, 104(%r1)
    bl az_rev1655_module_settings_detour_c
    lwz %r12, 104(%r1)
    addi %r1, %r1, 112
    mtlr %r12

.Laz_module_settings_resume:
    li %r5, 1
    lis %r11, 0x822c
    ori %r11, %r11, 0x8cec
    mtctr %r11
    bctr
