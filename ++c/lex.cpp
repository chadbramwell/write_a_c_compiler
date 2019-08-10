#include "lex.h"

bool is_letter_or_underscore(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool isnumber(char c)
{
	return c >= '0' && c <= '9';
}
bool iswhitespace(char c)
{
	return (c >= '\t' && c <= '\r') || c == ' ';
}

bool lex(const LexInput& input, LexOutput& output)
{
	// store length of output buffer and wipe it's length for return value
	// code below will write to and increment end_output
	const char* stream = input.stream;
	const char* end_stream = input.stream + input.length;

	while (stream < end_stream)
	{
		// skip whitespace
		if(iswhitespace(*stream))
		{
			++stream;
			continue;
		}

		// if no room token stream for a single-byte, we are done.
		// code below assumes there's room for at least one byte.
		if (stream >= end_stream)
		{
			output.failure_location = stream;
			output.failure_reason = "[lex] no room left in output for tokens";
			return false;
		}

		// handle one-char syntax: (){};
		switch (*stream)
		{
		case '!':
			output.tokens.push_back(Token(eToken::bang, stream, stream+1));
			++stream;
			continue;
		case '(':
			output.tokens.push_back(Token(eToken::open_parens, stream, stream + 1));
			++stream;
			continue;
		case ')':
			output.tokens.push_back(Token(eToken::closed_parens, stream, stream + 1));
			++stream;
			continue;
		case '*':
			output.tokens.push_back(Token(eToken::star, stream, stream + 1));
			++stream;
			continue;
		case '+':
			output.tokens.push_back(Token(eToken::plus, stream, stream + 1));
			++stream;
			continue;
		case '-':
			output.tokens.push_back(Token(eToken::dash, stream, stream + 1));
			++stream;
			continue;
		case '/':
			output.tokens.push_back(Token(eToken::forward_slash, stream, stream + 1));
			++stream;
			continue;
		case ';':
			output.tokens.push_back(Token(eToken::semicolon, stream, stream + 1));
			++stream;
			continue;
		case '{':
			output.tokens.push_back(Token(eToken::open_curly, stream, stream + 1));
			++stream;
			continue;
		case '}':
			output.tokens.push_back(Token(eToken::closed_curly, stream, stream + 1));
			++stream;
			continue;
		case '~':
			output.tokens.push_back(Token(eToken::tilde, stream, stream + 1));
			++stream;
			continue;
		}

		if (isnumber(*stream))
		{
			const char* num_start = stream;
			uint64_t number = *stream - '0';
			++stream;

			while (stream < end_stream && isnumber(*stream))
			{
				number *= 10;
				number += *stream - '0';
				++stream;
			}

			Token token(eToken::constant_number, num_start, stream);
			token.number = number;
			output.tokens.push_back(token);
			continue;
		}
		
		if(is_letter_or_underscore(*stream))
		{
			const char* token_end = stream + 1;
			while (token_end < end_stream && (is_letter_or_underscore(*token_end) || isnumber(*token_end)))
			{
				++token_end;
			}

			Token token(eToken::identifier, stream, token_end);
			token.identifier = std::string(stream, token_end);
			stream = token_end;

			if (token.identifier == "int")
				token.type = eToken::keyword_int;
			else if (token.identifier == "return")
				token.type = eToken::keyword_return;

			output.tokens.push_back(token);
			continue;
		}

		output.failure_location = stream;
		output.failure_reason = "[lex] unsupported data in input";
		return false;
	}

	return true;
}

void dump_lex(FILE* file, const LexOutput& lex)
{
	for(size_t i = 0; i < lex.tokens.size(); ++i)
	{
		const Token& token = lex.tokens[i];
		switch (token.type)
		{
		case eToken::identifier:
			fwrite(token.identifier.c_str(), 1, token.identifier.length(), file);
			continue;
		case eToken::constant_number:
			fprintf(file, "%" PRIu64, token.number);
			continue;
		case '!': fputc(token.type, file); continue;
		case '(': fputc(token.type, file); continue;
		case ')': fputc(token.type, file); continue;
		case '*': fputc(token.type, file); continue;
		case '+': fputc(token.type, file); continue;
		case '-': fputc(token.type, file); continue;
		case '/': fputc(token.type, file); continue;
		case ';': fputc(token.type, file); continue;
		case '{': fputc(token.type, file); continue;
		case '}': fputc(token.type, file); continue;
		case '~': fputc(token.type, file); continue;
		case eToken::keyword_int: fprintf(file, "int "); continue;
		case eToken::keyword_return: fprintf(file, "return "); continue;
		}
		
		break;// detected token we don't know how to handle
	}
}