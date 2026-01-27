# Minimal PPC assembly that writes test pattern and loops forever
# Entry point: 0x80003100 (standard DOL entry)

.section .text
.global _start

_start:
    # Write test pattern to 0x80300000
    lis     3, 0x8030          # r3 = 0x80300000

    # Write 0xDEADBEEF 16 times (64 bytes)
    lis     4, 0xDEAD
    ori     4, 4, 0xBEEF       # r4 = 0xDEADBEEF
    li      5, 16              # counter
    mtctr   5
write_deadbeef:
    stw     4, 0(3)
    addi    3, 3, 4
    bdnz    write_deadbeef

    # Write 0xCAFEBABE 16 times (64 bytes)
    lis     4, 0xCAFE
    ori     4, 4, 0xBABE       # r4 = 0xCAFEBABE
    li      5, 16
    mtctr   5
write_cafebabe:
    stw     4, 0(3)
    addi    3, 3, 4
    bdnz    write_cafebabe

    # Write bytes 0x00-0x7F (128 bytes)
    li      4, 0               # byte value
    li      5, 128             # counter
    mtctr   5
write_seq:
    stb     4, 0(3)
    addi    3, 3, 1
    addi    4, 4, 1
    bdnz    write_seq

    # Infinite loop
infinite:
    b       infinite
