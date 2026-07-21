.intel_syntax noprefix
.text
.global lfsr_rand, binary_search_entry

# uint16_t lfsr_rand(void)
lfsr_rand:
    push rbp
    mov rbp, rsp
    # Используем глобальную переменную состояния (можно через TLS, но для прототипа static memory)
    mov eax, DWORD PTR [rip + lfsr_state]
    mov edx, eax
    shl edx, 13
    xor eax, edx
    mov edx, eax
    shr edx, 17
    xor eax, edx
    mov edx, eax
    shl edx, 5
    xor eax, edx
    mov DWORD PTR [rip + lfsr_state], eax
    # возвращаем младшие 16 бит
    and eax, 0xFFFF
    pop rbp
    ret

.data
.align 4
lfsr_state: .long 0xDEADBEEF

# uint16_t binary_search_entry(const uint16_t *row, uint16_t rnd, int len)
binary_search_entry:
    # rdi = row pointer, rsi = rnd, edx = len
    xor eax, eax          # индекс = 0
    cmp edx, 0
    je .end_error
    # сохраняем базу в rdi, будем искать бинарно
    push rbx
    mov ecx, edx          # ecx = high = len-1
    dec ecx
    xor ebx, ebx          # ebx = low = 0
.loop:
    cmp ebx, ecx
    jg .not_found
    mov edx, ebx
    add edx, ecx
    shr edx, 1            # mid
    lea r8, [rdi + rdx*4] # каждая запись 4 байта (2+2)
    movzx r9d, WORD PTR [r8+2]  # cum_prob (смещение +2)
    cmp r9w, si
    jae .maybe_left
    # cum_prob < rnd -> идём вправо
    lea ebx, [rdx + 1]
    jmp .loop
.maybe_left:
    # cum_prob >= rnd; проверим предыдущий элемент
    cmp edx, 0
    je .found
    movzx r10d, WORD PTR [r8 - 2] # cum_prob предыдущего (смещение -2 от текущего, адрес r8-4+2)
    cmp r10w, si
    jb .found
    # prev >= rnd -> сдвигаем high в mid-1
    lea ecx, [rdx - 1]
    jmp .loop
.found:
    movzx eax, WORD PTR [r8]   # id
    pop rbx
    ret
.not_found:
    movzx eax, WORD PTR [rdi]  # если всё < rnd, возвращаем первый (защита)
    pop rbx
    ret
.end_error:
    xor eax, eax
    pop rbx
    ret