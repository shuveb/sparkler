bits 16

SERIAL_PORT             equ 0x3f8
TWITTER_DEVICE          equ 0x100
WEATHER_DEVICE_BASE     equ 0x100
AIR_QUALITY_DEVICE_BASE equ 0x200

start:
    mov ax, 0x100
    add ax, 0x20
    mov ss, ax
    mov sp, 0x1000
    cld

    mov ax, 0x100
    mov ds, ax

    mov si, welcome_msg
    call print_str

    jmp menu_loop

press_key:
    mov si, press_any_key
    call print_str
    call get_users_choice
menu_loop:
    call display_main_menu
    call get_users_choice
    cmp al, 0x31
    je .cpu_details
    cmp al, 0x32
    je .latest_tweet
    cmp al, 0x33
    je .weather
    cmp al, 0x34
    je .air_quality
    cmp al, 0x35
    je .halt

    mov si, illegal_choice
    call print_str
    jmp press_key

    .cpu_details:
        call print_cpu_details
        jmp press_key
    .latest_tweet:
        call print_latest_tweet
        call print_new_line
        jmp press_key
    .weather:
        mov si, weather_str
        call print_str
        call print_new_line
        mov si, cities_str
        call print_str
        call print_new_line
        mov si, your_choice
        call print_str
        sub ax, ax
        call get_users_choice
        sub ax, 0x30                    ; turn it from ascii to number

        cmp ax, 1
        jl  .illegal_choice
        cmp ax, 6
        jg .illegal_choice

        add ax, WEATHER_DEVICE_BASE     ; this gives us the port number for the city
        mov dx, ax
        call print_weather
        jmp press_key
    .air_quality:
        mov si, air_quality_str
        call print_str
        call print_new_line
        mov si, cities_str
        call print_str
        call print_new_line
        mov si, your_choice
        call print_str
        sub ax, ax
        call get_users_choice
        sub ax, 0x30                        ; turn it from ascii to number

        cmp ax, 1
        jl  .illegal_choice
        cmp ax, 6
        jg .illegal_choice

        add ax, AIR_QUALITY_DEVICE_BASE     ; this gives us the port number for the city
        mov dx, ax
        call print_weather
        jmp press_key

        .illegal_choice:
            call print_new_line
            mov si, illegal_choice
            call print_str
            jmp press_key
    .halt:
        hlt

data:
    welcome_msg         db `Welcome to Sparkler!\n`, 0

    ; Used by the menu system
    main_menu           db  `\nMain menu:\n==========\n`, 0
    main_menu_items     db  `1. CPU Info\n2. Latest CliMagic Tweet\n3. Get Weather\n4. Get Air Quality\n5. Halt VM\n`, 0
    your_choice         db  `Your choice: \n`, 0
    illegal_choice      db  `You entered an illegal choice!\n\n`, 0
    press_any_key       db  `Press any key to continue...\n`, 0

    ; Used by our CPU ID routines
    cpu_info_str        db  `\nHere is your CPU information:\n`, 0
    cpuid_str           db  `Vendor ID\t: `, 0
    brand_str           db  `Brand string\t: `, 0
    cpu_type_str        db  `CPU type\t: `, 0
    cpu_type_oem        db  'Original OEM Processor', 0
    cpu_type_overdrive  db  'Intel Overdrive Processor', 0
    cpu_type_dual       db  'Dual processor', 0
    cpu_type_reserved   db  'Reserved', 0
    cpu_family_str      db  `Family\t\t: `, 0
    cpu_model_str       db  `Model\t\t: `, 0
    cpu_stepping_str    db  `Stepping\t: `, 0

    ; Used by devices which fetch over the internet
    fetching_wait       db  `\nFetching, please wait...\n`, 0


    weather_str         db `\nChoose the city to get weather forecast for:`, 0
    air_quality_str     db `\nChoose the city to get air quality report for:`, 0
    ; Cities
    cities_str          db  `1. Chennai\n2. New Delhi\n3. London\n4. Chicago\n5. San Francisco\n6. New York`,0

    cpuid_function      dd  0x80000002

get_users_choice:
    mov dx, SERIAL_PORT
    in ax, dx
    ret

display_main_menu:
    mov si, main_menu
    call print_str
    mov si, main_menu_items
    call print_str
    mov si, your_choice
    call print_str
    ret

print_latest_tweet:
    mov si, fetching_wait
    call print_str
    mov dx, TWITTER_DEVICE
    .get_next_char:
        in ax, dx
        cmp ax, 0
        je .done
        call print_char
        jmp .get_next_char

    .done:
        ret

; To be called with weather port alreay in DX
print_weather:
    mov si, fetching_wait
    call print_str
    .get_next_char:
        in ax, dx
        cmp ax, 0
        je .done
        call print_char
        jmp .get_next_char

    .done:
        ret

print_cpu_details:
    mov si, cpu_info_str
    call print_str

    mov si, cpuid_str
    call print_str
    call print_cpuid
    call print_new_line

    call print_cpu_info

    mov si, brand_str
    call print_str
    call print_cpu_brand_string
    call print_new_line
    ret

print_cpuid:
    mov eax, 0
    cpuid
    push ecx
    push edx
    push ebx

    mov cl, 3
    .next_dword:
        pop eax
        mov bl, 4
        .print_register:
            call print_char
            shr eax, 8
            dec bl
            jnz .print_register
        dec cl
        jnz .next_dword

    ret

print_cpu_brand_string:
    mov al, '"'
    call print_char
    .next_function:
        mov eax, [cpuid_function]
        cpuid
        push edx
        push ecx
        push ebx
        push eax

    mov cl, 4
    .next_dword:
        pop eax
        mov bl, 4
        .print_register:
            call print_char
            shr eax, 8
            dec bl
            jnz .print_register
        dec cl
        jnz .next_dword

    inc dword[cpuid_function]
    cmp dword[cpuid_function], 0x80000004
    jle .next_function

    mov al, '"'
    call print_char
    ret

print_cpu_info:
    mov eax, 1
    cpuid

    mov si, cpu_type_str
    call print_str
    mov ecx, eax                        ; save a copy
    shr eax, 12
    and eax, 0x0005
    cmp al, 0
    je .type_oem
    cmp al, 1
    je .type_overdrive
    cmp al, 2
    je .type_dual
    cmp al, 3
    je .type_reserved

    .type_oem:
        mov si, cpu_type_oem
        jmp .print_cpu_type
    .type_overdrive:
        mov si, cpu_type_oem
        jmp .print_cpu_type
    .type_dual:
        mov si, cpu_type_dual
        jmp .print_cpu_type
    .type_reserved:
        mov si, cpu_type_reserved
        jmp .print_cpu_type

    .print_cpu_type:
    call print_str
    call print_new_line

    ; Family
    mov si, cpu_family_str
    call print_str
    mov eax, ecx
    shr eax, 8
    and ax, 0x000f

    cmp ax, 15                  ; if Family == 15, Family is derived as the
    je .calculate_family        ; sum of Family + Extended family bits

    jmp .family_done            ; else

    .calculate_family:
        mov ebx, ecx
        shr ebx, 20
        and bx, 0x00ff
        add ax, bx
    .family_done:
        call print_word_hex

    ; Model
    mov si, cpu_model_str
    call print_str
    cmp al, 6                   ; If family is 6 or 15, the model number
    je .calculate_model         ; is derived from the extended model ID bits
    cmp al, 15
    je .calculate_model

    mov eax, ecx                ; else
    shr eax, 4
    and ax, 0x000f
    jmp .model_done

    .calculate_model:
        mov eax, ecx
        mov ebx, ecx
        shr eax, 16
        and ax, 0x000f
        shl eax, 4
        shr ebx, 4
        and bx, 0x000f
        add eax, ebx
    .model_done:
        call print_word_hex

    ; Stepping
    mov si, cpu_stepping_str
    call print_str
    mov eax, ecx
    and ax, 0x000f
    call print_word_hex

    ret

print_new_line:
    push dx
    push ax
    mov dx, SERIAL_PORT
    mov al, `\n`
    out dx, al
    pop ax
    pop dx
    ret

print_char:
    push dx
    mov dx, SERIAL_PORT
    out dx, al
    pop dx
    ret

print_str:
    push dx
    push ax
    mov dx, SERIAL_PORT
    .print_next_char:
        lodsb               ; load byte pointed to by SI into AL and SI++
        cmp al, 0
        je .printstr_done
        out dx, al
        jmp .print_next_char
    .printstr_done:
        pop ax
        pop dx
        ret

; Print the 16-bit value in AX as HEX
print_word_hex:
    xchg al, ah             ; Print the high byte first
    call print_byte_hex
    xchg al, ah             ; Print the low byte second
    call print_byte_hex
    call print_new_line
    ret

; Print lower 8 bits of AL as HEX
print_byte_hex:
    push dx
    push cx
    push ax

    lea bx, [.table]        ; Get translation table address

    ; Translate each nibble to its ASCII equivalent
    mov ah, al              ; Make copy of byte to print
    and al, 0x0f            ;     Isolate lower nibble in AL
    mov cl, 4
    shr ah, cl              ; Isolate the upper nibble in AH
    xlat                    ; Translate lower nibble to ASCII
    xchg ah, al
    xlat                    ; Translate upper nibble to ASCII

    mov dx, SERIAL_PORT
    mov ch, ah              ; Make copy of lower nibble
    out dx, al
    mov al, ch
    out dx, al

    pop ax
    pop cx
    pop dx
    ret
.table: db "0123456789ABCDEF", 0