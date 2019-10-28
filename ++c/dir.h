#pragma once
#include "stdint.h"

// NOT THREAD SAFE
// NOT THREAD SAFE (at least on Win32, FindFirstFileA uses global state)
// NOT THREAD SAFE

struct DirectoryIter;

bool dopen(DirectoryIter** io_iter, const char* path, const char* filter = "*");
void dclose(DirectoryIter** io_iter);
bool dnext(DirectoryIter* io_iter);

bool disdir(DirectoryIter* io_iter);
bool dendswith(DirectoryIter* io_iter, const char* str);
const char* dfpath(DirectoryIter* io_iter);

bool get_absolute_path(const char* partial_path, char(*out_abs_path)[260]);
