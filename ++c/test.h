#pragma once

void interpreter_practice();

bool run_ir_tests();
int run_all_tests();
int run_tests_on_folder(int folder_index, bool verbose);
int get_clang_ground_truth(const char* source_path);
