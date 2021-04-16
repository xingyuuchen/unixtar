#include "fileutil.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "log.h"

namespace file {

size_t WriteFile(const char *_path, const void *_ptr, size_t _len) {
    FILE *fp = ::fopen(_path, "wb");
    if (!fp) {
        LogE("open file failed, path: %s, errno(%d): %s",
             _path, errno, strerror(errno))
        return 0;
    }
    LogI("fp: %p, path: %s, len: %ld", fp, _path, _len)
    size_t nwrite = ::fwrite(_ptr, _len, 1, fp);
    if (nwrite < 1) {
        LogE("write: %ld", nwrite)
        return nwrite;
    }
    ::fclose(fp);
    return _len;
}

bool ReadFile(const char *_path, std::string &_res) {
    FILE *fp = ::fopen(_path, "rb");
    if (!fp) {
        LogE("open file failed: %s", _path)
        return false;
    }
    size_t nread;
    while (true) {
        char buff[4096] = {0, };
        nread = ::fread(buff, 1, sizeof(buff), fp);
        if (nread > 0) {
            _res.append(buff, nread);
            continue;
        }
        break;
    }
    ::fclose(fp);
    return true;
}

long GetFileSize(const char *_path) {
    FILE *fp = ::fopen(_path, "rb");
    if (!fp) {
        LogE("open file failed, path: %s, errno(%d): %s",
             _path, errno, strerror(errno));
        return -1;
    }
    ::fseek(fp, 0, SEEK_END);
    long file_size = ::ftell(fp);
    ::fclose(fp);
    return file_size;
}

bool IsFileExist(const char *_path) {
    return ::access(_path, F_OK) == 0;
}

}