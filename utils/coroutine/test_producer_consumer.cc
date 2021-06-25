#include "coroutine.h"
#include <thread>


void *Produce(void *) {
    for (int i = 0; i < 10; ++i) {
        printf("Produce: %d\n", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        CoroutineDispatcher::Instance().CoYieldCurr();
    }
    return nullptr;
}

void *Consume(void *) {
    for (int i = 0; i < 10; ++i) {
        printf("Consume: %d\n", i);
        CoroutineDispatcher::Instance().CoYieldCurr();
    }
    return nullptr;
}


int main() {
    CoroutineProfile producer(Produce);
    CoroutineProfile consumer(Consume);
    CoroutineDispatcher::Instance().AddCoroutine(&producer);
    CoroutineDispatcher::Instance().AddCoroutine(&consumer);
    
    CoroutineDispatcher::Instance().CoResume(&producer);
}

