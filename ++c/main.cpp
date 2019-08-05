#include "lex.h"
#include "ast.h"
#include "string.h"
#include "stdio.h"

int main(int argc, char** argv)
{
	LexInput input;
	input.stream =
		"int main() {\n"
		"	return 2;\n"
		"}\n";
	input.length = strlen(input.stream);

	LexOutput lex_out;
	if (!lex(input, lex_out))
	{
		printf("lex failure: %s\n", lex_out.failure_reason);
		dump_lex(stdout, lex_out);
		return 1;
	}

	printf("lex success!\n");
	dump_lex(stdout, lex_out);
	printf("\n");

	TokenStream ast_in;
	ast_in.next = lex_out.tokens.data();
	ast_in.end = lex_out.tokens.data() + lex_out.tokens.size();

	AST ast_out;
	if (!ast(ast_in, ast_out))
	{
		printf("ast failure\n");
		dump_ast(stdout, ast_out);
		return 1;
	}
		
	printf("ast success!\n");
	dump_ast(stdout, ast_out);

	return 0;
}
