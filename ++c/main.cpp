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

enum TestType : uint8_t
{
	TEST_LEX				= 0b00000001,
	TEST_AST				= 0b00000011,
	TEST_GEN				= 0b00000111,
	TEST_SIMPLIFY			= 0b00001011, //no GEN
	TEST_INTERP				= 0b00010011, //no GEN

	TEST_DUMP_ON_SUCCESS	= 0b10000000,
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

void compare_to_clang(const char* source_path, const char* exe_path)
{
	char buff[256];
	sprintf_s(buff, "clang %s", source_path);
	int compilation_result = system(buff);
	assert(compilation_result == 0);

	int clang_result = system("a.exe");
	int our_result = system(exe_path);

	assert(clang_result == our_result);
}

struct path
{
	const char* original;
	const char* name_start;
	const char* name_end;

	char lex_path[256];
	char ast_path[256];
	char asm_path[256];
	char exe_path[256];
};

void path_init(path* p, const char* filename)
{
	*p = {};
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
	sprintf_s(p->lex_path, "%.*s.lex.txt", name_no_path_len, p->original);
	sprintf_s(p->ast_path, "%.*s.ast.txt", name_no_path_len, p->original);
	sprintf_s(p->asm_path, "%.*s.s", name_no_path_len, p->original);
	sprintf_s(p->exe_path, "%.*s.exe", name_no_path_len, p->original);
}

void dump(TestType tt, const char* file_path, const LexOutput& lexout, const ASTNode* root, const AsmInput& asm_in)
{
	if (tt & TEST_DUMP_ON_SUCCESS)
	{
		printf("===%s===\n", file_path);
		if(tt & TEST_LEX) dump_lex(stdout, lexout);
		if(tt & TEST_AST) dump_ast(stdout, *root, 0);
		if(tt & TEST_GEN) gen_asm(stdout, asm_in);
	}
}

struct perf_numbers
{
	float read_file;
	float lex;
	float ast;
	float gen;
	float clang;
	float compare;
	float interp;
};
void clear_perf(perf_numbers* n)
{
	memset(n, 0, sizeof(n[0]));
}
void update_perf(float* n, float ms)
{
	if (*n == 0.0f)
		*n = ms;
	else
		*n = (*n + ms) / 2.0f;
}

void Test(TestType tt, perf_numbers* perf, const char* directory)
{
	DirectoryIter* dir = dopen(directory, "*.c");
	if (!dir) return;

	int test_count = 0;
	int test_fail = 0;
	bool success = true;

	do
	{
		if (disdir(dir))
			continue;

		++test_count;

		const char* file_path = dfpath(dir);
		ASTNode* root = NULL;
		AsmInput asm_in;

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
			printf("failed to lex file %s\n", file_path);
			continue;
		}
		timer.end();
		update_perf(&perf->lex, timer.milliseconds());

		////// AST
		if ((tt & TEST_AST) != TEST_AST)
		{
			dump(tt, file_path, lexout, root, asm_in);
			continue;
		}
		TokenStream ast_in;
		ast_in.next = lexout.tokens.data();
		ast_in.end = lexout.tokens.data() + lexout.tokens.size();

		std::vector<ASTError> errors;
		timer.start();
		root = ast(ast_in, errors);
		if (!root)
		{
			printf("failed to ast file %s\n", file_path);
			success = false;
			++test_fail;
			dump_ast_errors(stdout, errors, lexin);
			continue;
		}
		timer.end();
		update_perf(&perf->ast, timer.milliseconds());

		// From here on, our tests diverge. If we only wanted to TEST_AST, exit now.
		if ((tt ^ TEST_AST) == 0)
		{
			dump(tt, file_path, lexout, root, asm_in);
			continue;
		}

		///// ASM
		if((tt & TEST_GEN) == TEST_GEN)
		{
			asm_in.root = root;

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
				update_perf(&perf->gen, timer.milliseconds());

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
			update_perf(&perf->clang, timer.milliseconds());

			timer.start();
			compare_to_clang(file_path, exe_file_path);
			timer.end();
			update_perf(&perf->compare, timer.milliseconds());

			dump(tt, file_path, lexout, root, asm_in);
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
			int interp_result;
			timer.start();
			if (interp_return_value(root, &interp_result))
			{
				printf("Interp [%s] Success! Returned: %d\n", file_path, interp_result);
				timer.end();
				update_perf(&perf->interp, timer.milliseconds());
			}
			else
				printf("Interp failed for [%s].\n", file_path);
			continue;
		}

	} while (dnext(dir));

	// print results
	switch (tt)
	{
	case TEST_GEN: printf("LEX, AST, and GEN/CLANG"); break;
	case TEST_AST: printf("LEX, AST"); break;
	case TEST_LEX: printf("LEX"); break;
	case TEST_INTERP: printf("INTERPRETOR"); break;
	default:
		debug_break();
		printf("???[%s]:", directory);
	}
	success 
		? printf("[%s]:OK (%d tests)\n", directory, test_count) 
		: printf("[%s]:FAILED. Tests Passed: %d/%d\n", directory, (test_count - test_fail), test_count);
}

void init_lex_to_ret2(LexInput& lex_in)
{
	lex_in.filename = "ret2";
	lex_in.stream =
		"int main() {\n"
		"	return 2;\n"
		"}\n";
	lex_in.length = strlen(lex_in.stream);
}

void test_simplify(const LexInput& lexin)
{
	LexOutput lexout;
	if (!lex(lexin, lexout))
		return;

	TokenStream tokens;
	tokens.next = lexout.tokens.data();
	tokens.end = tokens.next + lexout.tokens.size();

	std::vector<ASTError> errors;
	ASTNode* original = ast(tokens, errors);
	if (!original)
		return;

	printf("=== Attempting Simplification of AST: ===\n");
	dump_ast(stdout, *original, 0);

	ASTNode* simple = original;
	int reductions = 0;
	while (true)
	{
		int prev_reductions = reductions;
		simple = simplify(simple, &reductions);
		assert(simple);

		if (prev_reductions == reductions)
			break;

		printf("=== SIMPLIFICATION FOUND! ===\n");
		dump_ast(stdout, *simple, 0);
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
		"	return -(-1);\n"
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
		"	return 1+2;\n"
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
		"	return -(-1+-2);\n"
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
	printf("waiting for [%s]. type whatever you want and then type [%s] to compile and run your code.\n", end_string, end_string);

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

	TokenStream tokens;
	tokens.next = lexout.tokens.data();
	tokens.end = lexout.tokens.data() + lexout.tokens.size();
	std::vector<ASTError> errors;
	ASTNode* root = ast(tokens, errors);
	if(!root)
	{
		printf("AST FAILED!\n");
		dump_ast_errors(stdout, errors, lexin);
		return;
	}
	else
	{
		printf("AST OK\n");
		dump_ast(stdout, *root, 0);
	}

	int result;
	if (!interp_return_value(root, &result))
	{
		printf("INTERPRETER FAILED!\n");
		return;
	}

	printf("INTERPRETER SUCCESS! RESULT: %d\n", result);
}

int main(int argc, char** argv)
{
	//interpreter_practice();

	// check for -test flag
	for (int i = 1; i < argc; ++i)
	{
		if (0 != strcmp(argv[i], "-test"))
			continue;

		perf_numbers perf;
		clear_perf(&perf);
		
		Timer timer;
		timer.start();

		Test(TEST_LEX, &perf, "../stage_1/invalid/");
		Test(TEST_LEX, &perf, "../stage_2/invalid/");
		Test(TEST_LEX, &perf, "../stage_3/invalid/");
		Test(TEST_LEX, &perf, "../stage_4/invalid/");
		Test(TEST_LEX, &perf, "../stage_5/invalid/");
		Test(TEST_LEX, &perf, "../stage_6/invalid/statement/");
		Test(TEST_LEX, &perf, "../stage_6/invalid/expression/");
		Test(TEST_LEX, &perf, "../stage_7/invalid/");

		clear_perf(&perf); // above includes invalid files. I don't want those to lower the avg perf of lexing

		Test(TEST_INTERP, &perf, "../stage_1/valid/");
		Test(TEST_INTERP, &perf, "../stage_2/valid/");
		Test(TEST_INTERP, &perf, "../stage_3/valid/");
		Test(TEST_INTERP, &perf, "../stage_4/valid/");
		Test(TEST_INTERP, &perf, "../stage_4/valid_skip_on_failure/");
		Test(TEST_INTERP, &perf, "../stage_5/valid/");
		Test(TEST_INTERP, &perf, "../stage_6/valid/statement/");
		Test(TEST_INTERP, &perf, "../stage_6/valid/expression/");
		Test(TEST_INTERP, &perf, "../stage_7/valid/");

		Test(TEST_GEN, &perf, "../stage_1/valid/");
		Test(TEST_GEN, &perf, "../stage_2/valid/");
		Test(TEST_GEN, &perf, "../stage_3/valid/");
		Test(TEST_GEN, &perf, "../stage_4/valid/");
		Test(TEST_GEN, &perf, "../stage_4/valid_skip_on_failure/");
		Test(TEST_GEN, &perf, "../stage_5/valid/");
		Test(TEST_GEN, &perf, "../stage_6/valid/statement/");
		Test(TEST_GEN, &perf, "../stage_6/valid/expression/");
		Test(TEST_GEN, &perf, "../stage_7/valid/");

		timer.end();
		printf("Tests took %.2fms\n", timer.milliseconds());

		printf("Perf Results (average milliseconds)\n"
			"\tread file: %.2fms\n"
			"\tlex: %.2fms\n"
			"\tast: %.2fms\n"
			"\tgen: %.2fms\n"
			"\tclang: %.2fms\n"
			"\tcompare: %.2fms\n"
			"\tinterp: %.2fms\n",
			perf.read_file,
			perf.lex,
			perf.ast,
			perf.gen,
			perf.clang,
			perf.compare,
			perf.interp);

		//test_simplify_double_negative();
		//test_simplify_1_plus_2();
		//test_simplify_dn_and_1p2();

		return 0;
		
	}

	bool debug_print = false;
	bool debug_print_to_disk = false;
	bool debug_print_timers = false;

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
				debug_print = true;
				debug_print_timers = true;
			}
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
	else if (debug_print)
	{
		fprintf(stdout, "==lex success!==[");
		dump_lex(stdout, lex_out);
		fprintf(stdout, "]\n");

		FILE* file;
		if (debug_print_to_disk && 0 == fopen_s(&file, p.lex_path, "wb"))
		{
			dump_lex(file, lex_out);
			fclose(file);
		}
	}

	TokenStream ast_in;
	ast_in.next = lex_out.tokens.data();
	ast_in.end = lex_out.tokens.data() + lex_out.tokens.size();

	std::vector<ASTError> errors;
	ASTNode* root = ast(ast_in, errors);
	if (!root)
	{
		dump_ast_errors(stdout, errors, lex_in);
		main_timer.end();
		fprintf(timer_log, "[%s] AST fail, took %.2fms\n", p.original, main_timer.milliseconds());
		debug_break();
		return 1;
	}
	else if (debug_print)
	{
		fprintf(stdout, "==ast success!==[\n");
		dump_ast(stdout, *root, 0);
		fprintf(stdout, "\n]\n");

		FILE* file;
		if (debug_print_to_disk && 0 == fopen_s(&file, p.ast_path, "wb"))
		{
			dump_ast(file, *root, 0);
			fclose(file);
		}
	}

	AsmInput asm_in;
	asm_in.root = root;

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
	else if (debug_print)
	{
		fprintf(stdout, "==gen_asm success!==[\n");
		gen_asm(stdout, asm_in);
		fprintf(stdout, "\n]\n");
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
		sprintf_s(clang_buffer, "clang %s -o%s", p.asm_path, p.exe_path);
		//printf("FILENAME:[%s]\n", clang_buffer);
		clang_timer.start();
		clang_error = system(clang_buffer);
		clang_timer.end();
		if (debug_print)
			fprintf(stdout, "Clang Compliation Result: %d\n", clang_error);
		if(debug_print_timers)
			fprintf(stdout, "Clang Took %.2fms\n", clang_timer.milliseconds());
	}

	main_timer.end();
	if(debug_print_timers) fprintf(stdout, "Total Time: %.2fms\n", main_timer.milliseconds());

	fprintf(timer_log, "[%s] total time: %.2fms of which a system call to clang took %.2fms\n",
		p.original,
		main_timer.milliseconds(),
		clang_timer.milliseconds());
	fclose(timer_log);
	
	if(clang_error)
		debug_break();
	else
	{
		char source_path[260];
		char exe_path[260];
		bool ok = get_absolute_path(p.original, &source_path);
		ok &= get_absolute_path(p.exe_path, &exe_path);
		assert(ok);
		compare_to_clang(source_path, exe_path);
	}
	return clang_error;
}
