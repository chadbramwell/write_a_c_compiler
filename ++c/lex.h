#pragma once
#include "strings.h"
#include <inttypes.h>
#include <cstdio>
#include <vector>

// Below is an attempt at a simple lexer. It's not meant to be optimal.

enum eToken : uint8_t
{
	UNKNOWN = 0,
	identifier = 1,
	constant_number = 2,

	logical_not		= '!',//ASCII 33
	bitwise_and		= '&',//38
	open_parens		= '(',//40
	closed_parens	= ')',//41
	star			= '*',//42 - could be multiply, pointer decl, pointer deref
	plus			= '+',//43 - could be sign decl or binary add
	dash			= '-',//45 - could be sign decl or binary sub
	forward_slash	= '/',//47
	colon			= ':',//58
	semicolon		= ';',//59
	less_than		= '<',//60
	assignment		= '=',//61
	greater_than	= '>',//62
	question_mark	= '?',//63
	open_curly		= '{',//123
	bitwise_or		= '|',//124
	closed_curly	= '}',//125
	bitwise_not		= '~',//126

	logical_and = 127,		// &&
	logical_or,				// ||
	logical_equal,			// ==
	logical_not_equal,		// !=
	less_than_or_equal,		// <=
	greater_than_or_equal,	// >=

	keyword_int,
	keyword_return,
	keyword_if,
	keyword_else
};

struct Token
{
	eToken type;
	const char* start; // simplest way to store this data, but assumes LexInput stream will last for as long as this is needed.
	const char* end;

	str identifier; // only valid if type == eToken::identifier or one of the keywords
	uint64_t number; // only valid if type == eToken::constant_number

	explicit Token(eToken _type, const char* _start, const char* _end) 
		: type(_type), start(_start), end(_end)
	{}
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
