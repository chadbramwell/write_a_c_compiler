#include "lex.h"
#include "debug.h"
#include "strings.h"

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
	const str strInt = strings_insert_nts("int");
	const str strReturn = strings_insert_nts("return");
	const str strIf = strings_insert_nts("if");
	const str strElse = strings_insert_nts("else");

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

		// handle two-char syntax: &&, ||, ==, !=, <=, >=
		if (stream + 1 < end_stream)
		{
			if (stream[0] == '&' && stream[1] == '&')
			{
				output.tokens.push_back(Token(eToken::logical_and, stream, stream + 2));
				stream += 2;
				continue;
			}
			if (stream[0] == '|' && stream[1] == '|')
			{
				output.tokens.push_back(Token(eToken::logical_or, stream, stream + 2));
				stream += 2;
				continue;
			}
			if (stream[0] == '=' && stream[1] == '=')
			{
				output.tokens.push_back(Token(eToken::logical_equal, stream, stream + 2));
				stream += 2;
				continue;
			}
			if (stream[0] == '!' && stream[1] == '=')
			{
				output.tokens.push_back(Token(eToken::logical_not_equal, stream, stream + 2));
				stream += 2;
				continue;
			}
			if (stream[0] == '<' && stream[1] == '=')
			{
				output.tokens.push_back(Token(eToken::less_than_or_equal, stream, stream + 2));
				stream += 2;
				continue;
			}
			if (stream[0] == '>' && stream[1] == '=')
			{
				output.tokens.push_back(Token(eToken::greater_than_or_equal, stream, stream + 2));
				stream += 2;
				continue;
			}
		}

		// handle one-char syntax: (){};
		switch (*stream)
		{
		case '!':
			output.tokens.push_back(Token(eToken::logical_not, stream, stream+1));
			++stream;
			continue;
		case '&':
			output.tokens.push_back(Token(eToken::bitwise_and, stream, stream + 1));
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
		case ':':
			output.tokens.push_back(Token(eToken::colon, stream, stream + 1));
			++stream;
			continue;
		case ';':
			output.tokens.push_back(Token(eToken::semicolon, stream, stream + 1));
			++stream;
			continue;
		case '<':
			output.tokens.push_back(Token(eToken::less_than, stream, stream + 1));
			++stream;
			continue;
		case '=':
			output.tokens.push_back(Token(eToken::assignment, stream, stream + 1));
			++stream;
			continue;
		case '>':
			output.tokens.push_back(Token(eToken::greater_than, stream, stream + 1));
			++stream;
			continue;
		case '?':
			output.tokens.push_back(Token(eToken::question_mark, stream, stream + 1));
			++stream;
			continue;
		case '{':
			output.tokens.push_back(Token(eToken::open_curly, stream, stream + 1));
			++stream;
			continue;
		case '|':
			output.tokens.push_back(Token(eToken::logical_or, stream, stream + 1));
			++stream;
			continue;
		case '}':
			output.tokens.push_back(Token(eToken::closed_curly, stream, stream + 1));
			++stream;
			continue;
		case '~':
			output.tokens.push_back(Token(eToken::bitwise_not, stream, stream + 1));
			++stream;
			continue;
		}

		if (isnumber(*stream))
		{
			const char* num_start = stream;
			uint64_t number = uint64_t(*stream - '0');
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
			token.identifier = strings_insert(stream, token_end);
			stream = token_end;

			if (token.identifier.nts == strInt.nts)
				token.type = eToken::keyword_int;
			else if (token.identifier.nts == strReturn.nts)
				token.type = eToken::keyword_return;
			else if (token.identifier.nts == strIf.nts)
				token.type = eToken::keyword_if;
			else if (token.identifier.nts == strElse.nts)
				token.type = eToken::keyword_else;

			output.tokens.push_back(token);
			continue;
		}

		output.failure_location = stream;
		output.failure_reason = "[lex] unsupported data in input";
		debug_break();
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
			fwrite(token.identifier.nts, 1, token.identifier.len, file);
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
		case ':': fputc(token.type, file); continue;
		case ';': fputc(token.type, file); continue;
		case '<': fputc(token.type, file); continue;
		case '=': fputc(token.type, file); continue;
		case '>': fputc(token.type, file); continue;
		case '?': fputc(token.type, file); continue;
		case '{': fputc(token.type, file); continue;
		case '}': fputc(token.type, file); continue;
		case '~': fputc(token.type, file); continue;
		case eToken::logical_and: fprintf(file, "&&"); continue;
		case eToken::logical_or: fprintf(file, "||"); continue;
		case eToken::logical_equal: fprintf(file, "=="); continue;
		case eToken::logical_not_equal: fprintf(file, "!="); continue;
		case eToken::less_than_or_equal: fprintf(file, "<="); continue;
		case eToken::greater_than_or_equal: fprintf(file, ">="); continue;
		case eToken::keyword_int: fprintf(file, "int "); continue;
		case eToken::keyword_return: fprintf(file, "return "); continue;
		case eToken::keyword_if: fprintf(file, "if "); continue;
		case eToken::keyword_else: fprintf(file, "else "); continue;
		}
		
		debug_break();
		break;// detected token we don't know how to handle
	}
}