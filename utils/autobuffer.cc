#include "autobuffer.h"
#include <cstring>
#include <cassert>
#include <cstdio>
#include "log.h"


AutoBuffer::AutoBuffer(size_t _malloc_unit_size)
        : byte_array_p_(nullptr)
        , is_shallow_copy_(false)
        , pos_(0)
        , length_(0)
        , capacity_(0)
        , malloc_unit_size_(_malloc_unit_size) {}


void AutoBuffer::Write(const char *_byte_array, size_t _len) {
    if (_len <= 0 || _byte_array == nullptr) { return; }
    assert(!is_shallow_copy_);
    if (capacity_ < length_ + _len) {
        AddCapacity(length_ + _len - capacity_);
    }
    memcpy(Ptr(length_), _byte_array, _len);
    length_ += _len;
}

size_t AutoBuffer::Pos() const {
    return pos_;
}

void AutoBuffer::Seek(TWhence _whence, size_t _pos /* = 0 */) {
    if (_whence == kStart) {
        pos_ = 0;
    } else if (_whence == kCurrent) {
        pos_ += _pos;
    } else if (_whence == kEnd) {
        pos_ = Length();
    } else {
        LogE("Invalid TWhence: %d", _whence)
        assert(false);
    }
}

size_t AutoBuffer::Length() const {
    return length_;
}

char *AutoBuffer::Ptr(const size_t _offset /* = 0*/) const {
    return byte_array_p_ + _offset;
}

void AutoBuffer::AddCapacity(size_t _size_to_add) {
    if (_size_to_add <= 0) {
        LogE("Illegal arg _size_to_add: %zd", _size_to_add);
        return;
    }
    assert(!is_shallow_copy_);
    
    if (_size_to_add % malloc_unit_size_ != 0) {
        _size_to_add = (_size_to_add / malloc_unit_size_ + 1) * malloc_unit_size_;
    }
    
    byte_array_.resize(capacity_ + _size_to_add);
    capacity_ += _size_to_add;
    byte_array_p_ = byte_array_.data();
}

size_t AutoBuffer::Capacity() const {
    return capacity_;
}

size_t AutoBuffer::AvailableSize() const {
    return capacity_ - length_;
}

AutoBuffer::~AutoBuffer() {
    Reset();
}

void AutoBuffer::ShallowCopyFrom(char *_ptr, size_t _len) {
    byte_array_p_ = _ptr;
    SetLength(_len);
    is_shallow_copy_ = true;
    byte_array_.clear();
}

void AutoBuffer::Reset() {
    capacity_ = 0;
    length_ = 0;
    pos_ = 0;
    byte_array_.clear();
    byte_array_p_ = nullptr;
    is_shallow_copy_ = false;
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
