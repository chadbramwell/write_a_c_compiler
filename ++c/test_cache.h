#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

    uint32_t get_test_cache_misses(void);
    uint32_t test_cache_path_hash(const char* path);
    bool get_cached_test_result(uint32_t path_hash, int32_t* result);
    void add_cached_test_result(uint32_t path_hash, int32_t result); // up to user to ensure result isn't already in the cache
    void save_test_results(void);
    void load_test_results(void);

#ifdef __cplusplus
}
#endif
