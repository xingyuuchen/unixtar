/**
 *  AT&T assembly.
 *
 * Note:
 *  1. Params are saved in rdi, rsi, rdx, rcx, r8, r9.
 *  2. The return value is saved in rax.
 *  3. The stack of x86 arch is full descendant.
 *
 */


.data
test_str:
.asciz "this is for test %p\n"


.text
// make symbol "SwitchCoroutineContext" visible for linker.
.global SwitchCoroutineContext

#ifndef __APPLE__
// below is unnecessary in MacOS assembly.
.type SwitchCoroutineContext, @function
#endif

SwitchCoroutineContext:
    // save register set.
    movq %rax, (%rdi)
    callq SaveRbxToR15
    addq $8, 40(%rdi)

    movq (%rsp), %rax
    movq %rax, 128(%rdi)

    // revert register set.
    movq (%rsi), %rax
    movq 8(%rsi), %rbx
    movq 16(%rsi), %rcx
    movq 24(%rsi), %rdx
    movq 32(%rsi), %rbp
    movq 40(%rsi), %rsp
    movq 56(%rsi), %rdi
    movq 64(%rsi), %r8
    movq 72(%rsi), %r9
    movq 80(%rsi), %r10
    movq 88(%rsi), %r11
    movq 96(%rsi), %r12
    movq 104(%rsi), %r13
    movq 112(%rsi), %r14
    movq 120(%rsi), %r15
    pushq 128(%rsi)   // where coroutine continues.
    movq 48(%rsi), %rsi  // revert %rsi at last.
    retq


.global StartCoroutine
#ifndef __APPLE__
.type StartCoroutine, @function
#endif
StartCoroutine:
    movq %rax, (%rdi)

    movq (%rsp), %rax
    movq %rax, 128(%rdi)

    callq SaveRbxToR15
    addq $8, 40(%rdi)

    movq (%rdi), %rax
    pushq 128(%rsi)   // where coroutine starts.

    movq %rsp, 136(%rsi)   // save stack_frames_start.
    retq


.global GetRsp
#ifndef __APPLE__
.type GetRsp, @function
#endif
GetRsp:
    movq %rsp, (%rdi)
    retq


SaveRbxToR15:
    movq %rbx, 8(%rdi)
    movq %rcx, 16(%rdi)
    movq %rdx, 24(%rdi)
    movq %rbp, 32(%rdi)
    movq %rsp, 40(%rdi)
    // add $8 to %rsp because this is a procedure call.
    addq $8, 40(%rdi)
    movq %rsi, 48(%rdi)
    movq %rdi, 56(%rdi)
    movq %r8, 64(%rdi)
    movq %r9, 72(%rdi)
    movq %r10, 80(%rdi)
    movq %r11, 88(%rdi)
    movq %r12, 96(%rdi)
    movq %r13, 104(%rdi)
    movq %r14, 112(%rdi)
    movq %r15, 120(%rdi)
    retq


.global SaveStackFrames
#ifndef __APPLE__
.type SaveStackFrames, @function
#endif
SaveStackFrames:
  save_qword:
    movq (%rsi), %rax
    movq %rax, (%rdi)
    subq $8, %rsi
    addq $8, %rdi
    subq $8, %rdx
    jnz save_qword

    retq

.global RevertStackFrames
#ifndef __APPLE__
.type RevertStackFrames, @function
#endif
RevertStackFrames:
  revert_qword:
    movq (%rdi), %rax
    movq %rax, (%rsi)
    subq $8, %rsi
    addq $8, %rdi
    subq $8, %rdx
    jnz revert_qword

    retq
