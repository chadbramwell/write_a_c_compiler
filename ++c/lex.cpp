#include "lex.h"
#include "stdio.h"
#include "memory.h"

bool isletter(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool isnumber(char c)
{
	return c >= '0' && c <= '9';
}
bool iswhitespace(char c)
{
	return (c >= '\t' && c <= '\r') || c == ' ';
}

bool lex(LexConfig* io_config)
{
	// store length of output buffer and wipe it's length for return value
	// code below will write to and increment end_output
	const char* max_output = io_config->end_output;
	io_config->end_output = io_config->output;

	while (io_config->input < io_config->end_input)
	{
		// skip whitespace
		if(iswhitespace(*io_config->input))
		{
			++io_config->input;
			continue;
		}

		// if no room token stream for a single-byte, we are done.
		// code below assumes there's room for at least one byte.
		if (io_config->end_output + 1 >= max_output)
		{
			io_config->failure_reason = "[lex] no room left in output for tokens";
			return false;
		}

		// handle one-char syntax: (){};
		switch (*io_config->input)
		{
		case '(':
			io_config->input += 1;
			*io_config->end_output = eToken::open_parens;
			io_config->end_output += 1;
			continue;
		case ')':
			io_config->input += 1;
			*io_config->end_output = eToken::closed_parens;
			io_config->end_output += 1;
			continue;
		case '{':
			io_config->input += 1;
			*io_config->end_output = eToken::open_brace;
			io_config->end_output += 1;
			continue;
		case '}':
			io_config->input += 1;
			*io_config->end_output = eToken::closed_brace;
			io_config->end_output += 1;
			continue;
		case ';':
			io_config->input += 1;
			*io_config->end_output = eToken::semicolon;
			io_config->end_output += 1;
			continue;
		}

		// handle 3 letter keywords 
		if (io_config->input + 4 < io_config->end_input) // HACK: +1 because we are assuming all keywords require space after.
		{
			//int
			if (io_config->input[0] == 'i' &&
				io_config->input[1] == 'n' &&
				io_config->input[2] == 't' &&
				(io_config->input[3] == ' ')) // HACK/TODO: handle otherwhitespace and handle stuff like commas: "void func(int,char**)"
			{
				io_config->input += 3;
				*io_config->end_output = eToken::k_int;
				io_config->end_output += 1;
				continue;
			}
		}

		// handle 6 letter keywords
		if (io_config->input + 6 < io_config->end_input)
		{
			//return
			if (io_config->input[0] == 'r' &&
				io_config->input[1] == 'e' &&
				io_config->input[2] == 't' &&
				io_config->input[3] == 'u' &&
				io_config->input[4] == 'r' &&
				io_config->input[5] == 'n') // todo: handle "returnval" or other things
			{
				io_config->input += 6;
				*io_config->end_output = eToken::k_return;
				io_config->end_output += 1;
				continue;
			}
		}

		// handle identifiers & literals ("constants") at the same time
		if (isletter(io_config->input[0]) || isnumber(io_config->input[0]))
		{
			const char* input = io_config->input + 1;

			// read until whitespace or end
			while (input < io_config->end_input && 
				(isletter(*input) || isnumber(*input)))
			{
				input += 1;
			}

			int identifier_len = input - io_config->input;
			if (identifier_len > 255)
			{
				io_config->failure_reason = "[lex] identifiers, literals, etc.. greater than 255 bytes in length are not supported";
				return false;
			}
			if (identifier_len + 2 > max_output - io_config->end_output) // +1 for eToken id, +1 for length
			{
				io_config->failure_reason = "[lex] not enough room for output of identifier";
				return false;
			}

			io_config->end_output[0] = eToken::something;
			io_config->end_output[1] = (char)identifier_len;
			memcpy(io_config->end_output + 2, io_config->input, identifier_len);
			io_config->input += identifier_len;
			io_config->end_output += identifier_len + 2;
			continue;
		}

		io_config->failure_reason = "[lex] unsupported data in input";
		return false;
	}

	return true;
}

void unlex(FILE* out, const char* tokens, const char* tokens_end)
{
	while (tokens < tokens_end)
	{
		switch (*tokens)
		{
		case eToken::k_int:
			fwrite("int ", 1, 4, out);
			tokens += 1;
			continue;
		case eToken::k_return:
			fwrite("return", 1, 6, out);
			tokens += 1;
			continue;
		case eToken::open_parens:
			fwrite("(", 1, 1, out);
			tokens += 1;
			continue;
		case eToken::closed_parens:
			fwrite(")", 1, 1, out);
			tokens += 1;
			continue;
		case eToken::open_brace:
			fwrite("{", 1, 1, out);
			tokens += 1;
			continue;
		case eToken::closed_brace:
			fwrite("}", 1, 1, out);
			tokens += 1;
			continue;
		case eToken::semicolon:
			fwrite(";", 1, 1, out);
			tokens += 1;
			continue;
		case eToken::something:
			fwrite(tokens + 2, 1, tokens[1], out);
			tokens += 2 + tokens[1];
			continue;
		}
		
		break;// detected token we don't know how to handle
	}
}