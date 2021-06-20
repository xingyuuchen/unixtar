#include "coroutine.h"
#include <cstdio>


void Printf() {
    printf("this is printf\n");
}

int main() {
    CoroutineContext context1{};
    CoroutineContext context2{};
    SwitchCoroutineContext(&context1, &context2);
    
    Printf();
    printf("%llu, %llu, %llu\n", context1.rbp, context1.rsp, context1.co_resume_addr);
}

