#pragma once
#include <inttypes.h>
#include <cstdio>
#include <string>
#include <vector>

// Below is an attempt at a simple lexer. It's not meant to be optimal.

enum eToken : uint8_t
{
	identifier = 0,
	constant_number = 1,

	bang			= '!',//ASCII 33
	open_parens		= '(',//40
	closed_parens	= ')',//41
	star			= '*',//42
	plus			= '+',//43
	dash			= '-',//45
	forward_slash	= '/',//47
	semicolon		= ';',//59
	open_curly		= '{',//123
	closed_curly	= '}',//125
	tilde			= '~',//126

	keyword_int = 128,
	keyword_return,
};

struct Token
{
	eToken type;
	const char* location; // simplest way to store this data, but assumes LexInput stream will last for as long as this is needed.

	std::string identifier; // only valid if type == eToken::identifier or one of the keywords
	uint64_t number; // only valid if type == eToken::constant_number

	explicit Token(eToken t, const char* loc) : type(t), location(loc) {}
};

struct LexInput
{
	const char* filename;
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
void dump_lex(FILE* file, const LexOutput& lex);