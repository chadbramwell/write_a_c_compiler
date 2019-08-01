// because we all know that c++ should've matched ++c but people wanted to be clever and turn it into something else...
// ++c extends C to a more modern language (no headers) while simplifying silly idioms that would confuse new programmers
// the goal here is to reduce total mental state of a user when reading code which in turn will make the language easier to use.
// example: what does "Foo f;" do? In C: it'll make space on the stack for that struct. In C++: literally anything. And you don't know unless you look at the definition of that struct.
#include "lex.h"
#include "string.h"
#include "stdio.h"

int main(int argc, char** argv)
{
	char buffer[1024];
	LexConfig config;
	config.input =
		"int main() {\n"
		"	return 2;\n"
		"}\n";
	config.end_input = config.input + strlen(config.input);
	config.output = buffer;
	config.end_output = buffer + sizeof(buffer);
	config.failure_reason = 0;

	if (lex(&config))
	{
		printf("success!\n");
		unlex(stdout, config.output, config.end_output);
		return 0;
	}
	printf("failure: %s\n", config.failure_reason);
	unlex(stdout, config.output, config.end_output);
	return 1;
}
