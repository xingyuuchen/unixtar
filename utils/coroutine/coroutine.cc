#include "coroutine.h"
#include <cstring>
#include <cassert>


#ifdef __x86_64__
/**
 * 1. The output symbol to the linker is produced in the C language.
 *    If the output symbol is generated in the C++ way,
 *    the function name will be treated specially
 *    because the function can be overloaded,
 *    so the linker cannot find the corresponding symbol.
 *
 * 2. The function call rules are in the form of C language,
 *    such as register order for passing parameters.
 */
extern "C" {

void StartCoroutine(CoroutineContext *_from,
                    CoroutineContext *_to)
                    asm("StartCoroutine");

void SwitchCoroutine(CoroutineContext *_from,
                     CoroutineContext *_to)
                     asm("SwitchCoroutine");

void GetRsp(uint64_t *_rsp) asm("GetRsp");

void SaveStackFrames(void *_buf, void *_stack_start, uint64_t _size)
                     asm("SaveStackFrames");

void RestoreStackFrames(void *_buf, void *_stack_start, uint64_t _size)
                       asm("RestoreStackFrames");

}
#else
// Only support x64.
#endif


void InitialCoContext(CoroutineContext *_co_ctx) {
    memset(_co_ctx, 0, sizeof(CoroutineContext));
    _co_ctx->stack_frames_buf =
            (char *) malloc(CoroutineProfile::kCoStackFramesBufMallocUnit);
    _co_ctx->stack_frames_buf_capacity =
            CoroutineProfile::kCoStackFramesBufMallocUnit;
}

void DestroyCoContext(CoroutineContext *_co_ctx) {
    free(_co_ctx->stack_frames_buf);
    _co_ctx->stack_frames_buf = nullptr;
}


uint64_t CoroutineProfile::kInvalidUid = 0;
const size_t CoroutineProfile::kCoStackFramesBufMallocUnit = 1024;
const size_t CoroutineProfile::kMaxCoStackFramesBuffSize = 102400;

CoroutineProfile::CoroutineProfile(CoEntry _entry)
        : co_ctx_()
        , co_uid_(kInvalidUid)
        , co_entry_(_entry)
        , has_start_(false) {
    
    static uint64_t curr_uid = kInvalidUid;
    co_uid_ = ++curr_uid;
    InitialCoContext(&co_ctx_);
    co_ctx_.co_resume_addr = (void *) co_entry_;
}

void CoroutineProfile::CoResumeSelf(CoroutineProfile *_curr) {
    assert(this != _curr);
    auto *ctx_from = &_curr->co_ctx_;
    uint64_t rsp;
    GetRsp(&rsp);
#ifdef __x86_64__
    rsp += 8;   // `addq $8, %rsp` because this is a procedure call.
#endif
    
    // save stack frames of `_curr`, except for main coroutine.
    if (_curr->co_ctx_.stack_frames_start) {
        if ((uint64_t) ctx_from->stack_frames_start < rsp) {
            assert(false);
        }
        // x86 full descent stack.
        ctx_from->stack_frames_len =
                    (uint64_t) ctx_from->stack_frames_start - rsp + 8;
        if (ctx_from->stack_frames_len > ctx_from->stack_frames_buf_capacity) {
            assert(ctx_from->stack_frames_len < kMaxCoStackFramesBuffSize);
            
            uint64_t malloc_unit = CoroutineProfile::kCoStackFramesBufMallocUnit;
            uint64_t new_capacity = ctx_from->stack_frames_len;
            if (new_capacity % malloc_unit != 0) {
                new_capacity = (new_capacity / malloc_unit + 1) * malloc_unit;
            }
            ctx_from->stack_frames_buf =
                    (char *) realloc(ctx_from->stack_frames_buf, new_capacity);
            assert(ctx_from->stack_frames_buf);
            ctx_from->stack_frames_buf_capacity = new_capacity;
        }
    
        SaveStackFrames(ctx_from->stack_frames_buf, ctx_from->stack_frames_start,
                        ctx_from->stack_frames_len);
    }
    
    if (has_start_) {
        assert(ctx_from->stack_frames_len < kMaxCoStackFramesBuffSize);
    
        RestoreStackFrames(co_ctx_.stack_frames_buf, co_ctx_.stack_frames_start,
                           co_ctx_.stack_frames_len);
        SwitchCoroutine(&_curr->co_ctx_, &co_ctx_);
        
    } else {
        assert(co_ctx_.co_resume_addr);
        has_start_ = true;
        StartCoroutine(&_curr->co_ctx_, &co_ctx_);
    }
}

void CoroutineProfile::SetEntry(
        CoroutineProfile::CoEntry _entry) {
    assert(!has_start_);
    co_ctx_.co_resume_addr = (void *) _entry;
}

uint64_t CoroutineProfile::CoUid() const { return co_uid_; }

void CoroutineProfile::CoEntryWrapper() const {

}

CoroutineProfile::~CoroutineProfile() {
    DestroyCoContext(&co_ctx_);
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

