#pragma once
#include "stdint.h"

// NOT THREAD SAFE
// NOT THREAD SAFE (at least on Win32, FindFirstFileA uses global state)
// NOT THREAD SAFE

struct DirectoryIter;

DirectoryIter* dopen(const char* path, const char* filter = "*");
bool dnext(DirectoryIter* dir);

bool disdir(DirectoryIter* dir);
const char* dfname(DirectoryIter* dir);
const char* dfpath(DirectoryIter* dir);
uint64_t dfsize(DirectoryIter* dir);
