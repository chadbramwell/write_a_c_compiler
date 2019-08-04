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

	ASTInput ast_in;
	ast_in.tokens = lex_out.tokens.data();
	ast_in.num_tokens = lex_out.tokens.size();

	ASTOutput ast_out;
	if (!ast(ast_in, ast_out))
	{
		printf("ast failure: %s\n", ast_out.failure_reason);
		dump_ast(ast_out);
		return 1;
	}
		
	printf("ast success!\n");
	dump_ast(ast_out);

	return 0;
}
