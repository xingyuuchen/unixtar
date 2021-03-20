#include "fileutil.h"
#include <errno.h>
#include <string.h>
#include "log.h"

namespace File {

size_t WriteFileBin(const char *_path, const void *_ptr, size_t _len) {
    FILE *fp = fopen(_path, "wb");
    if (!fp) {
        LogE(__FILE__, "[WriteFileBin] open file failed, path: %s, errno(%d): %s",
             _path, errno, strerror(errno))
        return 0;
    }
    LogI(__FILE__, "[WriteFileBin] fp: %p, path: %s, len: %ld", fp, _path, _len)
    size_t nwrite = fwrite(_ptr, _len, 1, fp);
    if (nwrite < 1) {
        LogE(__FILE__, "[WriteFileBin] write: %ld", nwrite)
        return nwrite;
    }
    fclose(fp);
    return _len;
}

}