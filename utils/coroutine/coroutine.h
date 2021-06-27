#pragma once
/**
 *
 * Currently the coroutine only supports x86-64 arch.
 *
 */
#include <cstdint>
#include <list>


struct CoroutineContext {     // POD.
    uint64_t    rax;
    uint64_t    rbx;
    uint64_t    rcx;
    uint64_t    rdx;
    uint64_t    rbp;
    uint64_t    rsp;
    uint64_t    rsi;
    uint64_t    rdi;
    uint64_t    r8;
    uint64_t    r9;
    uint64_t    r10;
    uint64_t    r11;
    uint64_t    r12;
    uint64_t    r13;
    uint64_t    r14;
    uint64_t    r15;
    void      * co_resume_addr;
    void      * stack_frames_start;     // 136
    uint64_t    stack_frames_len;
    char      * stack_frames_buf;
    uint64_t    stack_frames_buf_capacity;
}__attribute__((packed));   // compact, no align.

// No constructor nor destructor in CoroutineContext to make it a POD.
void InitialCoContext(CoroutineContext *);
void DestroyCoContext(CoroutineContext *);


class CoroutineProfile {
  public:
    using CoEntry = void *(*)(void *);
    
    explicit CoroutineProfile(CoEntry = nullptr);
    
    ~CoroutineProfile();
    
    void CoResumeSelf(CoroutineProfile *_from);
    
    void SetEntry(CoEntry _entry);
    
    uint64_t CoUid() const;

  private:
    void CoEntryWrapper() const;

  public:
    static uint64_t                 kInvalidUid;
    static const size_t             kCoStackFramesBufMallocUnit;
    static const size_t             kMaxCoStackFramesBuffSize;
    uint64_t                        co_uid_;
    CoroutineContext                co_ctx_;
    CoEntry                         co_entry_;
    bool                            has_start_;
};


class CoroutineDispatcher {
  public:
    static CoroutineDispatcher &Instance() {
        static CoroutineDispatcher instance;
        return instance;
    }
    
    CoroutineDispatcher();
    
    ~CoroutineDispatcher();
    
    void CoResume(CoroutineProfile *);
    
    void AddCoroutine(CoroutineProfile *);
    
    void DelCoroutine(CoroutineProfile *);
    
    void CoYieldCurr();
    
    CoroutineProfile *CurrCoro() const;
    
    CoroutineProfile *MainCoro();


  private:
    CoroutineProfile                            main_coro_;
    std::list<CoroutineProfile *>               coroutines_;
    std::list<CoroutineProfile *>::iterator     curr_coro_;
};


