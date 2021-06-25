#include "coroutine.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <thread>


#ifdef __x86_64__
extern "C" {

void StartCoroutine(CoroutineContext *_from,
                    CoroutineContext *_to)
                    asm("StartCoroutine");

void GetRsp(uint64_t *_rsp) asm("GetRsp");

void SaveStackFrames(void *_buf, void *_stack_start, uint64_t _size)
                     asm("SaveStackFrames");

void RevertStackFrames(void *_buf, void *_stack_start, uint64_t _size)
                       asm("RevertStackFrames");

}
#else
// Only support x64.
#endif


uint64_t CoroutineProfile::kInvalidUid = 0;

CoroutineProfile::CoroutineProfile(CoEntry _entry)
        : co_context()
        , co_uid(kInvalidUid)
        , has_start_(false) {
    
    static uint64_t curr_uid = kInvalidUid;
    co_uid = ++curr_uid;
    memset(&co_context, 0, sizeof(co_context));
    co_context.co_resume_addr = (void *) _entry;
}

void CoroutineProfile::CoResumeSelf(CoroutineProfile *_curr) {
    CoroutineContext *ctx_from = &_curr->co_context;
    uint64_t rsp;
    GetRsp(&rsp);
#ifdef __x86_64__
    rsp += 8;   // `addq $8, %rsp` because this is a procedure call.
#endif
    assert(this != _curr);
    
    // save stack frames of `_from`, except for main coroutine.
    if (_curr->co_context.stack_frames_start) {
        if ((uint64_t) ctx_from->stack_frames_start < rsp) {
            assert(false);
        }
        // x86 full descent stack.
        ctx_from->stack_frames_len =
                    (uint64_t) ctx_from->stack_frames_start - rsp + 8;
        assert(ctx_from->stack_frames_len < sizeof(ctx_from->stack_frames));
    
        SaveStackFrames(ctx_from->stack_frames, ctx_from->stack_frames_start,
                        ctx_from->stack_frames_len);
    }
    
    if (has_start_) {
        assert(ctx_from->stack_frames_len < sizeof(ctx_from->stack_frames));
    
        RevertStackFrames(co_context.stack_frames, co_context.stack_frames_start,
                          co_context.stack_frames_len);
        SwitchCoroutineContext(&_curr->co_context, &co_context);
        
    } else {
        assert(co_context.co_resume_addr);
        has_start_ = true;
        StartCoroutine(&_curr->co_context, &co_context);
    }
}

void CoroutineProfile::SetEntry(
        CoroutineProfile::CoEntry _entry) {
    co_context.co_resume_addr = (void *) _entry;
}

uint64_t CoroutineProfile::CoUid() const { return co_uid; }

void CoroutineProfile::__CoEntryWrap() {

}

void *Producer(void *) {
    int i = 0;
    while (true) {
        printf("Produce %d\n", i++);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CoroutineDispatcher::Instance().CoYieldCurr();
    }
    return nullptr;
}

void *Consumer(void *) {
    int i = 0;
    while (true) {
        printf("Consume %d\n", i++);
        CoroutineDispatcher::Instance().CoYieldCurr();
    }
    return nullptr;
}


void CoYield() {
}

int main() {
    CoroutineProfile co1(Producer);
    CoroutineProfile co2(Consumer);
    CoroutineDispatcher::Instance().AddCoroutine(&co1);
    CoroutineDispatcher::Instance().AddCoroutine(&co2);
    CoroutineDispatcher::Instance().CoResume(&co1);
    
    printf("-%llu\n", CoroutineDispatcher::Instance().MainCoro()->co_context.rbp);
    printf("-%llu\n", CoroutineDispatcher::Instance().MainCoro()->co_context.rsp);
}

CoroutineDispatcher::CoroutineDispatcher() {
    coroutines_.push_back(&main_coro_);
    curr_coro_ = coroutines_.begin();
}

void CoroutineDispatcher::CoResume(CoroutineProfile *_co) {
    CoroutineProfile *from_co = *curr_coro_;
    for (auto it = coroutines_.begin(); it != coroutines_.end(); it++) {
        if ((*it) == _co) {
            curr_coro_ = it;
            break;
        }
    }
    assert(curr_coro_ != coroutines_.end());
    (*curr_coro_)->CoResumeSelf(from_co);
}

void CoroutineDispatcher::AddCoroutine(CoroutineProfile *_co) {
    coroutines_.push_back(_co);
}

void CoroutineDispatcher::DelCoroutine(CoroutineProfile *) {

}

void CoroutineDispatcher::CoYieldCurr() {
    CoroutineProfile *from_co = *curr_coro_;
    if (++curr_coro_ == coroutines_.end()) {
        curr_coro_ = coroutines_.begin();
        ++curr_coro_;
    }
    (*curr_coro_)->CoResumeSelf(from_co);
}

CoroutineProfile *CoroutineDispatcher::CurrCoro() const { return *curr_coro_; }

CoroutineProfile *CoroutineDispatcher::MainCoro() { return &main_coro_; }

CoroutineDispatcher::~CoroutineDispatcher() = default;

