bits 64
global asmCopyMemoryDwordAtomicW
global asmCopyMemoryQwordAtomicW
default rel

section .text

; rcx dst
; rdx src
; r8 count
asmCopyMemoryDwordAtomicW:
    lea r9, [rdx + r8 * 4] ; src end pointer
    .loop:
        mov r10d, dword[rdx]
        xchg dword[rcx], r10d
        add rcx, 4
        add rdx, 4
        cmp rdx, r9 ; if src pointer = src end pointer
        jne .loop
    ret

; rcx dst
; rdx src
; r8 count
asmCopyMemoryQwordAtomicW:
    lea r9, [rdx + r8 * 8] ; src end pointer
    .loop:
        mov r10, qword[rdx]
        xchg qword[rcx], r10
        add rcx, 8
        add rdx, 8
        cmp rdx, r9 ; if src pointer = src end pointer
        jne .loop
    ret
