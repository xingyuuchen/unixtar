#pragma once

#include <cstddef>
#include <vector>


class AutoBuffer {
  public:
    explicit AutoBuffer(size_t _malloc_unit_size = 128);

    AutoBuffer(const AutoBuffer &_auto_buffer) = delete;
    
    ~AutoBuffer();
    
    void Write(const char *_byte_array, size_t _len);
    
    size_t Pos() const;
    
    size_t Length() const;
    
    void SetLength(size_t _len);
    
    void AddLength(size_t _len);
    
    char *Ptr(size_t _offset = 0) const;
    
    size_t Capacity() const;
    
    size_t AvailableSize() const;
    
    void AddCapacity(size_t _size);
    
    enum TWhence {
        kStart = 0,
        kCurrent,
        kEnd,
    };
    void Seek(TWhence _whence, size_t _pos = 0);
    
    void Reset();
    
    void ShallowCopyFrom(char *_ptr, size_t _len);

  private:
    char              * byte_array_p_;
    bool                is_shallow_copy_;
    std::vector<char>   byte_array_;
    size_t              pos_;
    size_t              length_;
    size_t              capacity_;
    const size_t        malloc_unit_size_;
    
};

