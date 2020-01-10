#include "file.h"
#include "debug.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

void file_dump_to_stdout(const char* filename)
{
    size_t size;
    const char* data = file_read_into_memory(filename, &size);
    assert(data);
    fwrite(data, 1, (size_t)size, stdout);
}

char* file_read_into_memory(const char* filename, size_t* o_size)
{
    FILE* file;
    if (0 != fopen_s(&file, filename, "rb"))
    {
        printf("failed to open file %s\n", filename);
        debug_break();
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    uint32_t file_size = ftell(file);
    rewind(file);

    void* memory = malloc(file_size);
    size_t actually_read = fread_s(memory, file_size, 1, file_size, file);
    fclose(file);

    if (file_size != actually_read)
    {
        printf("failed to read file %s of size %" PRIu32 "\n", filename, file_size);
        debug_break();
        free(memory);
        return NULL;
    }

    *o_size = file_size;
    return (char*)memory;
}
