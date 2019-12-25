#include "test.h"
#include "lex.h"
#include "ast.h"
#include "gen.h"
#include "simplify.h"
#include "dir.h"
#include "timer.h"
#include "debug.h"
#include "interp.h"


#include <string>
#include <vector>

enum TestType2 : uint8_t
{
    TEST_LEX = 0b00000001,
    TEST_AST = 0b00000011,
    TEST_GEN = 0b00000111,
    TEST_SIMPLIFY = 0b00001011, //no GEN
    TEST_INTERP = 0b00010011, //no GEN

    TEST_DUMP_ON = 0b10000000,
};

static const char* read_file_into_memory(const char* filename, size_t* o_size)
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
    return (const char*)memory;
}
static void dump_file_to_stdout(const char* filename)
{
    size_t size;
    const char* data = read_file_into_memory(filename, &size);
    assert(data);
    fwrite(data, 1, (size_t)size, stdout);
}

static int get_clang_ground_truth(const char* source_path)
{
    char buff[256];
    sprintf_s(buff, "clang %s", source_path);
    int compilation_result = system(buff);
    assert(compilation_result == 0);

    int ground_truth = system("a.exe");

    int del_ok = system("del a.exe");
    assert(del_ok == 0);

    return ground_truth;
}

struct perf_numbers
{
    std::vector<float> read_file;
    std::vector<float> lex;
    std::vector<float> ast;
    std::vector<float> gen_asm;
    std::vector<float> gen_exe;
    std::vector<float> run_exe;
    std::vector<float> ground_truth;
    std::vector<float> interp;
    std::vector<float> cleanup;
};
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

static void dump(uint8_t tt, const LexInput& lexin, const LexOutput& lexout, const ASTNode* root, const AsmInput* asm_in)
{
    if (tt & TEST_DUMP_ON)
    {
        printf("===DEBUG INFO FOR [%s]===\n", lexin.filename);
        printf("=== RAW FILE ===\n");
        fwrite(lexin.stream, 1, (size_t)lexin.length, stdout);
        printf("\n");
        if ((tt & TEST_LEX) == TEST_LEX) {
            printf("=== LEX ===\n");
            dump_lex(stdout, &lexout);
            printf("\n");
        }
        if ((tt & TEST_AST) == TEST_AST)
        {
            printf("=== AST ===\n");
            dump_ast(stdout, root, 0);
            //dump_ast_errors(stdout, )
            printf("\n");
        }
        if ((tt & TEST_GEN) == TEST_GEN)
        {
            printf("=== GEN ASSEMBLY ===\n");
            gen_asm(stdout, *asm_in);
            printf("\n");

            fprintf(stdout, "Clang's ASM==[\n");

            char temp_name[L_tmpnam];
            tmpnam_s(temp_name);

            char temp_cmd[256];
            sprintf_s(temp_cmd, "clang -S %s -o%s", lexin.filename, temp_name);
            assert(0 == system(temp_cmd));
            dump_file_to_stdout(temp_name);
        }
        printf("=== END DEBUG INFO ===\n");
    }
}

void Test(TestType2 tt, perf_numbers* perf, const char* path)
{
    DirectoryIter* dir = NULL;
    if (!dopen(&dir, path, "*.c")) return;

    int test_count = 0;
    int test_fail = 0;
    bool success = true;

    do
    {
        if (disdir(dir))
            continue;

        ++test_count;

        const char* file_path = dfpath(dir);
        printf("> %s\n", file_path);

        ///// READ FILE
        Timer timer;
        timer.start();
        size_t file_length;
        const char* file_data = read_file_into_memory(file_path, &file_length);
        if (!file_data)
        {
            printf("failed to read file %s\n", file_path);
            success = false;
            ++test_fail;
            continue;
        }
        timer.end();
        update_perf(&perf->read_file, timer.milliseconds());

        ///// LEX
        LexOutput lexout = {};
        timer.start();
        LexInput lexin = init_lex(file_path, file_data, file_length);
        if (!lex(&lexin, &lexout))
        {
            debug_break();
            success = false;
            ++test_fail;
            printf("failed to lex file %s\nComparing to Clang error:\n", file_path);
            char buff[256];
            sprintf_s(buff, "clang %s", file_path);
            system(buff);
            continue;
        }
        timer.end();
        update_perf(&perf->lex, timer.milliseconds());

        ////// AST
        if ((tt & TEST_AST) != TEST_AST)
        {
            dump(tt, lexin, lexout, NULL, NULL);
            continue;
        }

        timer.start();
        ASTOut ast_out;
        if (!ast(lexout.tokens, lexout.tokens_size, &ast_out))
        {
            printf("failed to ast file %s\n", file_path);
            success = false;
            ++test_fail;
            debug_break();
            continue;
        }
        timer.end();
        update_perf(&perf->ast, timer.milliseconds());

        // From here on, our tests diverge. If we only wanted to TEST_AST, exit now.
        if ((tt ^ TEST_AST) == 0)
        {
            dump(tt, lexin, lexout, ast_out.root, NULL);
            continue;
        }

        // Calc Ground Truth
        // - clang *.c
        // - run clangs' output: a.exe, and return result
        timer.start();
        int clang_ground_truth = get_clang_ground_truth(file_path);
        timer.end();
        update_perf(&perf->ground_truth, timer.milliseconds());
        //printf("GROUND TRUTH [%s] = %d\n", file_path, clang_ground_truth);

        ///// ASM
        if ((tt & TEST_GEN) == TEST_GEN)
        {
            AsmInput asm_in;
            asm_in.root = ast_out.root;

            char asm_file_path[L_tmpnam_s + 2]; // NOTE: +2 for .s
            char exe_file_path[L_tmpnam_s + 4]; // NOTE: +4 for .exe
            errno_t err = tmpnam_s(asm_file_path);
            if (err) debug_break();
            err = tmpnam_s(exe_file_path);
            if (err) debug_break();

            // append .s to each temp file name
            {
                char* set_s = asm_file_path;
                while (*set_s) ++set_s;
                memcpy(set_s, ".s", 3); // 3 to include null-terminator

                set_s = exe_file_path;
                while (*set_s) ++set_s;
                memcpy(set_s, ".exe", 5); // 5 to include null-terminator
            }

            {
                FILE* file;
                err = fopen_s(&file, asm_file_path, "wb");
                if (err) debug_break();

                timer.start();
                if (!gen_asm(file, asm_in))
                {
                    printf("failed to gen asm for %s\n", file_path);
                    success = false;
                    ++test_fail;
                    gen_asm(stdout, asm_in);
                    continue;
                }
                timer.end();
                update_perf(&perf->gen_asm, timer.milliseconds());

                fclose(file);
            }

            char clang_buffer[1024];
            sprintf_s(clang_buffer, "clang %s -o%s", asm_file_path, exe_file_path);
            timer.start();
            if (int clang_error = system(clang_buffer))
            {
                printf("Clang Failed with %d\n", clang_error);
                gen_asm(stdout, asm_in);
                success = false;
                ++test_fail;
                debug_break();
                continue;
            }
            timer.end();
            update_perf(&perf->gen_exe, timer.milliseconds());

            timer.start();
            int our_result = system(exe_file_path);
            timer.end();
            if (clang_ground_truth != our_result)
            {
                dump(tt | TEST_DUMP_ON, lexin, lexout, ast_out.root, &asm_in);
                printf("Ground Truth [%d] does not match our result [%d]\n", clang_ground_truth, our_result);
                debug_break();
            }
            update_perf(&perf->run_exe, timer.milliseconds());

            dump(tt, lexin, lexout, ast_out.root, &asm_in);
            continue;
        }

        // SIMPLIFY
        if ((tt & TEST_SIMPLIFY) == TEST_SIMPLIFY)
        {
            debug_break();
            continue;
        }

        // INTERP
        if ((tt & TEST_INTERP) == TEST_INTERP)
        {
            int64_t interp_result;
            timer.start();
            if (!interp_return_value(ast_out.root, &interp_result))
            {
                debug_break();
                printf("Interp failed for [%s].\n", file_path);
                continue;
            }
            timer.end();
            update_perf(&perf->interp, timer.milliseconds());

            if (interp_result != clang_ground_truth)
            {
                printf("Interp result of [%s] does not match ground truth!\nReturned: %" PRIi64 " vs Ground Truth: %d\n",
                    file_path, interp_result, clang_ground_truth);
                dump(tt | TEST_DUMP_ON, lexin, lexout, ast_out.root, NULL);
                debug_break();
            }
            continue;
        }

    } while (dnext(dir));
    dclose(&dir);

    // print results
    switch (tt)
    {
    case TEST_GEN: printf("LEX, AST, and GEN/CLANG"); break;
    case TEST_AST: printf("LEX, AST"); break;
    case TEST_LEX: printf("LEX"); break;
    case TEST_INTERP: printf("INTERPRETER"); break;
    default:
        debug_break();
        printf("???[%s]:", path);
    }
    success
        ? printf("[%s]:OK (%d tests)\n", path, test_count)
        : printf("[%s]:FAILED. Tests Passed: %d/%d\n", path, (test_count - test_fail), test_count);
}

int run_ir_tests()
{
    int folder_index = 1;

    perf_numbers perf;

    Timer timer;
    timer.start();

    switch (folder_index)
    {
    case 0: // test all
        printf("=== RUNNING ALL TESTS\n");
    case 1:
        Test(TEST_LEX, &perf, "../stage_1/valid/");
        Test(TEST_LEX, &perf, "../stage_1/invalid/");
        Test(TEST_INTERP, &perf, "../stage_1/valid/");
        Test(TEST_GEN, &perf, "../stage_1/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_1/valid/");
        cleanup_artifacts(&perf.cleanup, "../stage_1/invalid/");
        if (folder_index != 0) break; // quit if 0 or fall-through if not
    case 2:
        Test(TEST_LEX, &perf, "../stage_2/valid/");
        Test(TEST_LEX, &perf, "../stage_2/invalid/");
        Test(TEST_INTERP, &perf, "../stage_2/valid/");
        Test(TEST_GEN, &perf, "../stage_2/valid/");
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
        //Test(TEST_LEX, &perf, "../stage_10+/");
        //Test(TEST_LEX, &perf, "../stage_10+/invalid_lex/");
        //Test(TEST_INTERP, &perf, "../stage_10+/");
        //Test(TEST_GEN, &perf, "../stage_10+/");
        //cleanup_artifacts(&perf.cleanup, "../stage_10+/");
        //cleanup_artifacts(&perf.cleanup, "../stage_10+/invalid_lex/");
        break; // quit, hit our last test.
    default:
        printf("Invalid Test #. Quitting.\n");
        debug_break();
        return 1;
    }

    timer.end();
    printf("Tests took %.2fms\n", timer.milliseconds());

    float tracked_total = 0.0f;

    printf("Perf Results  [samples,      total,        avg,        low,       high]\n");
    tracked_total += print_perf(&perf.read_file, "  read_file:  ", "\n");
    tracked_total += print_perf(&perf.lex, "  lex:        ", "\n");
    tracked_total += print_perf(&perf.ast, "  ast:        ", "\n");
    tracked_total += print_perf(&perf.gen_asm, "  gen_asm:    ", "\n");
    tracked_total += print_perf(&perf.gen_exe, "  gen_exe:    ", "\n");
    tracked_total += print_perf(&perf.run_exe, "  run_exe:    ", "\n");
    tracked_total += print_perf(&perf.ground_truth, "  grnd_truth: ", "\n");
    tracked_total += print_perf(&perf.interp, "  interp:     ", "\n");
    tracked_total += print_perf(&perf.cleanup, "  cleanup:    ", "\n");
    printf("Unaccounted for: %.2fms\n", (timer.milliseconds() - tracked_total));

    return 0;
}
