#include "dir.h"
#include "stdio.h"
#include "stdlib.h"

int replace_tabs_in(const char* path, const char* filter)
{
    DirectoryIter* iter = dopen(path, filter);
    if (!iter)
    {
        printf("failed to open directory %s with filter %s\n", path, filter);
        return 1;
    }

    do
    {
        if (disdir(iter))
            continue;

        FILE* file;
        errno_t err = fopen_s(&file, dfpath(iter), "rb+");
        if (err != 0)
        {
            printf("failed to open %s. got error %d\n", dfpath(iter), err);
            return 2;
        }

        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char* buff = (char*)malloc(file_size);
        size_t read = fread_s(buff, file_size, 1, file_size, file);
        if (read != file_size)
        {
            printf("failed to read full file into memory. expected %d, got %d\n", file_size, read);
            return 3;
        }
        fseek(file, 0, SEEK_SET);

        char* iter = buff;
        while (iter != buff + file_size)
        {
            if (*iter == '\t')
            {
                fwrite("    ", 1, 4, file);
                ++iter;
            }
            else
            {
                fwrite(iter, 1, 1, file);
                ++iter;
            }
        }

        fclose(file);
        free(buff);

    } while (dnext(iter));

    return 0;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("expected directory as first argument\n");
        return 1;
    }

    int err = replace_tabs_in(argv[1], "*.cpp");
    if (err != 0) return err;
    err = replace_tabs_in(argv[1], "*.h");
    return err;
}
