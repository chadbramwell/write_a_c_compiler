#include "lex.h"
#include "ast.h"
#include "gen.h"
#include "simplify.h"
#include "dir.h"
#include "timer.h"
#include "debug.h"
#include "interp.h"

#include "string.h"
#include "stdio.h"
#include "stdlib.h"

enum TestType : uint8_t
{
    TEST_LEX                = 0b00000001,
    TEST_AST                = 0b00000011,
    TEST_GEN                = 0b00000111,
    TEST_SIMPLIFY           = 0b00001011, //no GEN
    TEST_INTERP             = 0b00010011, //no GEN

    TEST_DUMP_ON            = 0b10000000,
};

bool read_file_into_lex_input(const char* filename, LexInput* lex_in)
{
    lex_in->filename = filename;

    FILE* file;
    if (0 != fopen_s(&file, lex_in->filename, "rb"))
    {
        printf("failed to open file %s\n", lex_in->filename);
        debug_break();
        return false;
    }

    fseek(file, 0, SEEK_END);
    uint32_t file_size = ftell(file);
    rewind(file);

    void* memory = malloc(file_size);
    size_t actually_read = fread_s(memory, file_size, 1, file_size, file);        
    fclose(file);

    if (file_size != actually_read)
    {
        printf("failed to read file %s of size %" PRIu32 "\n", lex_in->filename, file_size);
        debug_break();
        free(memory);
        return false;
    }

    lex_in->stream = (const char*)memory;
    lex_in->length = file_size;
    return true;
}

int get_clang_ground_truth(const char* source_path)
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

struct path
{
    const char* original;
    const char* name_start;
    const char* name_end;

    char src_path[260];
    char lex_path[260];
    char ast_path[260];
    char asm_path[260];
    char exe_path[260];
};

void path_init(path* p, const char* filename)
{
    memset(p, 0, sizeof(*p));
    p->original = filename;

    // find very end
    const char* end = filename;
    while (*end)
        ++end;

    // work backwards to find first extension 
    p->name_end = end; // set now, '.' may not exist in filename
    while (end > filename && *end != '.')
        --end;
    if (end != filename)
        p->name_end = end;

    // work backwards to find name start
    p->name_start = filename;
    while (end > filename && *end != '/' && *end != '\\')
        --end;
    if (end != filename)
        p->name_start = end+1;

    int name_no_path_len = int(p->name_end - p->original);
    if (!get_absolute_path(p->original, &p->src_path))
        debug_break();
    sprintf_s(p->lex_path, "%.*s.lex.txt", name_no_path_len, p->original);
    sprintf_s(p->ast_path, "%.*s.ast.txt", name_no_path_len, p->original);
    sprintf_s(p->asm_path, "%.*s.s", name_no_path_len, p->original);

    char tmp[260];
    sprintf_s(tmp, "%.*s.exe", name_no_path_len, p->original);    
    if (!get_absolute_path(tmp, &p->exe_path))
        debug_break();
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
void update_perf(std::vector<float>* p, float ms)
{
    p->push_back(ms);
}
bool get_perf(std::vector<float>* p, float* o_min, float* o_max, float* o_avg, float* o_total)
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
float print_perf(std::vector<float>* p, const char* preamble, const char* postamble)
{
    float min, max, avg, total;
    if (!get_perf(p, &min, &max, &avg, &total))
        return 0.0f;

    printf("%s[%7zu, %8.2fms, %8.2fms, %8.2fms, %8.2fms]%s", preamble, p->size(), total, avg, min, max, postamble);
    return total;
}

void cleanup_artifacts(std::vector<float>* p, const char* path)
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

void dump(uint8_t tt, const LexInput& lexin, const LexOutput& lexout, const ASTNode* root, const AsmInput* asm_in)
{
    if (tt & TEST_DUMP_ON)
    {
        printf("===DEBUG INFO FOR [%s]===\n", lexin.filename);
        printf("=== RAW FILE ===\n");
        fwrite(lexin.stream, 1, (size_t)lexin.length, stdout);
        printf("\n");
        if ((tt & TEST_LEX) == TEST_LEX) {
            printf("=== LEX ===\n");
            dump_lex(stdout, lexout);
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
            LexInput temp2;
            assert(read_file_into_lex_input(temp_name, &temp2));
            fprintf(stdout, "%*s\n", int(temp2.length), temp2.stream);
        }
        printf("=== END DEBUG INFO ===\n");
    }
}

void Test(TestType tt, perf_numbers* perf, const char* path)
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

        ///// LEX
        LexInput lexin;
        Timer timer; 
        timer.start();
        if (!read_file_into_lex_input(file_path, &lexin))
        {
            printf("failed to read file %s\n", file_path);
            success = false;
            ++test_fail;
            continue;
        }
        timer.end();
        update_perf(&perf->read_file, timer.milliseconds());

        LexOutput lexout;
        timer.start();
        if (!lex(lexin, lexout))
        {
            draw_error_caret_at(stdout, lexin, lexout.failure_location, lexout.failure_reason);
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
            dump_ast_errors(stdout, &ast_out, lexin);
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
        if((tt & TEST_GEN) == TEST_GEN)
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

void init_lex_to_ret2(LexInput& lex_in)
{
    lex_in.filename = "ret2";
    lex_in.stream =
        "int main() {\n"
        "    return 2;\n"
        "}\n";
    lex_in.length = strlen(lex_in.stream);
}

void test_simplify(const LexInput& lexin)
{
    LexOutput lexout;
    if (!lex(lexin, lexout))
        return;

    ASTOut ast_out;
    if (!ast(lexout.tokens, lexout.tokens_size, &ast_out))
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
    printf("BEFORE: "); dump_lex(stdout, lexout);
    printf("\n AFTER: ");  dump_simplify(stdout, simple);
}

void test_simplify_double_negative()
{
    LexInput lexin;
    lexin.filename = "ret--1";
    lexin.stream =
        "int main() {\n"
        "    return -(-1);\n"
        "}\n";
    lexin.length = strlen(lexin.stream);

    test_simplify(lexin);
}

void test_simplify_1_plus_2()
{
    LexInput lexin;
    lexin.filename = "ret1+2";
    lexin.stream =
        "int main() {\n"
        "    return 1+2;\n"
        "}\n";
    lexin.length = strlen(lexin.stream);

    test_simplify(lexin);
}

void test_simplify_dn_and_1p2()
{
    LexInput lexin;
    lexin.filename = "ret--1+-2";
    lexin.stream =
        "int main() {\n"
        "    return -(-1+-2);\n"
        "}\n";
    lexin.length = strlen(lexin.stream);

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
        fgets(out, buffer_end - out, stdin);
        char* found = strstr(out, end_string);
        if (found)
        {
            out = found;
            break;
        }
        out += strlen(out);
    }
    printf("thanks! you wrote: ==========[%.*s]==========\n", (out - buffer), buffer);

    LexInput lexin;
    lexin.filename = "interp";
    lexin.stream = buffer;
    lexin.length = (out - buffer);

    LexOutput lexout;
    if (!lex(lexin, lexout))
    {
        printf("LEX FAILED! %s\n", lexout.failure_reason);
        dump_lex(stdout, lexout);
        printf("\n");
        return;
    }
    else
    {
        printf("LEX OK==========[");
        dump_lex(stdout, lexout);
        printf("]==========\n");
    }

    ASTOut ast_out;
    if(!ast(lexout.tokens, lexout.tokens_size, &ast_out))
    {
        printf("AST FAILED!\n");
        dump_ast_errors(stdout, &ast_out, lexin);
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

int main(int argc, char** argv)
{
    //interpreter_practice();

    // check for -test flag
    for (int i = 1; i < argc; ++i)
    {
        if (0 != strcmp(argv[i], "-test"))
            continue;

        int test_single = 0;        
        if (i + 1 < argc)
        {
            test_single = atoi(argv[i + 1]);
        }


        perf_numbers perf;
        
        Timer timer;
        timer.start();

        switch (test_single)
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
            if (test_single != 0) break; // quit if 0 or fall-through if not
        case 2:
            Test(TEST_LEX, &perf, "../stage_2/valid/");
            Test(TEST_LEX, &perf, "../stage_2/invalid/");
            Test(TEST_INTERP, &perf, "../stage_2/valid/");
            Test(TEST_GEN, &perf, "../stage_2/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_2/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_2/invalid/");
            if (test_single != 0) break; // quit if 0 or fall-through if not
        case 3:
            Test(TEST_LEX, &perf, "../stage_3/valid/");
            Test(TEST_LEX, &perf, "../stage_3/invalid/");
            Test(TEST_INTERP, &perf, "../stage_3/valid/");
            Test(TEST_GEN, &perf, "../stage_3/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_3/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_3/invalid/");
            if (test_single != 0) break; // quit if 0 or fall-through if not
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
            if (test_single != 0) break; // quit if 0 or fall-through if not
        case 5:
            Test(TEST_LEX, &perf, "../stage_5/valid/");
            Test(TEST_LEX, &perf, "../stage_5/invalid/");
            Test(TEST_INTERP, &perf, "../stage_5/valid/");
            Test(TEST_GEN, &perf, "../stage_5/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_5/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_5/invalid/");
            if (test_single != 0) break; // quit if 0 or fall-through if not
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
            if (test_single != 0) break; // quit if 0 or fall-through if not
        case 7:
            Test(TEST_LEX, &perf, "../stage_7/valid/");
            Test(TEST_LEX, &perf, "../stage_7/invalid/");
            Test(TEST_INTERP, &perf, "../stage_7/valid/");
            Test(TEST_GEN, &perf, "../stage_7/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_7/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_7/invalid/");
            if (test_single != 0) break; // quit if 0 or fall-through if not
        case 8:
            Test(TEST_LEX, &perf, "../stage_8/valid/");
            Test(TEST_LEX, &perf, "../stage_8/invalid/");
            Test(TEST_INTERP, &perf, "../stage_8/valid/");
            Test(TEST_GEN, &perf, "../stage_8/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_8/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_8/invalid/");
            if (test_single != 0) break; // quit if 0 or fall-through if not
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
            if (test_single != 0) break; // quit if 0 or fall-through if not
        case 10:
            Test(TEST_LEX, &perf, "../stage_10/valid/");
            Test(TEST_LEX, &perf, "../stage_10/invalid/");
            Test(TEST_INTERP, &perf, "../stage_10/valid/");
            Test(TEST_GEN, &perf, "../stage_10/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_10/valid/");
            cleanup_artifacts(&perf.cleanup, "../stage_10/invalid/");
            if (test_single != 0) break; // quit if 0 or fall-through if not
        case 11:
            Test(TEST_LEX, &perf, "../stage_10+/");
            Test(TEST_LEX, &perf, "../stage_10+/invalid_lex/");
            Test(TEST_INTERP, &perf, "../stage_10+/");
            Test(TEST_GEN, &perf, "../stage_10+/");
            cleanup_artifacts(&perf.cleanup, "../stage_10+/");
            cleanup_artifacts(&perf.cleanup, "../stage_10+/invalid_lex/");
            break; // quit, hit our last test.
        default:
            printf("Invalid Test #. Quitting.\n");
            debug_break();
            return 1;
        }

        timer.end();
        printf("Tests took %.2fms\n", timer.milliseconds());

        float tracked_total = 0.0f;

                         printf(                         "Perf Results  [samples,      total,        avg,        low,       high]\n");
        tracked_total += print_perf(&perf.read_file,     "  read_file:  ", "\n");
        tracked_total += print_perf(&perf.lex,           "  lex:        ", "\n");
        tracked_total += print_perf(&perf.ast,           "  ast:        ", "\n");
        tracked_total += print_perf(&perf.gen_asm,       "  gen_asm:    ", "\n");
        tracked_total += print_perf(&perf.gen_exe,       "  gen_exe:    ", "\n");
        tracked_total += print_perf(&perf.run_exe,       "  run_exe:    ", "\n");
        tracked_total += print_perf(&perf.ground_truth,  "  grnd_truth: ", "\n");
        tracked_total += print_perf(&perf.interp,        "  interp:     ", "\n");
        tracked_total += print_perf(&perf.cleanup,       "  cleanup:    ", "\n");
                         printf(                         "Unaccounted for: %.2fms\n", (timer.milliseconds() - tracked_total));
        
        test_simplify_double_negative();
        test_simplify_1_plus_2();
        test_simplify_dn_and_1p2();

        return 0;
        
    }

    bool verbose_print = false;
    bool verbose_print_to_disk = false;
    bool verbose_print_timers = false;

    Timer main_timer;
    main_timer.start();

    LexInput lex_in;
    if (argc >= 2)
    {
        if (!read_file_into_lex_input(argv[1], &lex_in))
        {
            // error reasons printed by function.
            return 2;
        }

        if (argc == 3)
        {
            if (argv[2][0] == '-' &&
                argv[2][1] == 'v' &&
                argv[2][2] == 0)
            {
                verbose_print = true;
                verbose_print_timers = true;
            }
        }

        if (verbose_print)
        {
            fprintf(stdout, "===RAW FILE [%s]===\n", lex_in.filename);
            fwrite(lex_in.stream, 1, (size_t)lex_in.length, stdout);
            fprintf(stdout, "\n===END RAW FILE===\n");
        }
    }
    else
    {
        printf("no path given so defaulting to simple return 2 program\n");
        init_lex_to_ret2(lex_in);
    }

    FILE* timer_log;
    if (0 != fopen_s(&timer_log, "++c.timer.log", "ab"))
    {
        printf("failed to open timer log file to append data\n");
        return 3;
    }

    path p;
    path_init(&p, lex_in.filename);
    
    LexOutput lex_out;
    if (!lex(lex_in, lex_out))
    {
        fprintf(stdout, "lex failure: %s\n", lex_out.failure_reason);
        dump_lex(stdout, lex_out);
        main_timer.end();
        fprintf(timer_log, "\n[%s] lex fail, took %.2fms\n", p.original, main_timer.milliseconds());
        debug_break();
        return 1;
    }
    else if (verbose_print)
    {
        fprintf(stdout, "==lex success!==[");
        dump_lex(stdout, lex_out);
        fprintf(stdout, "]\n");

        FILE* file;
        if (verbose_print_to_disk && 0 == fopen_s(&file, p.lex_path, "wb"))
        {
            dump_lex(file, lex_out);
            fclose(file);
        }
    }

    ASTOut ast_out;
    if (!ast(lex_out.tokens, lex_out.tokens_size, &ast_out))
    {
        dump_ast_errors(stdout, &ast_out, lex_in);
        main_timer.end();
        fprintf(timer_log, "[%s] AST fail, took %.2fms\n", p.original, main_timer.milliseconds());
        debug_break();
        return 1;
    }
    else if (verbose_print)
    {
        fprintf(stdout, "==ast success!==[\n");
        dump_ast(stdout, ast_out.root, 0);
        fprintf(stdout, "\n]\n");

        FILE* file;
        if (verbose_print_to_disk && 0 == fopen_s(&file, p.ast_path, "wb"))
        {
            dump_ast(file, ast_out.root, 0);
            fclose(file);
        }
    }

    const int ground_truth = get_clang_ground_truth(p.src_path);

    int64_t interp_result;
    if (!interp_return_value(ast_out.root, &interp_result))
    {
        fprintf(stdout, "Interpreter failed.\n");
    }
    if (interp_result != ground_truth)
    {
        fprintf(stdout, "Interpreter result %" PRIi64 " does not match ground truth result %d\n", interp_result, ground_truth);
        debug_break();
    }

    AsmInput asm_in;
    asm_in.root = ast_out.root;

    FILE* asm_test_file;
    if (0 != tmpfile_s(&asm_test_file))
        return 4;
    if (!gen_asm(asm_test_file, asm_in))
    {
        fprintf(stdout, "gen_asm failure\n");
        gen_asm(stdout, asm_in);
        main_timer.end();
        fprintf(timer_log, "[%s] gen_asm failed, took %.2fms\n", p.original, main_timer.milliseconds());
        debug_break();
        return 1;
    }
    else if (verbose_print)
    {
        fprintf(stdout, "==gen_asm success!==[\n");
        gen_asm(stdout, asm_in);
        fprintf(stdout, "\n]\n");

        fprintf(stdout, "Clang's ASM==[\n");

        char temp[260];
        sprintf_s(temp, "clang -S %s -o%s", p.original, p.asm_path);
        assert(0 == system(temp));
        LexInput temp2;
        assert(read_file_into_lex_input(p.asm_path, &temp2));
        fprintf(stdout, "%*s\n", int(temp2.length), temp2.stream);
    }
    fclose(asm_test_file);

    

    // write assembly(.s) file.
    FILE* file;
    if (0 == fopen_s(&file, p.asm_path, "wb"))
    {
        gen_asm(file, asm_in);
        fclose(file);
    }

    // generate exe with clang
    Timer clang_timer;
    int clang_error = 0;
    {
        char clang_buffer[1024];
        sprintf_s(clang_buffer, "clang -g %s -o%s", p.asm_path, p.exe_path);
        //printf("FILENAME:[%s]\n", clang_buffer);
        clang_timer.start();
        clang_error = system(clang_buffer);
        clang_timer.end();
        if (verbose_print)
            fprintf(stdout, "Clang Compliation Result: %d\n", clang_error);
        if(verbose_print_timers)
            fprintf(stdout, "Clang Took %.2fms\n", clang_timer.milliseconds());
    }

    main_timer.end();
    if(verbose_print_timers) fprintf(stdout, "Total Time: %.2fms\n", main_timer.milliseconds());

    fprintf(timer_log, "[%s] total time: %.2fms of which a system call to clang took %.2fms\n",
        p.original,
        main_timer.milliseconds(),
        clang_timer.milliseconds());
    fclose(timer_log);
    
    if(clang_error)
        debug_break();
    else
    {
        int our_result = system(p.exe_path);
        if (our_result != ground_truth)
        {
            if(!verbose_print) dump(TEST_GEN | TEST_DUMP_ON, lex_in, lex_out, ast_out.root, &asm_in);
            printf("Ground Truth [%d] does not match our result [%d]\n", ground_truth, our_result);
            debug_break();
        }
        else if(verbose_print)
            printf("Return value of program: [%d]\n", our_result);
    }
    return clang_error;
}
