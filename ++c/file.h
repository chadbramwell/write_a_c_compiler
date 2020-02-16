#pragma once

void file_dump_to_stdout(const char* filename);
bool file_read_into_stretchy_memory(const char* filename, size_t* o_size, char** io_buffer, size_t* io_buffer_size);
char* file_read_into_memory(const char* filename, size_t* o_size);
