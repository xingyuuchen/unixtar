#include "autobuffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "log.h"
#include <errno.h>


AutoBuffer::AutoBuffer(size_t _malloc_unit_size)
        : byte_array_(nullptr)
        , is_shallow_copy_(false)
        , pos_(0)
        , length_(0)
        , capacity_(0)
        , malloc_unit_size_(_malloc_unit_size) {}


void AutoBuffer::Write(const char *_byte_array, size_t _len) {
    if (_len <= 0 || _byte_array == nullptr) { return; }
    if (capacity_ < length_ + _len) {
        AddCapacity(length_ + _len - capacity_);
    }
    memcpy(Ptr(length_), _byte_array, _len);
    length_ += _len;
}

size_t AutoBuffer::Pos() const {
    return pos_;
}

void AutoBuffer::Seek(const size_t _pos) {
    pos_ = _pos;
}

size_t AutoBuffer::Length() const {
    return length_;
}

char *AutoBuffer::Ptr(const size_t _offset/* = 0*/) const {
    return byte_array_ + _offset;
}

void AutoBuffer::SetPtr(char *_ptr) {
    byte_array_ = _ptr;
}

void AutoBuffer::AddCapacity(size_t _size_to_add) {
    if (_size_to_add <= 0) {
        LogE("Illegal arg _size_to_add: %zd", _size_to_add);
        return;
    }
    if (_size_to_add % malloc_unit_size_ != 0) {
        _size_to_add = (_size_to_add / malloc_unit_size_ + 1) * malloc_unit_size_;
    }
    
    void *p = realloc(byte_array_, capacity_ + _size_to_add);
    if (!p) {
        LogE("realloc failed, errno(%d): %s", errno, strerror(errno));
        free(byte_array_), byte_array_ = nullptr;
        capacity_ = 0;
        return;
    }
    capacity_ += _size_to_add;
    byte_array_ = (char *) p;
}

size_t AutoBuffer::GetCapacity() const {
    return capacity_;
}

size_t AutoBuffer::AvailableSize() const {
    return capacity_ - length_;
}

AutoBuffer::~AutoBuffer() {
    Reset();
}

void AutoBuffer::ShallowCopy(bool _val) {
    is_shallow_copy_ = _val;
}

void AutoBuffer::Reset() {
    if (is_shallow_copy_) { return; }
    capacity_ = 0;
    length_ = 0;
    pos_ = 0;
    if (byte_array_) {
        free(byte_array_), byte_array_ = nullptr;
    }
}

void AutoBuffer::SetLength(size_t _len) {
    if (_len >= 0) {
        length_ = _len;
    }
}

void AutoBuffer::AddLength(size_t _len) {
    if (_len > 0) {
        length_ += _len;
    }
}
