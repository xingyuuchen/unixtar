#pragma once
#include <cstdint>


struct CoroutineContext {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbp;
    uint64_t rsp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t co_resume_addr;
}__attribute__((packed));   // compact, no align


#ifdef __x86_64__
#ifdef __cplusplus
/**
 * 1. The output symbol to the linker is produced in the C language.
 *    If the output symbol is generated in the C++ way,
 *    the function name will be treated specially
 *    because the function can be overloaded,
 *    so the linker cannot find the corresponding symbol.
 *
 * 2. The function call rules are in the form of C language,
 *    such as register order for passing parameters
 */
extern "C" {
#endif

extern void SwitchCoroutineContext(CoroutineContext *_from,
                            CoroutineContext *_to)
                            asm("SwitchCoroutineContext");

#ifdef __cplusplus
}
#endif

#else
// Only support x64.
#endif


void CoYield();

