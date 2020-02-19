#include "test.h"
#include "lex.h"
#include "ir.h"
#include "ast.h"
#include "gen.h"
#include "simplify.h"
#include "dir.h"
#include "timer.h"
#include "debug.h"
#include "interp.h"
#include "file.h"
#include "test_cache.h"


#include <string>
#include <vector>

struct perf_numbers
{
    uint64_t total_tests = 0;
    float test_cache_load;
    float test_cache_save;
    std::vector<float> read_file;
    std::vector<float> invalid_lex;
    std::vector<float> lex;
    std::vector<float> lex_strip;
    std::vector<float> ir;
    std::vector<float> ast;
    std::vector<float> gen_asm;
    std::vector<float> gen_asm_from_ir;
    std::vector<float> gen_exe;
    std::vector<float> run_exe;
    std::vector<float> ground_truth;
    std::vector<float> interp;
    std::vector<float> cleanup;
};
struct test_config
{
    bool lex;
    bool ir;
    bool ast;
    bool gen;

    bool simplify;
    bool interp;

    bool dump;
    bool print_file_path;
    bool expect_lex_fail;
};
struct test_iter
{
    const char* file_path;

    size_t file_length;
    const char* file_data;

    LexOutput lex_out;
    LexInput lex_in;

    IR* ir;
    size_t ir_size;

    ASTOut ast;

    char asm_file_path[L_tmpnam_s + 2]; // NOTE: +2 for .s
    char exe_file_path[L_tmpnam_s + 4]; // NOTE: +4 for .exe
    char clang_buffer[1024];

    int64_t interp_result;
    int clang_ground_truth;
    int our_result;
};


int get_clang_ground_truth(const char* source_path)
{
    uint32_t path_hash = test_cache_path_hash(source_path);

    int32_t ground_truth;
    if (!get_cached_test_result(path_hash, &ground_truth))
    {
        char buff[256];
        sprintf_s(buff, "clang %s", source_path);
        int compilation_result = system(buff);
        assert(compilation_result == 0);

        ground_truth = system("a.exe");

        int del_ok = system("del a.exe");
        assert(del_ok == 0);

        add_cached_test_result(path_hash, ground_truth);
    }

    return ground_truth;
}

static void update_perf(std::vector<float>* p, float ms)
{
    p->push_back(ms);
}
static bool get_perf(std::vector<float>* p, float* o_min, float* o_max, float* o_avg, float* o_total)
{
    float* iter = p->data();
    float* end = p->data() + p->size();

    if (iter == end)
        return false;

    float min, max, avg;
    min = max = avg = *iter;

    ++iter;

    for (; iter != end; ++iter)
    {
        avg += *iter;

        if (*iter < min)
            min = *iter;
        if (*iter > max)
            max = *iter;
    }

    *o_min = min;
    *o_max = max;
    *o_avg = avg / p->size();
    *o_total = avg;

    return true;
}
static float print_perf(std::vector<float>* p, const char* preamble, const char* postamble)
{
    float min, max, avg, total;
    if (!get_perf(p, &min, &max, &avg, &total))
        return 0.0f;

    printf("%s[%7zu, %8.2fms, %8.2fms, %8.2fms, %8.2fms]%s", preamble, p->size(), total, avg, min, max, postamble);
    return total;
}

static void cleanup_artifacts(std::vector<float>* p, const char* path)
{
    Timer t;
    t.start();

    DirectoryIter* dir = NULL;
    if (!dopen(&dir, path)) return;

    do
    {
        if (disdir(dir))
            continue;

        if (!dendswith(dir, ".ilk")
            && !dendswith(dir, ".pdb")
            && !dendswith(dir, ".s")
            && !dendswith(dir, ".exe"))
            continue;

        char delete_command[260];
        sprintf_s(delete_command, "del %s", dfpath(dir));
        system(delete_command);

    } while (dnext(dir));
    dclose(&dir);

    t.end();
    update_perf(p, t.milliseconds());
}

static void dump(test_config cfg, const test_iter& test)
{
    if (cfg.dump)
    {
        printf("===DEBUG INFO FOR [%s]===\n", test.lex_in.filename);
        printf("=== RAW FILE ===\n");
        fwrite(test.lex_in.stream, 1, (size_t)test.lex_in.length, stdout);
        printf("\n");
        if (cfg.lex) 
        {
            printf("=== LEX ===\n");
            dump_lex(stdout, &test.lex_out);
            printf("\n");
        }
        if (cfg.ast)
        {
            printf("=== AST ===\n");
            dump_ast(stdout, test.ast.root, 0);
            //dump_ast_errors(stdout, )
            printf("\n");
        }
        if (cfg.gen)
        {
            if (cfg.ast)
            {
                printf("=== GEN ASSEMBLY (from AST) ===\n");
                gen_asm(stdout, test.ast.root);
                printf("\n");
            }
            if (cfg.ir)
            {
                printf("=== GEN ASSEMBLY (from IR) ===\n");
                gen_asm_from_ir(stdout, test.ir, test.ir_size);
                printf("\n");
            }

            fprintf(stdout, "Clang's ASM==[\n");

            char temp_name[L_tmpnam];
            tmpnam_s(temp_name);

            char temp_cmd[256];
            sprintf_s(temp_cmd, "clang -S %s -o%s", test.lex_in.filename, temp_name);
            assert(0 == system(temp_cmd));
            file_dump_to_stdout(temp_name);
        }
        printf("=== END DEBUG INFO ===\n");
    }
}

static void Test(test_config cfg, perf_numbers* perf, const char* path)
{
    DirectoryIter* dir = NULL;
    if (!dopen(&dir, path, "*.c")) return;

    int test_count = 0;
    int test_fail = 0;
    bool success = true;

    do
    {
        struct test_iter test = {};

        if (disdir(dir))
            continue;

        ++test_count;

        test.file_path = dfpath(dir);
        if (cfg.print_file_path) printf("> %s\n", test.file_path);

        ///// READ FILE
        Timer timer;
        timer.start();
        test.file_data = file_read_into_memory(test.file_path, &test.file_length);
        if (!test.file_data)
        {
            printf("failed to read file %s\n", test.file_path);
            success = false;
            ++test_fail;
            continue;
        }
        timer.end();
        update_perf(&perf->read_file, timer.milliseconds());

        ///// LEX
        test.lex_in = init_lex(test.file_path, test.file_data, test.file_length);
        if (cfg.expect_lex_fail)
        {
            timer.start();
            if (lex(&test.lex_in, &test.lex_out))
            {
                debug_break();
                success = false;
                ++test_fail;
                printf("expected %s to fail lex but it succeeded?\n", test.file_path);
                continue;
            }
            timer.end();
            update_perf(&perf->invalid_lex, timer.milliseconds());
            continue;
        }
        else
        {
            LexOutput lexout_temp = {};
            timer.start();
            if (!lex(&test.lex_in, &lexout_temp))
            {
                debug_break();
                success = false;
                ++test_fail;
                printf("failed to lex file %s\nComparing to Clang error:\n", test.file_path);
                char buff[256];
                sprintf_s(buff, "clang %s", test.file_path);
                system(buff);
                continue;
            }
            timer.end();
            update_perf(&perf->lex, timer.milliseconds());
            timer.start();
            lex_strip_comments(&lexout_temp, &test.lex_out);
            timer.end();
            update_perf(&perf->lex_strip, timer.milliseconds());
        }

        ////// IR
        if (cfg.ir)
        {
            timer.start();
            bool ok = ir(
                test.lex_out.tokens, 
                test.lex_out.num_tokens, 
                &test.ir, 
                &test.ir_size);
            timer.end();
            assert(ok);

            update_perf(&perf->ir, timer.milliseconds());
        }

        ////// AST
        if (cfg.ast)
        {
            timer.start();
            if (!ast(test.lex_out.tokens, test.lex_out.num_tokens, &test.ast))
            {
                printf("failed to ast file %s\n", test.file_path);
                success = false;
                ++test_fail;

                test_config temp_cfg = {};
                temp_cfg.lex = true;
                temp_cfg.dump = true;

                dump(temp_cfg, test);
                debug_break();
                continue;
            }
            timer.end();
            update_perf(&perf->ast, timer.milliseconds());
        }

        // Calc Ground Truth
        if (cfg.gen || cfg.interp) {
            // - clang *.c
            // - run clangs' output: a.exe, and return result
            timer.start();
            test.clang_ground_truth = get_clang_ground_truth(test.file_path);
            timer.end();
            update_perf(&perf->ground_truth, timer.milliseconds());
            //printf("GROUND TRUTH [%s] = %d\n", file_path, clang_ground_truth);
        }

        ///// ASM
        if (cfg.gen)
        {
            errno_t err = tmpnam_s(test.asm_file_path);
            if (err) debug_break();
            err = tmpnam_s(test.exe_file_path);
            if (err) debug_break();

            // append .s to each temp file name
            {
                char* set_s = test.asm_file_path;
                while (*set_s) ++set_s;
                memcpy(set_s, ".s", 3); // 3 to include null-terminator

                set_s = test.exe_file_path;
                while (*set_s) ++set_s;
                memcpy(set_s, ".exe", 5); // 5 to include null-terminator
            }

            if (cfg.ast) {
                {
                    FILE* file;
                    err = fopen_s(&file, test.asm_file_path, "wb");
                    if (err) debug_break();

                    timer.start();
                    if (!gen_asm(file, test.ast.root))
                    {
                        printf("failed to gen asm for %s\n", test.file_path);
                        success = false;
                        ++test_fail;
                        gen_asm(stdout, test.ast.root);
                        continue;
                    }
                    timer.end();
                    update_perf(&perf->gen_asm, timer.milliseconds());

                    fclose(file);
                }

                sprintf_s(test.clang_buffer, "clang %s -o%s", test.asm_file_path, test.exe_file_path);
                timer.start();
                if (int clang_error = system(test.clang_buffer))
                {
                    printf("Clang Failed with %d\n", clang_error);
                    gen_asm(stdout, test.ast.root);
                    success = false;
                    ++test_fail;
                    debug_break();
                    continue;
                }
                timer.end();
                update_perf(&perf->gen_exe, timer.milliseconds());
            }
            else {
                assert(cfg.ir);
                {
                    FILE* file;
                    err = fopen_s(&file, test.asm_file_path, "wb");
                    if (err) debug_break();

                    timer.start();
                    if (!gen_asm_from_ir(file, test.ir, test.ir_size))
                    {
                        printf("failed to gen asm for %s\n", test.file_path);
                        success = false;
                        ++test_fail;
                        gen_asm(stdout, test.ast.root);
                        continue;
                    }
                    timer.end();
                    update_perf(&perf->gen_asm_from_ir, timer.milliseconds());

                    fclose(file);
                }

                sprintf_s(test.clang_buffer, "clang %s -o%s", test.asm_file_path, test.exe_file_path);
                timer.start();
                if (int clang_error = system(test.clang_buffer))
                {
                    printf("Clang Failed with %d\n", clang_error);
                    gen_asm_from_ir(stdout, test.ir, test.ir_size);
                    success = false;
                    ++test_fail;
                    debug_break();
                    continue;
                }
                timer.end();
                update_perf(&perf->gen_exe, timer.milliseconds());
            }

            timer.start();
            test.our_result = system(test.exe_file_path);
            timer.end();
            if (test.clang_ground_truth != test.our_result)
            {
                test_config temp_cfg = cfg;
                temp_cfg.dump = true;

                dump(temp_cfg, test);
                printf("Ground Truth [%d] does not match our result [%d]\n", test.clang_ground_truth, test.our_result);
                debug_break();
            }
            update_perf(&perf->run_exe, timer.milliseconds());
        }

        // SIMPLIFY
        if (cfg.simplify)
        {
            debug_break(); //TODO?
        }

        // INTERP
        if (cfg.interp)
        {
            test.interp_result;
            timer.start();
            if (!interp_return_value(test.ast.root, &test.interp_result))
            {
                debug_break();
                printf("Interp failed for [%s].\n", test.file_path);
                continue;
            }
            timer.end();
            update_perf(&perf->interp, timer.milliseconds());

            if (test.interp_result != test.clang_ground_truth)
            {
                test_config temp_cfg = cfg;
                temp_cfg.dump = true;

                printf("Interp result of [%s] does not match ground truth!\nReturned: %" PRIi64 " vs Ground Truth: %d\n",
                    test.file_path, test.interp_result, test.clang_ground_truth);
                dump(temp_cfg, test);
                debug_break();
            }
        }

        dump(cfg, test);

    } while (dnext(dir));
    dclose(&dir);

    // print results
    bool prior = false;
    if (cfg.lex) {
        printf("LEX"); 
        prior = true;
    }
    if (cfg.ir) {
        printf("%sIR", prior ? ", " : "");
        prior = true;
    }
    if (cfg.ast) {
        printf("%sAST", prior ? ", " : ""); 
        prior = true;
    }
    if (cfg.gen) {
        printf("%sGEN(ASM)", prior ? ", " : "");
        prior = true;
    }
    if (cfg.interp) {
        printf("%sINTERPRETER", prior ? ", " : "");
        prior = true;
    }
    
    if (!prior) debug_break(); // missing a check for a cfg flag.

    success
        ? printf("[%s]:OK (%d tests)\n", path, test_count)
        : printf("[%s]:FAILED. Tests Passed: %d/%d\n", path, (test_count - test_fail), test_count);

    perf->total_tests += test_count;
}

static void test_simplify(const LexInput& lexin)
{
    LexOutput lexout = {};
    if (!lex(&lexin, &lexout))
        return;

    ASTOut ast_out;
    if (!ast(lexout.tokens, lexout.num_tokens, &ast_out))
        return;

    printf("=== Attempting Simplification of AST: ===\n");
    dump_ast(stdout, ast_out.root, 0);

    ASTNode* simple = ast_out.root;
    int reductions = 0;
    while (true)
    {
        int prev_reductions = reductions;
        simple = simplify(simple, &reductions);
        assert(simple);

        if (prev_reductions == reductions)
            break;

        printf("=== SIMPLIFICATION FOUND! ===\n");
        dump_ast(stdout, simple, 0);
    }

    printf("=== SIMPLIFICATIONS ATTEMPT COMPLETE, TOTAL REDUCTIONS: %d ===\n", reductions);
    printf("BEFORE: "); dump_lex(stdout, &lexout);
    printf("\n AFTER: ");  dump_simplify(stdout, simple);
}

static void test_simplify_double_negative()
{
    const char* prog =
        "int main() {\n"
        "    return -(-1);\n"
        "}\n";

    LexInput lexin = init_lex("ret--1", prog, strlen(prog));

    test_simplify(lexin);
}

static void test_simplify_1_plus_2()
{
    const char* prog =
        "int main() {\n"
        "    return 1+2;\n"
        "}\n";

    LexInput lexin = init_lex("ret1+2", prog, strlen(prog));

    test_simplify(lexin);
}

static void test_simplify_dn_and_1p2()
{
    const char* prog =
        "int main() {\n"
        "    return -(-1+-2);\n"
        "}\n";

    LexInput lexin = init_lex("ret--1+-2", prog, strlen(prog));

    test_simplify(lexin);
}

void interpreter_practice()
{
    printf("enter end string: ");
    char end_string[32];
    fgets(end_string, 32, stdin);
    end_string[strlen(end_string) - 1] = 0; //string newline at end
    printf("waiting for [%s]. type whatever you want and then type [%s] to compile and run your code.\n",
        end_string, end_string);

    char buffer[1024];
    char* buffer_end = buffer + _countof(buffer);
    char* out = buffer;
    while (out < buffer_end)
    {
        fgets(out, (int)(buffer_end - out), stdin);
        char* found = strstr(out, end_string);
        if (found)
        {
            out = found;
            break;
        }
        out += strlen(out);
    }
    printf("thanks! you wrote: ==========[%.*s]==========\n", (int)(out - buffer), buffer);

    LexInput lexin = init_lex("interp", buffer, (out - buffer));
    LexOutput lexout = {};
    if (!lex(&lexin, &lexout))
    {
        printf("LEX FAILED! %s\n", lexout.failure_reason);
        dump_lex(stdout, &lexout);
        printf("\n");
        return;
    }
    else
    {
        printf("LEX OK==========[");
        dump_lex(stdout, &lexout);
        printf("]==========\n");
    }

    ASTOut ast_out;
    if (!ast(lexout.tokens, lexout.num_tokens, &ast_out))
    {
        printf("AST FAILED!\n");
        debug_break();
        return;
    }
    else
    {
        printf("AST OK\n");
        dump_ast(stdout, ast_out.root, 0);
    }

    int64_t result;
    if (!interp_return_value(ast_out.root, &result))
    {
        printf("INTERPRETER FAILED!\n");
        return;
    }

    printf("INTERPRETER SUCCESS! RESULT: %" PRIi64 "\n", result);
}

int run_all_tests()
{
    return run_tests_on_folder(0);
}

int run_tests_on_folder(int folder_index)
{
    perf_numbers perf;

    test_config TEST_LEX = {};
    TEST_LEX.lex = true;

    test_config TEST_IR = {};
    TEST_IR.lex = true;
    TEST_IR.ir = true;

    test_config TEST_INVALID_LEX = {};
    TEST_INVALID_LEX.lex = true;
    TEST_INVALID_LEX.expect_lex_fail = true;

    test_config TEST_GEN = {};
    TEST_GEN.lex = true;
    TEST_GEN.ast = true;
    TEST_GEN.gen = true;

    test_config TEST_IR_GEN = {};
    TEST_IR_GEN.lex = true;
    TEST_IR_GEN.ir = true;
    TEST_IR_GEN.gen = true;

    test_config TEST_INTERP = {};
    TEST_INTERP.lex = true;
    TEST_INTERP.ast = true;
    TEST_INTERP.interp = true;

    Timer timer;
    timer.start();

    // Load cached test results
    {
        Timer t;
        t.start();
        load_test_results();
        t.end();
        perf.test_cache_load = t.milliseconds();
    }

    switch (folder_index)
    {
    case 0: // test all
        printf("=== RUNNING ALL TESTS\n");
    case 1:
        Test(TEST_LEX, &perf, "../stage_1/valid/");
        Test(TEST_LEX, &perf, "../stage_1/invalid/");
        Test(TEST_IR, &perf, "../stage_1/valid/");
        Test(TEST_INTERP, &perf, "../stage_1/valid/");
        Test(TEST_GEN, &perf, "../stage_1/valid/");
        Test(TEST_IR_GEN, &perf, "../stage_1/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_1/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_1/invalid/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 2:
        Test(TEST_LEX, &perf, "../stage_2/valid/");
        Test(TEST_LEX, &perf, "../stage_2/invalid/");
        Test(TEST_IR, &perf, "../stage_2/valid/");
        Test(TEST_INTERP, &perf, "../stage_2/valid/");
        Test(TEST_GEN, &perf, "../stage_2/valid/");
        Test(TEST_IR_GEN, &perf, "../stage_2/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_2/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_2/invalid/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 3:
        Test(TEST_LEX, &perf, "../stage_3/valid/");
        Test(TEST_LEX, &perf, "../stage_3/invalid/");
        Test(TEST_INTERP, &perf, "../stage_3/valid/");
        Test(TEST_GEN, &perf, "../stage_3/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_3/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_3/invalid/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 4:
        Test(TEST_LEX, &perf, "../stage_4/valid/");
        Test(TEST_LEX, &perf, "../stage_4/valid_skip_on_failure/");
        Test(TEST_LEX, &perf, "../stage_4/invalid/");
        Test(TEST_INTERP, &perf, "../stage_4/valid/");
        Test(TEST_INTERP, &perf, "../stage_4/valid_skip_on_failure/");
        Test(TEST_GEN, &perf, "../stage_4/valid/");
        Test(TEST_GEN, &perf, "../stage_4/valid_skip_on_failure/");
        cleanup_artifacts(&perf.cleanup, "../stage_4/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_4/valid_skip_on_failure/");
        cleanup_artifacts(&perf.cleanup, "../stage_4/invalid/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 5:
        Test(TEST_LEX, &perf, "../stage_5/valid/");
        Test(TEST_LEX, &perf, "../stage_5/invalid/");
        Test(TEST_INTERP, &perf, "../stage_5/valid/");
        Test(TEST_GEN, &perf, "../stage_5/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_5/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_5/invalid/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 6:
        Test(TEST_LEX, &perf, "../stage_6/valid/statement/");
        Test(TEST_LEX, &perf, "../stage_6/valid/expression/");
        Test(TEST_LEX, &perf, "../stage_6/invalid/statement/");
        Test(TEST_LEX, &perf, "../stage_6/invalid/expression/");
        Test(TEST_INTERP, &perf, "../stage_6/valid/statement/");
        Test(TEST_INTERP, &perf, "../stage_6/valid/expression/");
        Test(TEST_GEN, &perf, "../stage_6/valid/statement/");
        Test(TEST_GEN, &perf, "../stage_6/valid/expression/");
        cleanup_artifacts(&perf.cleanup, "../stage_6/valid/statement/");
        cleanup_artifacts(&perf.cleanup, "../stage_6/invalid/statement/");
        cleanup_artifacts(&perf.cleanup, "../stage_6/valid/expression/");
        cleanup_artifacts(&perf.cleanup, "../stage_6/invalid/expression/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 7:
        Test(TEST_LEX, &perf, "../stage_7/valid/");
        Test(TEST_LEX, &perf, "../stage_7/invalid/");
        Test(TEST_INTERP, &perf, "../stage_7/valid/");
        Test(TEST_GEN, &perf, "../stage_7/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_7/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_7/invalid/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 8:
        Test(TEST_LEX, &perf, "../stage_8/valid/");
        Test(TEST_LEX, &perf, "../stage_8/invalid/");
        Test(TEST_INTERP, &perf, "../stage_8/valid/");
        Test(TEST_GEN, &perf, "../stage_8/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_8/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_8/invalid/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 9:
        Test(TEST_LEX, &perf, "../stage_9/valid/");
        Test(TEST_LEX, &perf, "../stage_9/invalid/");
        Test(TEST_INTERP, &perf, "../stage_9/valid/");
        Test(TEST_GEN, &perf, "../stage_9/valid/");
        Test(TEST_LEX, &perf, "../stage_9/");
        Test(TEST_INTERP, &perf, "../stage_9/");
        Test(TEST_GEN, &perf, "../stage_9/");
        cleanup_artifacts(&perf.cleanup, "../stage_9/");
        cleanup_artifacts(&perf.cleanup, "../stage_9/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_9/invalid/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 10:
        Test(TEST_LEX, &perf, "../stage_10/valid/");
        Test(TEST_LEX, &perf, "../stage_10/invalid/");
        Test(TEST_INTERP, &perf, "../stage_10/valid/");
        Test(TEST_GEN, &perf, "../stage_10/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_10/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_10/invalid/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 11:
        Test(TEST_INTERP, &perf, "../stage_11_void/");
        Test(TEST_GEN, &perf, "../stage_11_void/");
        cleanup_artifacts(&perf.cleanup, "../stage_11_void/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 12:
        Test(TEST_INVALID_LEX, &perf, "../stage_12_single_quotes/invalid_lex/");
        Test(TEST_LEX, &perf, "../stage_12_single_quotes/");
        Test(TEST_INTERP, &perf, "../stage_12_single_quotes/");
        Test(TEST_GEN, &perf, "../stage_12_single_quotes/");
        cleanup_artifacts(&perf.cleanup, "../stage_12_single_quotes/invalid_lex/");
        cleanup_artifacts(&perf.cleanup, "../stage_12_single_quotes/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 13:
        Test(TEST_INVALID_LEX, &perf, "../stage_13_comments_and_backslash/invalid_lex/");
        Test(TEST_LEX, &perf, "../stage_13_comments_and_backslash/");
        Test(TEST_INTERP, &perf, "../stage_13_comments_and_backslash/");
        Test(TEST_GEN, &perf, "../stage_13_comments_and_backslash/");
        cleanup_artifacts(&perf.cleanup, "../stage_13_comments_and_backslash/invalid_lex/");
        cleanup_artifacts(&perf.cleanup, "../stage_13_comments_and_backslash/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 14:
        //Test(TEST_LEX, &perf, "../stage_14_include/");
        //cleanup_artifacts(&perf.cleanup, "../stage_14_include+/");
        break; // quit, hit our last test.
    default:
        printf("Invalid Test #. Quitting.\n");
        debug_break();
        return 1;
    }

    // Save cached test results
    {
        Timer t;
        t.start();
        save_test_results();
        t.end();
        perf.test_cache_save = t.milliseconds();
    }

    timer.end();
    printf("%" PRIu64 " Tests took %.2fms\n", perf.total_tests, timer.milliseconds());

    float tracked_total = 0.0f;
    tracked_total += perf.test_cache_load + perf.test_cache_save;
    printf(                                             "Perf Results      [samples,      total,        avg,        low,       high]\n");
    tracked_total += print_perf(&perf.read_file,        "  read_file:      ", "\n");
    tracked_total += print_perf(&perf.invalid_lex,      "  invalid_lex:    ", "\n");
    tracked_total += print_perf(&perf.lex,              "  lex:            ", "\n");
    tracked_total += print_perf(&perf.lex_strip,        "  lex_strip:      ", "\n");
    tracked_total += print_perf(&perf.ir,               "  ir:             ", "\n");
    tracked_total += print_perf(&perf.ast,              "  ast:            ", "\n");
    tracked_total += print_perf(&perf.gen_asm,          "  gen_asm:        ", "\n");
    tracked_total += print_perf(&perf.gen_asm_from_ir,  "  gen_asm_from_ir:", "\n");
    tracked_total += print_perf(&perf.gen_exe,          "  gen_exe:        ", "\n");
    tracked_total += print_perf(&perf.run_exe,          "  run_exe:        ", "\n");
    tracked_total += print_perf(&perf.ground_truth,     "  grnd_truth:     ", "\n");
    tracked_total += print_perf(&perf.interp,           "  interp:         ", "\n");
    tracked_total += print_perf(&perf.cleanup,          "  cleanup:        ", "\n");
    printf(                                             " test cache misses: %" PRIu32 ", load: %.2fms, save: %.2fms\n", 
        get_test_cache_misses(), 
        perf.test_cache_load, 
        perf.test_cache_save);
    printf(                                         "Unaccounted for: %.2fms\n", (timer.milliseconds() - tracked_total));
    getchar();
    test_simplify_double_negative();
    test_simplify_1_plus_2();
    test_simplify_dn_and_1p2();

    return 0;
}
