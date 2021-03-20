#pragma once
#include <stdio.h>


namespace File {

/**
 *
 * @return: Bytes count written upon successful completion,
 *          else 0, indicating nothing was written.
 */
size_t WriteFileBin(const char *_path, const void *_ptr, size_t _len);

}

