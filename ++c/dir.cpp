#include "dir.h"
#include "debug.h"
#include "stdio.h"
#include "windows.h"

struct DirectoryIter
{
    char fpath[MAX_PATH];
    char* fname; //points into fpath
    size_t room_left_for_fname;
    
    WIN32_FIND_DATAA data;
    HANDLE handle;
};

#define TO_STR(x) #x

bool is_slash(char c)
{
    return c == '\\' || c == '/';
}

bool dopen(DirectoryIter** io_iter, const char* path, const char* filter)
{
    if (!io_iter)
    {
        debug_break();
        return false;
    }
    *io_iter = NULL; // will get set on success.

    DirectoryIter* dir = (DirectoryIter*)malloc(sizeof(DirectoryIter));
    

    DWORD strlen = GetFullPathNameA(path, MAX_PATH, dir->fpath, &dir->fname);
    assert(strlen != 0);
    if (dir->fname == NULL)
        dir->fname = dir->fpath + strlen;

    dir->room_left_for_fname = MAX_PATH - strlen;


    // temporarily using dir.fpath for directory searching
    errno_t err = strcpy_s(dir->fname, dir->room_left_for_fname, filter);
    if (err)
    {
        debug_break();
        return err;
    }
    dir->handle = FindFirstFileA(dir->fpath, &dir->data);

    if (dir->handle == INVALID_HANDLE_VALUE)
    {
        DWORD error_code = GetLastError(); //https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes
        printf("dopen failed to open (%s) with error (%d)\n", dir->fpath, error_code);

        if (error_code == ERROR_PATH_NOT_FOUND)
        {
            char cwd[MAX_PATH];
            GetCurrentDirectoryA(MAX_PATH, cwd);
            printf("\t" TO_STR(ERROR_PATH_NOT_FOUND) "(%d) - could not be found from current working directory: (%s)\n", ERROR_PATH_NOT_FOUND, cwd);
        }
        else if (error_code == ERROR_INVALID_PARAMETER)
            printf("\t" TO_STR(ERROR_INVALID_PARAMETER) "(%d) - most likely caused by not adding '*' at end of path\n", ERROR_INVALID_PARAMETER);
        else if (error_code == ERROR_FILE_NOT_FOUND)
            printf("\t" TO_STR(ERROR_FILE_NOT_FOUND) "(%d) - uhh... no files found for that type... I think...\n", ERROR_FILE_NOT_FOUND);

        debug_break();
        return false;
    }

    strcpy_s(dir->fname, dir->room_left_for_fname, dir->data.cFileName);
    *io_iter = dir;
    return true;
}

void dclose(DirectoryIter** io_iter)
{
    free(*io_iter);
    *io_iter = NULL;
}

bool dnext(DirectoryIter* io_iter)
{
    BOOL ok = FindNextFileA(io_iter->handle, &io_iter->data);

    if (ok)
        strcpy_s(io_iter->fname, io_iter->room_left_for_fname, io_iter->data.cFileName);

    return ok;
}

bool disdir(DirectoryIter* dir)
{
    return dir->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
}

bool dendswith(DirectoryIter* io_iter, const char* str)
{
    const char* si = str;
    while (*si) ++si;

    const char* ni = io_iter->fname;
    while (*ni) ++ni;

    while (si >= str && ni >= io_iter->fname)
    {
        if (*si != *ni)
            return false;
        --si;
        --ni;
    }

    return true;
}

const char* dfpath(DirectoryIter* dir)
{
    return dir->fpath;
}

bool get_absolute_path(const char* partial_path, char(*out_abs_path)[MAX_PATH])
{
    DWORD copied_or_required_chars = GetFullPathNameA(partial_path, MAX_PATH, *out_abs_path, NULL);
    return copied_or_required_chars < MAX_PATH;
}
