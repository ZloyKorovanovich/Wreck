bits 64
global atomicCopyMemoryWrite_dword
global atomicCmpExchange_dword
global atomicExchange_dword
default rel

section .text

; rcx dst
; rdx src
; r8 count
; rax result
atomicCopyMemoryWrite_dword:
    xor rax, rax
    test r8, r8
    jz .fail

    ; check if addresses are aligned
    test rcx, 0x3
    jnz .fail

    lea r9, [rdx + r8 * 4] ; src end pointer
    .loop_4:
        mov r10d, dword[rdx]
        mov dword[rcx], r10d
        add rcx, 4
        add rdx, 4
        cmp rdx, r9 ; if src pointer = src end pointer
        jne .loop_4

    mfence
    mov rax, 1 ; set success value
    .fail:
    ret

; rcx, address
; edx, value
; r8d, compare value
; eax memory value
atomicCmpExchange_dword:
    mov eax, r8d
    cmpxchg dword [rcx], edx
    ret

; rcx, address
; edx, value
; eax memory value
atomicExchange_dword:
    xchg dword [rcx], edx
    mov eax, edx
    ret
