#include "lex.h"
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

	LexOutput output;

	if (lex(input, output))
	{
		printf("success!\n");
		unlex(stdout, output);
		return 0;
	}
	printf("failure: %s\n", output.failure_reason);
	unlex(stdout, output);
	return 1;
}
