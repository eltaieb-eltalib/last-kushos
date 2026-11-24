/*
 * Kush 1.1: fault with RSP pointing to unmapped memory. The kernel cannot
 * write the exception frame to the user stack (ProbeForWrite faults inside
 * KiDispatchExceptionToUser); the process must die, the system must not
 * bugcheck.   x86_64-w64-mingw32-gcc -O1 -o badstack64.exe badstack64.c
 */
#include <stdio.h>

int main(void)
{
    printf("badstack64: faulting with RSP = 0x10000 (unmapped)\n");
    fflush(stdout);
    __asm__ __volatile__(
        "movq $0x10000, %%rsp\n\t"
        "movq $0, %%rax\n\t"
        "movb %%al, (%%rax)\n\t"
        ::: "rax", "memory");
    return 0;
}
