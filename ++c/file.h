#pragma once

void file_dump_to_stdout(const char* filename);
char* file_read_into_memory(const char* filename, size_t* o_size);
