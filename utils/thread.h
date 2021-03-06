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
    
    virtual void OnStart();
    
    void Join();
    
    virtual void OnStop();
    
    void Detach();
    
    bool IsRunning() const;
    
    std::thread::id Tid() const;

  protected:
    bool            running_;
    
  private:
    std::thread *   thread_;
    
};

