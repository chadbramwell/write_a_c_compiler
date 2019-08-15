#include "lex.h"
#include "ast.h"
#include "gen.h"
#include "timer.h"
#include "debug.h"

#include "string.h"
#include "stdio.h"

bool read_file_into_lex_input(LexInput* lex_in)
{
	FILE* file;
	if (0 != fopen_s(&file, lex_in->filename, "rb"))
	{
		printf("failed to open file %s\n", lex_in->filename);
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
	p->name_start = filename;
	p->name_end = filename;

	// find very end
	const char* end = filename;
	while (*end)
		++end;

	// work backwards to find first extension 
	p->name_end = end; // set now, '.' may not exist in filename
	while (end > filename && *end != '.')
		--end;
	if (end == filename)
		return;
	p->name_end = end;

	// work backwards to find name start
	while (end > filename && *end != '/' && *end != '\\')
		--end;
	if (end == filename)
		return;
	p->name_start = end+1;

	int name_no_path_len = int(p->name_end - p->original);
	sprintf_s(p->lex_path, "%.*s.lex.txt", name_no_path_len, p->original);
	sprintf_s(p->ast_path, "%.*s.ast.txt", name_no_path_len, p->original);
	sprintf_s(p->asm_path, "%.*s.s", name_no_path_len, p->original);
	sprintf_s(p->exe_path, "%.*s.exe", name_no_path_len, p->original);
}

int main(int argc, char** argv)
{
	Timer main_timer;
	Timer clang_timer;
	main_timer.start();

	FILE* timer_log;
	if (0 != fopen_s(&timer_log, "++c.timer.log", "ab"))
		return 2;

	FILE* file;
	bool debug_print = false;
	bool debug_print_to_disk = false;
	bool debug_timers = false;

	LexInput lex_in;
	lex_in.filename = "ret2";
	lex_in.stream =
		"int main() {\n"
		"	return 2;\n"
		"}\n";
	lex_in.length = strlen(lex_in.stream);

	if (argc > 1)
	{
		lex_in.filename = argv[1];
		if (!read_file_into_lex_input(&lex_in))
		{
			return 1;
		}
	}

	path p;
	path_init(&p, lex_in.filename);

	LexOutput lex_out = {};
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
		fprintf(stdout, "\n]\n");

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
	if (0 == fopen_s(&file, p.asm_path, "wb"))
	{
		gen_asm(file, asm_in);
		fclose(file);
	}

	// generate exe with clang
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
		if(debug_timers)
			fprintf(stdout, "Clang Took %.2fms\n", clang_timer.milliseconds());
	}

	main_timer.end();
	if(debug_timers) fprintf(stdout, "Total Time: %.2fms\n", main_timer.milliseconds());

	fprintf(timer_log, "[%s] total time: %.2fms of which a system call to clang took %.2fms\n",
		p.original,
		main_timer.milliseconds(),
		clang_timer.milliseconds());
	fclose(timer_log);

	if(clang_error)
		debug_break();
	return clang_error;
}
