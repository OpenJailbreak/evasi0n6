.globl _sb_evaluate_hook
.globl _sb_evaluate_hook_orig_addr
.globl _sb_evaluate_hook_vn_getpath
.globl _sb_evaluate_hook_memcmp
.globl _sb_evaluate_hook_len

.thumb_func _sb_evaluate_hook

.thumb
.code 16
.align 2
_sb_evaluate_hook:
    pop {r0, r1}
    push {r0-r4, lr}
    sub sp, #0x44
    ldr r4, [r3, #0x14]
    cmp r4, #0
    beq Lactually_eval
    ldr r3, _sb_evaluate_hook_vn_getpath
    mov r1, sp
    mov r0, #0x40
    add r2, sp, #0x40
    str r0, [r2]
    mov r0, r4
    blx r3
    cmp r0, #28
    beq Lenospc
    cmp r0, #0
    bne Lactually_eval
Lenospc:
    # that error's okay...

    mov r0, sp
    adr r1, var_mobile
    mov r2, #19 ;# len(var_mobile)
    ldr r3, _sb_evaluate_hook_memcmp
    blx r3
    cmp r0, #0
    bne Lallow

    mov r0, sp
    adr r1, preferences_com_apple
    mov r2, #49 ;# len(preferences_com_apple)
    ldr r3, _sb_evaluate_hook_memcmp
    blx r3
    cmp r0, #0
    beq Lactually_eval

    mov r0, sp
    adr r1, preferences
    mov r2, #39 ;# len(preferences)
    ldr r3, _sb_evaluate_hook_memcmp
    blx r3
    cmp r0, #0
    bne Lactually_eval

Lallow:
    # it's not in /var/mobile but we have a path, let it through
    add sp, #0x44
    pop {r0}
    mov r1, #0
    str r1, [r0]
    mov r1, #0x18
    str r1, [r0, #4]
    pop {r1-r4, pc}

Lactually_eval:
    add sp, #0x44
    ldr r0, [sp, #5*4]
    mov lr, r0
    ldr r1, _sb_evaluate_hook_orig_addr
    mov r9, r1

    pop {r0-r4}
    add sp, #4

    b Ljump_back

.align 2
var_mobile: .ascii "/private/var/mobile"
.align 2
preferences_com_apple: .ascii "/private/var/mobile/Library/Preferences/com.apple"
.align 2
preferences: .ascii "/private/var/mobile/Library/Preferences"
.align 2
_sb_evaluate_hook_orig_addr: .word 0x0
_sb_evaluate_hook_vn_getpath: .word 0x0
_sb_evaluate_hook_memcmp: .word 0x0

.align 2
Ljump_back:

.align 2
_sb_evaluate_hook_end:

.align 2
_sb_evaluate_hook_len: .word (_sb_evaluate_hook_end - _sb_evaluate_hook)
