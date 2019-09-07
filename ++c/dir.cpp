#include "dir.h"
#include "debug.h"
#include "stdio.h"
#include "windows.h"

struct DirectoryIter
{
	char fpath[MAX_PATH];
	size_t dpath_cutoff_of_fpath;
	
	WIN32_FIND_DATAA data;
	HANDLE handle;
};

#define TO_STR(x) #x

DirectoryIter* dopen(const char* path, const char* filter)
{
	DirectoryIter* dir_ptr = (DirectoryIter*)malloc(sizeof(DirectoryIter));
	DirectoryIter& dir = *dir_ptr;
	dir.dpath_cutoff_of_fpath = strlen(path);
	memcpy(dir.fpath, path, dir.dpath_cutoff_of_fpath);

	// temporarily using dir.fpath for directory searching
	size_t filter_len = strlen(filter);
	memcpy(dir.fpath + dir.dpath_cutoff_of_fpath, filter, filter_len);
	dir.fpath[dir.dpath_cutoff_of_fpath + filter_len] = 0;
	dir.handle = FindFirstFileA(dir.fpath, &dir.data);

	if (dir.handle == INVALID_HANDLE_VALUE)
	{
		DWORD error_code = GetLastError(); //https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes
		printf("dopen failed to open (%s) with error (%d)\n", path, error_code);

		if (error_code == ERROR_PATH_NOT_FOUND)
		{
			char cwd[MAX_PATH];
			GetCurrentDirectoryA(MAX_PATH, cwd);
			printf("\t" TO_STR(ERROR_PATH_NOT_FOUND) "(%d) - could not be found from current working directory: (%s)", ERROR_PATH_NOT_FOUND, cwd);
		}
		if (error_code == ERROR_INVALID_PARAMETER)
			printf("\t" TO_STR(ERROR_INVALID_PARAMETER) "(%d) - most likely caused by not adding '*' at end of path", ERROR_INVALID_PARAMETER);

		debug_break();
		free(dir_ptr);
		return NULL;
	}

	size_t fname_len = strlen(dir.data.cFileName);
	memcpy(dir.fpath + dir.dpath_cutoff_of_fpath, dir.data.cFileName, fname_len);
	dir.fpath[dir.dpath_cutoff_of_fpath + fname_len] = 0;

	return dir_ptr;
}

bool dnext(DirectoryIter* dir_ptr)
{
	DirectoryIter& dir = *dir_ptr;

	BOOL ok = FindNextFileA(dir.handle, &dir.data);

	if (ok)
	{
		size_t fname_len = strlen(dir.data.cFileName);
		memcpy(dir.fpath + dir.dpath_cutoff_of_fpath, dir.data.cFileName, fname_len);
		dir.fpath[dir.dpath_cutoff_of_fpath + fname_len] = 0;
	}

	return ok;
}

bool disdir(DirectoryIter* dir)
{
	return dir->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
}

const char* dfname(DirectoryIter* dir)
{
	return dir->data.cFileName;
}

const char* dfpath(DirectoryIter* dir)
{
	return dir->fpath;
}

uint64_t dfsize(DirectoryIter* dir)
{
	uint64_t size = dir->data.nFileSizeHigh;
	size <<= 32;
	size |= dir->data.nFileSizeLow;
	return size;
}

char g_full_path[MAX_PATH];
const char* get_absolute_path(const char* partial_path)
{
	DWORD copied_or_required_chars = GetFullPathNameA(partial_path, MAX_PATH, g_full_path, NULL);
	assert(copied_or_required_chars < MAX_PATH);
	return g_full_path;
}