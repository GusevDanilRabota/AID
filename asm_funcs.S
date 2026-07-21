# @file asm_funcs.S
# @brief Ассемблерные реализации LFSR и бинарного поиска (Intel-синтаксис).
# @details Соглашение о вызовах Microsoft x64:
#          - Первые 4 целочисленных параметра: rcx, rdx, r8, r9
#          - Результат в rax (uint16_t возвращается в ax)
#          - Регистры rax, rcx, rdx, r8-r11 являются volatile
.intel_syntax noprefix
.text
.global lfsr_rand, binary_search_entry, lfsr_seed

# @brief 16-битный генератор псевдослучайных чисел на базе 32-битного LFSR (xorshift).
# @return uint16_t – очередное случайное число.
lfsr_rand:
    push rbp
    mov rbp, rsp
    mov eax, DWORD PTR [rip + lfsr_state]  # загружаем состояние
    mov edx, eax
    shl edx, 13
    xor eax, edx
    mov edx, eax
    shr edx, 17
    xor eax, edx
    mov edx, eax
    shl edx, 5
    xor eax, edx                          # xorshift
    mov DWORD PTR [rip + lfsr_state], eax  # сохраняем новое состояние
    and eax, 0xFFFF                       # возвращаем младшие 16 бит
    pop rbp
    ret

# @brief Инициализация состояния LFSR.
# @param seed 32-битное начальное значение (передаётся в ECX).
lfsr_seed:
    mov DWORD PTR [rip + lfsr_state], ecx
    ret

# @brief Бинарный поиск в строке матрицы переходов.
# @param row Указатель на массив пар {uint16_t id, uint16_t cum_prob} (RCX).
# @param rnd Случайное число uint16_t (передаётся в EDX, используется младшие 16 бит).
# @param len Количество элементов в строке (R8D).
# @return uint16_t Идентификатор следующего токена.
binary_search_entry:
    xor eax, eax                  # результат = 0
    test r8d, r8d                 # if len == 0 goto error
    jz .end_error
    push rbx
    # high = len - 1
    dec r8d                       # r8d = high
    xor ebx, ebx                  # low = 0
.loop:
    cmp ebx, r8d
    jg .not_found
    # mid = (low + high) / 2
    lea r9d, [ebx + r8d]
    shr r9d, 1
    # r10 = &row[mid]
    lea r10, [rcx + r9*4]         # каждая запись 4 байта (id + cum_prob)
    movzx r11d, WORD PTR [r10+2]  # cum_prob
    cmp r11w, dx                  # сравнение с rnd
    jae .maybe_left
    lea ebx, [r9d + 1]            # low = mid + 1
    jmp .loop
.maybe_left:
    test r9d, r9d
    jz .found                     # если mid == 0, то сразу подходит
    # проверим предыдущий cum_prob
    movzx r11d, WORD PTR [r10 - 2]
    cmp r11w, dx
    jb .found                     # prev < rnd => нашли
    lea r8d, [r9d - 1]            # high = mid - 1
    jmp .loop
.found:
    movzx eax, WORD PTR [r10]     # id
    pop rbx
    ret
.not_found:
    movzx eax, WORD PTR [rcx]     # возвращаем первый элемент
    pop rbx
    ret
.end_error:
    xor eax, eax
    pop rbx
    ret

.data
.align 4
lfsr_state: .long 0xDEADBEEF