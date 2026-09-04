    .text
    .align 2

    .globl az_rev1655_module_settings_direct_detour_entry
az_rev1655_module_settings_direct_detour_entry:
    /* Key 8 is the separately registered Aurora A-Z wrapper. Route only that
     * entry through the normal stock scene-loading path with our embedded XUR. */
    cmpwi %r30, 8
    bne .Laz_module_settings_resume
    mflr %r12
    stwu %r1, -112(%r1)
    stw %r12, 104(%r1)
    mr %r3, %r26
    bl az_module_settings_capture_controller
    bl az_module_settings_scene_path
    mr %r30, %r3
    lwz %r12, 104(%r1)
    addi %r1, %r1, 112
    mtlr %r12
    lis %r11, 0x822c
    ori %r11, %r11, 0x8bf0
    mtctr %r11
    bctr

    .globl az_rev1655_module_settings_scene_capture_detour_entry
az_rev1655_module_settings_scene_capture_detour_entry:
    /* Capture the live scene handle while Aurora's loader stack frame still
     * owns its XuiSceneCreate output slot at r31+0x70. */
    lwz %r3, 0x70(%r31)
    mflr %r12
    stwu %r1, -112(%r1)
    stw %r12, 104(%r1)
    bl az_module_settings_capture_scene
    lwz %r12, 104(%r1)
    addi %r1, %r1, 112
    mtlr %r12
    lwz %r3, 0x70(%r31)
    lis %r11, 0x822c
    ori %r11, %r11, 0x8c3c
    mtctr %r11
    bctr

.Laz_module_settings_resume:
    cmpwi %cr6, %r30, 1
    lis %r11, 0x822c
    ori %r11, %r11, 0x8b8c
    mtctr %r11
    bctr
