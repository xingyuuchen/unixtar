#pragma once
#include <thread>
#include <stdexcept>


class Thread {
  public:
    Thread();
    
    virtual ~Thread();
    
    virtual void Run() = 0;
    
    virtual void HandleException(std::exception &ex);
    
    void Entry();
    
    void Start();
    
    virtual void OnStarted();
    
    virtual void OnStop();
    
    void Join();
    
    void Detach();
    
    bool IsRunning() const;
    
    std::thread::id Tid() const;

  protected:
    bool            running_;
    
  private:
    std::thread *   thread_;
    
};

