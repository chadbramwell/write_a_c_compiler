#include "lex.h"
#include "ast.h"
#include "gen.h"
#include "simplify.h"
#include "dir.h"
#include "timer.h"
#include "debug.h"

#include "string.h"
#include "stdio.h"

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

void lex_directory(const char* directory)
{
	DirectoryIter* dir = dopen(directory);
	if (!dir) return;

	bool success = true;

	do
	{
		if (disdir(dir))
			continue;

		const char* file_path = dfpath(dir);

		LexInput lexin;
		if (!read_file_into_lex_input(file_path, &lexin))
		{
			printf("failed to read file %s\n", file_path);
			success = false;
			continue;
		}

		LexOutput lexout;
		if (!lex(lexin, lexout))
		{
			draw_error_caret_at(stdout, lexin, lexout.failure_location, lexout.failure_reason);
			success = false;
		}

	} while (dnext(dir));

	if(success)
		printf("LEX[%s]: OK\n", directory);
}

void test_lexing()
{
	Timer timer;
	timer.start();

	lex_directory("./stage_1/invalid/"); // note that most of the 'invalid' .c files are invalid due to AST issues, not lexing issues.
	lex_directory("./stage_1/valid/");
	lex_directory("./stage_2/invalid/");
	lex_directory("./stage_2/valid/");
	lex_directory("./stage_3/invalid/");
	lex_directory("./stage_3/valid/");
	lex_directory("./stage_4/invalid/");
	lex_directory("./stage_4/valid/");
	lex_directory("./stage_4/valid_skip_on_failure/");
	lex_directory("./stage_5/invalid/");
	lex_directory("./stage_5/valid/");
	lex_directory("./stage_6/invalid/statement/");
	lex_directory("./stage_6/invalid/expression/");
	lex_directory("./stage_6/valid/statement/");
	lex_directory("./stage_6/valid/expression/");

	timer.end();
	printf("Lex Tests took %.2fms\n", timer.milliseconds());
}

void ast_directory(const char* directory, bool dump_on_success = false)
{
	DirectoryIter* dir = dopen(directory);
	if (!dir) return;

	bool success = true;

	do
	{
		if (disdir(dir))
			continue;

		const char* file_path = dfpath(dir);

		LexInput lexin;
		if (!read_file_into_lex_input(file_path, &lexin))
		{
			printf("failed to read file %s\n", file_path);
			success = false;
			continue;
		}

		LexOutput lexout;
		if (!lex(lexin, lexout))
		{
			draw_error_caret_at(stdout, lexin, lexout.failure_location, lexout.failure_reason);
			success = false;
			printf("failed to lex file %s\n", file_path);
			continue;
		}

		TokenStream ast_in;
		ast_in.next = lexout.tokens.data();
		ast_in.end = lexout.tokens.data() + lexout.tokens.size();

		std::vector<ASTError> errors;
		ASTNode* root = ast(ast_in, errors);
		if (!root)
		{
			printf("failed to ast file %s\n", file_path);
			success = false;
			dump_ast_errors(stdout, errors, lexin);
			continue;
		}
		
		if (dump_on_success)
		{
			printf("===%s===\n", file_path);
			dump_lex(stdout, lexout);
			dump_ast(stdout, *root, 0);
		}

	} while (dnext(dir));

	if (success)
		printf("AST[%s]: OK\n", directory);
}

void test_ast()
{
	Timer timer;
	timer.start();

	ast_directory("./stage_1/valid/");
	ast_directory("./stage_2/valid/");
	ast_directory("./stage_3/valid/");
	ast_directory("./stage_4/valid/");
	ast_directory("./stage_4/valid_skip_on_failure/");
	ast_directory("./stage_5/valid/");
	ast_directory("./stage_6/valid/statement/");
	ast_directory("./stage_6/valid/expression/");

	timer.end();
	printf("AST Tests took %.2fms\n", timer.milliseconds());
}

void gen_directory(const char* directory, bool dump_on_success = false)
{
	DirectoryIter* dir = dopen(directory);
	if (!dir) return;

	bool success = true;

	do
	{
		if (disdir(dir))
			continue;

		const char* file_path = dfpath(dir);

		///// LEX
		LexInput lexin;
		if (!read_file_into_lex_input(file_path, &lexin))
		{
			printf("failed to read file %s\n", file_path);
			success = false;
			continue;
		}

		LexOutput lexout;
		if (!lex(lexin, lexout))
		{
			draw_error_caret_at(stdout, lexin, lexout.failure_location, lexout.failure_reason);
			success = false;
			printf("failed to lex file %s\n", file_path);
			continue;
		}

		////// AST
		TokenStream ast_in;
		ast_in.next = lexout.tokens.data();
		ast_in.end = lexout.tokens.data() + lexout.tokens.size();

		std::vector<ASTError> errors;
		ASTNode* root = ast(ast_in, errors);
		if (!root)
		{
			printf("failed to ast file %s\n", file_path);
			success = false;
			dump_ast_errors(stdout, errors, lexin);
			continue;
		}

		///// ASM
		AsmInput asm_in;
		asm_in.root = root;

		char asm_file_path[L_tmpnam_s+2];
		char exe_file_path[L_tmpnam_s+2];
		errno_t err = tmpnam_s(asm_file_path);
		if (err) debug_break();
		err = tmpnam_s(exe_file_path);
		if (err) debug_break();

		// append .s to each temp file name
		{
			char* set_s = asm_file_path;
			while (*set_s) ++set_s;
			*set_s++ = '.';
			*set_s++ = 's';
			*set_s = 0;

			set_s = exe_file_path;
			while (*set_s) ++set_s;
			*set_s++ = '.';
			*set_s++ = 's';
			*set_s = 0;
		}

		{
			FILE* file;
			err = fopen_s(&file, asm_file_path, "wb");
			if (err) debug_break();

			if (!gen_asm(file, asm_in))
			{
				printf("failed to gen asm for %s\n", file_path);
				success = false;
				gen_asm(stdout, asm_in);
				continue;
			}

			fclose(file);
		}

		char clang_buffer[1024];
		sprintf_s(clang_buffer, "clang %s -o%s", asm_file_path, exe_file_path);
		if (int clang_error = system(clang_buffer))
		{
			printf("Clang Failed with %d\n", clang_error);
			gen_asm(stdout, asm_in);
			success = false;
			debug_break();
			continue;
		}

		if (dump_on_success)
		{
			printf("===%s===\n", file_path);
			dump_lex(stdout, lexout);
			dump_ast(stdout, *root, 0);
			gen_asm(stdout, asm_in);
			fprintf(stdout, "\n]\n");
		}

	} while (dnext(dir));

	if (success)
		printf("GEN/CLANG[%s]: OK\n", directory);
}

void test_gen()
{
	Timer timer;
	timer.start();

	gen_directory("./stage_1/valid/");
	gen_directory("./stage_2/valid/");
	gen_directory("./stage_3/valid/");
	gen_directory("./stage_4/valid/");
	gen_directory("./stage_4/valid_skip_on_failure/");
	gen_directory("./stage_5/valid/");
	gen_directory("./stage_6/valid/statement/");
	gen_directory("./stage_6/valid/expression/");

	timer.end();
	printf("GEN/CLANG Tests took %.2fms\n", timer.milliseconds());
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
	printf(" AFTER: ");  dump_simplify(stdout, simple);
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

int main(int argc, char** argv)
{
	test_lexing();
	test_ast();
	test_gen();
	//test_simplify_double_negative();
	//test_simplify_1_plus_2();
	//test_simplify_dn_and_1p2();

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
		fprintf(timer_log, "[%s] lex fail, took %.2fms\n", p.original, main_timer.milliseconds());
		debug_break();
		return 1;
	}
	else if (debug_print)
	{
		fprintf(stdout, "==lex success!==[\n");
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
		fprintf(stdout, "ast failure\n");
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
		return 3;
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
			fprintf(stdout, "Clang Result: %d\n", clang_error);
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
	return clang_error;
}
