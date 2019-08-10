#include "lex.h"
#include "ast.h"
#include "gen.h"
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
	long file_size = ftell(file);
	rewind(file);

	void* memory = malloc(file_size);
	size_t actually_read = fread_s(memory, file_size, 1, file_size, file);		
	fclose(file);

	if (file_size != actually_read)
	{
		printf("failed to read file %s of size %d\n", lex_in->filename, file_size);
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
};

void path_init(path* p, const char* filename)
{
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
}

int main(int argc, char** argv)
{
	FILE* file;
	bool debug_print = true;
	bool dump_to_disk = false;

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
		debug_print = false;
	}

	LexOutput lex_out;
	if (!lex(lex_in, lex_out))
	{
		printf("lex failure: %s\n", lex_out.failure_reason);
		dump_lex(stdout, lex_out);
		return 1;
	}
	else if (debug_print)
	{
		printf("==lex success!==[\n");
		dump_lex(stdout, lex_out);
		printf("\n]\n");

		if (dump_to_disk && 0 == fopen_s(&file, "ret2.lex.txt", "wb"))
		{
			dump_lex(file, lex_out);
			fclose(file);
		}
	}

	TokenStream ast_in;
	ast_in.next = lex_out.tokens.data();
	ast_in.end = lex_out.tokens.data() + lex_out.tokens.size();

	AST ast_out;
	if (!ast(ast_in, ast_out))
	{
		printf("ast failure\n");
		dump_ast(stdout, ast_out);
		dump_ast_errors(stderr, ast_out.errors, lex_in);
		return 1;
	}
	else if (debug_print)
	{
		printf("==ast success!==[\n");
		dump_ast(stdout, ast_out);
		printf("\n]\n");

		if (dump_to_disk && 0 == fopen_s(&file, "ret2.ast.txt", "wb"))
		{
			dump_ast(file, ast_out);
			fclose(file);
		}
	}

	AsmInput asm_in;
	asm_in.p = &ast_out.root;
	
	AsmOutput asm_out;
	if (!gen_asm(asm_in, asm_out))
	{
		printf("gen_asm failure\n");
		dump_asm(stdout, asm_out);
		return 1;
	}
	else if (debug_print)
	{
		printf("==gen_asm success!==[\n");
		dump_asm(stdout, asm_out);
		printf("\n]\n");
	}

	path p;
	path_init(&p, lex_in.filename);

	// write assembly(.s) file.
	char filename_buffer[1024];
	sprintf_s(filename_buffer, "%.*s.s", (p.name_end - p.original), p.original);
	//printf("\nFILENAME:[%s]\n", filename_buffer);
	if (0 == fopen_s(&file, filename_buffer, "wb"))
	{
		dump_asm(file, asm_out);
		fclose(file);
	}

	// generate exe with clang
	char clang_buffer[1024];
	sprintf_s(clang_buffer, "clang %s -o%.*s", filename_buffer, (p.name_end - p.original), p.original);
	//printf("FILENAME:[%s]\n", clang_buffer);
	int error = system(clang_buffer);
	if(debug_print)
		printf("Clang Result: %d", error);
	return error;
}
