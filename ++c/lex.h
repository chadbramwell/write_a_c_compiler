#pragma once

enum eToken : char {
	// keyword [1 byte]
	k_int,
	k_return,
	// syntax [1 byte]
	open_parens,
	closed_parens,
	open_brace,
	closed_brace,
	semicolon,
	// qualifer [1 byte] length [1 byte] value [1 - 255 bytes]
	something,
};

struct LexConfig {
	// User should set all of these prior to calling lex.
	// * stream => end_stream is the data to parse.
	// * tokens => end_tokens is the buffer to output tokens to.
	// * failure_reason should be set to NULL.
	// Post-lex, all of these can be used to determine the state of lexing.
	// * stream will equal end_stream or be at the failed parsing location.
	// * end_tokens will be set beyond last token.
	// * if error occurs, failure_reason will be set to a null-terminated string.
	const char* input;
	const char* end_input;
	char* output;
	char* end_output;
	const char* failure_reason;
};

bool lex(LexConfig* io_config);
void unlex(struct _iobuf* out, const char* tokens, const char* tokens_end);