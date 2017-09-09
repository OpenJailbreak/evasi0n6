.globl _call_function_shellcode
.globl _call_function_shellcode_end
.globl _call_function_shellcode_len


.thumb_func _call_function_shellcode
.thumb
.code 16
.align 2
_call_function_shellcode:
    push {r4-r7, lr}
    mov r5, r2
    mov r6, r1
    ldr r4, [r6, #0x0]
    ldr r0, [r6, #0x4]
    ldr r1, [r6, #0x8]
    ldr r2, [r6, #0xC]
    ldr r3, [r6, #0x10]
    blx r4
    str r0, [r5]
    mov r0, #0
    pop {r4-r7, pc}

.align 2
_call_function_shellcode_end:

.align 2
_call_function_shellcode_len: .word (_call_function_shellcode_end - _call_function_shellcode)
