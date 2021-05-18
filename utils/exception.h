#pragma once
#include <string>
#include <exception>


namespace unixtar {

class Exception : public std::exception {
  public:
    
    explicit Exception(std::string &&_msg);
    
    explicit Exception(char *_msg);
    
    Exception(const Exception&) noexcept;
    
    Exception& operator=(const Exception&) noexcept;
    
    const char *what() const noexcept override;
    
    ~Exception() override;
    
  private:
    std::string msg_;
};

}
