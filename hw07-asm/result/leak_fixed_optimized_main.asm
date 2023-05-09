    bits 64
    extern malloc, puts, printf, fflush, abort, free
    global main

    section   .data
empty_str: db 0x0
int_format: db "%ld ", 0x0
data: dq 4, 8, 15, 16, 23, 42
data_length: equ ($-data) / 8

    section   .text
;;; print_int proc
print_int:
    mov rsi, rdi
    mov rdi, int_format
    xor rax, rax
    call printf

    xor rdi, rdi
    call fflush

    ret

;;; p proc
p:
    mov rax, rdi
    and rax, 1
    ret

;;; add_element proc
add_element:
    push rbp
    push rbx

    mov rbp, rdi
    mov rbx, rsi

    mov rdi, 16
    call malloc
    test rax, rax
    jz abort

    mov [rax], rbp
    mov [rax + 8], rbx

    pop rbx
    pop rbp

    ret

;;; m proc
m:
    push rbp
    push rbx
map_next_iter:
    test rdi, rdi
    jz outm

    mov rbx, rdi
    mov rbp, rsi

    mov rdi, [rdi]
    call rsi

    mov rdi, [rbx + 8]
    mov rsi, rbp
    jmp map_next_iter

outm:
    pop rbx
    pop rbp

    ret

;;; f proc
f:
    push rbx
    push r12
    push r13
filter_next_iter:
    mov rax, rsi

    test rdi, rdi
    jz outf

    mov rbx, rdi
    mov r12, rsi
    mov r13, rdx

    mov rdi, [rdi]
    call rdx
    test rax, rax
    jz z

    mov rdi, [rbx]
    mov rsi, r12
    call add_element
    mov rsi, rax
    jmp ff

z:
    mov rsi, r12

ff:
    mov rdi, [rbx + 8]
    mov rdx, r13
    jmp filter_next_iter

outf:
    pop r13
    pop r12
    pop rbx

    ret

list_head_delete:
        push    rbp ;save base
        push    rbx ;save rbx
        sub     rsp, 8 ;enlarge stack for 1 uint64 var
free_next_iter:
        mov     rax, rdi ;first parameter - *list
        test    rsi, rsi ;test count on 0
        je      last_delete_iteration ;return *list
        test    rdi, rdi ;test *list on null
        je      last_delete_iteration ;return *list
        mov     rbx, rsi ;in ebx/rbx now counter parameter
        mov     rbp, [rdi+8] ;assign to first var on stack frame *the_result list->next
        call    free ;at this point rdi is *list- calling free on list;
        lea     rsi, [rbx-1] ;optimized decrement of second parameter - passing as second parameter to recursive call
        mov     rdi, rbp ;address of first var on stack - list->next as first parameter to recursive call
        jmp free_next_iter ;next iteration
last_delete_iteration:
        add     rsp, 8 ;dispose stack vars
        pop     rbx ;restore registers
        pop     rbp

        ret

;;; main proc
main:
    push rbx

    xor rax, rax
    mov rbx, data_length
adding_loop:
    mov rdi, [data - 8 + rbx * 8]
    mov rsi, rax
    call add_element
    dec rbx
    jnz adding_loop

    mov rbx, rax

    mov rdi, rax
    mov rsi, print_int
    call m

    mov rdi, empty_str
    call puts

    mov rdx, p
    xor rsi, rsi
    mov rdi, rbx
    call f

    push r9

    mov r9, rax

    mov rdi, rax
    mov rsi, print_int
    call m

    mov rdi, empty_str
    call puts
;in rdx now we have source list
;in r9 now we have filtered list
    mov rdi, rdx
    mov rsi, -1
    call list_head_delete

    mov rdi, r9
    mov rsi, -1
    call list_head_delete

    pop r9

    pop rbx
    xor rax, rax

    ret
