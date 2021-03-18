#ifndef OI_SVR_AUTOBUFFER_H
#define OI_SVR_AUTOBUFFER_H
#include <stddef.h>


class AutoBuffer {
  public:
    AutoBuffer(size_t _malloc_unit_size = 128);

    AutoBuffer(const AutoBuffer &_auto_buffer) = delete;
    
    ~AutoBuffer();
    
    void Write(const char *_byte_array, size_t _len);
    
    size_t Pos() const;
    
    size_t Length() const;
    
    void SetLength(size_t _len);
    
    void AddLength(size_t _len);
    
    char *Ptr(const size_t _offset = 0) const;
    
    void SetPtr(char *_ptr);
    
    size_t GetCapacity() const;
    
    size_t AvailableSize() const;
    
    void AddCapacity(size_t _size);
    
    void Seek(const size_t _pos);
    
    void Reset();
    
    void ShareFromOther(bool _val);

  private:
    char *              byte_array_;
    bool                share_from_other_;
    size_t              pos_;
    size_t              length_;
    size_t              capacity_;
    const size_t        malloc_unit_size_;
    
};


#endif //OI_SVR_AUTOBUFFER_H
