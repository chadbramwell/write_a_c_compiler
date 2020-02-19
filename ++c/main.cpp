#include "test.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int compile_file(const char* path, bool verbose);

int main(int argc, char** argv)
{
    // handle global-opt-out flags
    for (int i = 1; i < argc; ++i)
    {
        // if -irtest, run ir tests
        if (0 == strcmp(argv[i], "-irtest")
            || 0 == strcmp(argv[i], "-testir"))
        {
            return run_ir_tests();
        }

        // if -test is specified, ignore rest of command-line
        if (0 == strcmp(argv[i], "-test"))
        {
            if (i + 1 < argc)
            {
                int folder_index = atoi(argv[i + 1]);
                return run_tests_on_folder(folder_index, true);
            }
            return run_all_tests();
        }
        
        // if -interp is specified, ignore rest of command-line
        if (0 == strcmp(argv[i], "-interp"))
        {
            while(true)
            {
                interpreter_practice();

                printf("again?[y]:");
                char temp[3];
                fgets(temp, sizeof(temp), stdin);
                if (temp[0] != 'y' || temp[1] != '\n')
                    break;
            }
            return 0;
        }
    }

    // check for specific file to test
    if (argc >= 2)
    {
        const char* test_file = argv[1];
        bool verbose = false;
        if (argc == 3)
        {
            if (argv[2][0] == '-' &&
                argv[2][1] == 'v' &&
                argv[2][2] == 0)
            {
                verbose = true;
            }
        }
        return compile_file(test_file, verbose);
    }
    
    printf("expected either '-interp' to run interpreter, '<file path to compile>', '-test' to run all tests, or '-test <number>' to run tests on a specific stage number\n");
    return compile_file(NULL, true);
}

#include "timer.h"
#include "file.h"
#include "dir.h"
#include "debug.h"
#include "lex.h"
#include "ir.h"
#include "gen.h"
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
static void path_init(path* p, const char* filename)
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
        p->name_start = end + 1;

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
static int compile_file(const char* path, bool verbose)
{
    bool verbose_print = false;
    bool verbose_print_to_disk = false;
    bool verbose_print_timers = false;

    Timer main_timer;
    main_timer.start();

    LexInput lexin;
    if (path)
    {
        size_t file_length;
        const char* file_data = file_read_into_memory(path, &file_length);
        if (!file_data)
        {
            // error reasons printed by function.
            return 2;
        }
        lexin = init_lex(path, file_data, file_length);

        if (verbose)
        {
            verbose_print = true;
            verbose_print_timers = true;
        }

        if (verbose_print)
        {
            fprintf(stdout, "===RAW FILE [%s]===\n", lexin.filename);
            fwrite(lexin.stream, 1, (size_t)lexin.length, stdout);
            fprintf(stdout, "\n===END RAW FILE===\n");
        }
    }
    else
    {
        const char* prog =
            "int main() {\n"
            "    return 2;\n"
            "}\n";
        printf("no path given so defaulting to simple program:\n%s\n", prog);
        lexin = init_lex("ret2", prog, strlen(prog));
    }

    FILE* timer_log;
    if (0 != fopen_s(&timer_log, "++c.timer.log", "ab"))
    {
        printf("failed to open timer log file to append data\n");
        return 3;
    }

    struct path p;
    path_init(&p, lexin.filename);

    LexOutput lexout = {};
    if (!lex(&lexin, &lexout))
    {
        fprintf(stdout, "lex failure: %s\n", lexout.failure_reason);
        dump_lex(stdout, &lexout);
        main_timer.end();
        fprintf(timer_log, "\n[%s] lex fail, took %.2fms\n", p.original, main_timer.milliseconds());
        debug_break();
        return 1;
    }
    else if (verbose_print)
    {
        fprintf(stdout, "==lex success!==[");
        dump_lex(stdout, &lexout);
        fprintf(stdout, "]\n");

        FILE* file;
        if (verbose_print_to_disk && 0 == fopen_s(&file, p.lex_path, "wb"))
        {
            dump_lex(file, &lexout);
            fclose(file);
        }
    }

    IR* ir_out = nullptr;
    size_t ir_out_size = 0;
    if (!ir(lexout.tokens, lexout.num_tokens, &ir_out, &ir_out_size))
    {
        main_timer.end();
        fprintf(timer_log, "[%s] IR fail, took %.2fms\n", p.original, main_timer.milliseconds());
        debug_break();
        return 1;
    }
    else if (verbose_print)
    {
        fprintf(stdout, "==ir success!==[\n");
        dump_ir(stdout, ir_out, ir_out_size);
        fprintf(stdout, "\n]\n");

        FILE* file;
        if (verbose_print_to_disk && 0 == fopen_s(&file, p.ast_path, "wb"))
        {
            dump_ir(stdout, ir_out, ir_out_size);
            fclose(file);
        }
    }

    const int ground_truth = path == NULL ? 2 : get_clang_ground_truth(p.src_path);

    //int64_t interp_result;
    //if (!interp_return_value(ast_out.root, &interp_result))
    //{
    //    fprintf(stdout, "Interpreter failed.\n");
    //}
    //if (interp_result != ground_truth)
    //{
    //    fprintf(stdout, "Interpreter result %" PRIi64 " does not match ground truth result %d\n", interp_result, ground_truth);
    //    debug_break();
    //}
    
    FILE* asm_test_file;
    if (0 != tmpfile_s(&asm_test_file))
        return 4;
    if (!gen_asm_from_ir(asm_test_file, ir_out, ir_out_size))
    {
        fprintf(stdout, "gen_asm failure\n");
        gen_asm_from_ir(stdout, ir_out, ir_out_size);
        main_timer.end();
        fprintf(timer_log, "[%s] gen_asm failed, took %.2fms\n", p.original, main_timer.milliseconds());
        debug_break();
        return 1;
    }
    else if (verbose_print)
    {
        fprintf(stdout, "==gen_asm success!==[\n");
        gen_asm_from_ir(stdout, ir_out, ir_out_size);
        fprintf(stdout, "\n]\n");

        fprintf(stdout, "Clang's ASM==[\n");

        char temp[260];
        sprintf_s(temp, "clang -S %s -o%s", p.original, p.asm_path);
        assert(0 == system(temp));
        file_dump_to_stdout(p.asm_path);
    }
    fclose(asm_test_file);



    // write assembly(.s) file.
    FILE* file;
    if (0 == fopen_s(&file, p.asm_path, "wb"))
    {
        gen_asm_from_ir(file, ir_out, ir_out_size);
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
        if (verbose_print_timers)
            fprintf(stdout, "Clang Took %.2fms\n", clang_timer.milliseconds());
    }

    main_timer.end();
    if (verbose_print_timers) fprintf(stdout, "Total Time: %.2fms\n", main_timer.milliseconds());

    fprintf(timer_log, "[%s] total time: %.2fms of which a system call to clang took %.2fms\n",
        p.original,
        main_timer.milliseconds(),
        clang_timer.milliseconds());
    fclose(timer_log);

    if (clang_error)
        debug_break();
    else
    {
        int our_result = system(p.exe_path);
        if (our_result != ground_truth)
        {
            printf("Ground Truth [%d] does not match our result [%d]\n", ground_truth, our_result);
            debug_break();
        }
        else if (verbose_print)
            printf("Return value of program: [%d]\n", our_result);
    }
    return clang_error;
}
