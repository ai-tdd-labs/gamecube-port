    .section .text
    .global _start

_start:
    # set stack pointer to a safe RAM location
    lis 1, 0x8170
    addi 1, 1, 0x0000

    bl main

loop:
    b loop
