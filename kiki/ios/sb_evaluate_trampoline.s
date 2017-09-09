.globl _sb_evaluate_trampoline
.globl _sb_evaluate_trampoline_hook_address
.globl _sb_evaluate_trampoline_len

.thumb_func _sb_evaluate_trampoline

.thumb
.code 16
.align 2
_sb_evaluate_trampoline:
    push  {r0, r1}
    ldr   r0, _sb_evaluate_trampoline_hook_address
    bx    r0
.align 2
_sb_evaluate_trampoline_hook_address: .word 0x0

.align 2
_sb_evaluate_trampoline_end:

.align 2
_sb_evaluate_trampoline_len: .word (_sb_evaluate_trampoline_end - _sb_evaluate_trampoline)
