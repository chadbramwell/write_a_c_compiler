#pragma once
#include <inttypes.h>
#include <cstdio>
#include <string>
#include <vector>

enum eToken : uint8_t
{
	identifier = 0,
	constant_number = 1,
	open_parens = '(', //40
	closed_parens = ')',//41
	semicolon = ';',//59
	open_brace = '{',//123
	closed_brace = '}',//125

	keyword_int = 128,
	keyword_return,
};

struct Token
{
	eToken type;
	const char* location;

	std::string identifier;
	uint64_t number;

	Token(eToken t, const char* loc) : type(t), location(loc) {}
};

struct LexInput
{
	const char* stream;
	uint64_t length;
};

struct LexOutput
{
	std::vector<Token> tokens;

	const char* failure_location;
	const char* failure_reason;
};

bool lex(const LexInput& input, LexOutput& output);
void unlex(FILE* file, const LexOutput& lex);