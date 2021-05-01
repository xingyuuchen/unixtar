#pragma once
#include <cstdio>
#include <string>


namespace file {

/**
 *
 * @return: Bytes count written upon successful completion,
 *          else 0, indicating nothing was written.
 */
size_t WriteFile(const char *_path, const void *_ptr, size_t _len);

/**
 *
 * @param _path: file path
 * @param _res: the string which contains the file data.
 * @return: true on success, else false.
 */
bool ReadFile(const char *_path, std::string &_res);


long GetFileSize(const char *_path);

bool IsFileExist(const char *_path);

}

