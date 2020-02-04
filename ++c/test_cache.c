#include "test_cache.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct cached_test
{
    uint32_t path_hash;
    int32_t exit_code;
};
static struct cached_test* g_tests;
static size_t g_tests_len;
static uint32_t g_cache_misses;

uint32_t get_test_cache_misses(void)
{
    return g_cache_misses;
}
// https://stackoverflow.com/questions/11413860/best-string-hashing-function-for-short-filenames
uint32_t test_cache_path_hash(const char* path)
{
    size_t len = strlen(path);
    unsigned char* p = (unsigned char*)path;
    uint32_t h = 2166136261;

    for (size_t i = 0; i < len; i++)
        h = (h * 16777619) ^ p[i];

    return h;
}
bool get_cached_test_result(uint32_t path_hash, int32_t* result)
{
    for(size_t i = 0; i < g_tests_len; ++i)
    {
        if (g_tests[i].path_hash == path_hash)
        {
            *result = g_tests[i].exit_code;
            return true;
        }
    }
    ++g_cache_misses;
    return false;
}
void add_cached_test_result(uint32_t path_hash, int32_t result)
{
    g_tests = realloc(g_tests, (g_tests_len + 1) * sizeof(struct cached_test));

    g_tests[g_tests_len].path_hash = path_hash;
    g_tests[g_tests_len].exit_code = result;
    ++g_tests_len;
}
void save_test_results(void)
{
    FILE* file;
    errno_t err = fopen_s(&file, "tests.cache", "wb");
    if (err)
    {
        debug_break();
        return;
    }
    const size_t cache_size = g_tests_len * sizeof(struct cached_test);
    size_t wrote = fwrite(g_tests, 1, cache_size, file);
    if (wrote != cache_size)
    {
        debug_break();
        fclose(file);
        return;
    }
    fclose(file);
}
void load_test_results(void)
{
    FILE* file;
    errno_t err = fopen_s(&file, "tests.cache", "rb");
    if (err)
    {
        return;
    }
    fseek(file, 0, SEEK_END);
    size_t len = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (len == 0 || len % sizeof(struct cached_test) != 0)
    {
        debug_break();
        fclose(file);
        return;
    }
    g_tests = realloc(g_tests, len);
    size_t read = fread_s(g_tests, len, 1, len, file);
    fclose(file);
    if (read != len)
    {
        debug_break();
        free(g_tests);
        g_tests = NULL;
        g_tests_len = 0;
        return;
    }
    g_tests_len = len / sizeof(struct cached_test);
}