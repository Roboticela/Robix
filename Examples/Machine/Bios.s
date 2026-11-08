.code16
.section .text
.globl bios_entry
bios_entry:
    mov $0x3f9, %dx
    xor %al, %al
    out %al, %dx
    mov $0x3fb, %dx
    mov $0x80, %al
    out %al, %dx
    mov $0x3f8, %dx
    mov $1, %al
    out %al, %dx
    mov $0x3f9, %dx
    xor %al, %al
    out %al, %dx
    mov $0x3fb, %dx
    mov $3, %al
    out %al, %dx
    mov $0x3f8, %dx
    mov $0x49, %al
    out %al, %dx
    cli
1:
    hlt
    jmp 1b

.section .reset, "ax"
.code16
    ljmp $0xf000, $0
