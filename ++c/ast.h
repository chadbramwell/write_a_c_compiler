#pragma once
#include "lex.h"

// Below is an attempt at a simple AST generator. It's meant to be simple.
// note: ASTNodes are dynamically allocated and never freed.

struct TokenStream
{
	const Token* next;
	const Token* end;
};

struct ASTError
{
	const Token* token;
	const char* reason;
};

struct ASTNode
{
	bool is_program;
	bool is_function;
	bool is_return;
	bool is_if;
	bool is_variable_declaration;
	bool is_variable_assignment;
	bool is_variable_usage;
	bool is_number;
	bool is_unary_op;
	bool is_binary_op;

	eToken op;
	str func_name; //TODO: replace these with indicies into a string table
	str var_name;
	int64_t number;

	std::vector<ASTNode*> children; // these leak, but who cares. we do our job and then end the program.

	ASTNode()
		: is_program(false)
		, is_function(false)
		, is_return(false)
		, is_if(false)
		, is_variable_declaration(false)
		, is_variable_assignment(false)
		, is_variable_usage(false)
		, is_number(false)
		, is_unary_op(false)
		, is_binary_op(false)
		, op(eToken::UNKNOWN)
		, number(0)
	{
	}
};

ASTNode* ast(TokenStream& tokens, std::vector<ASTError>& errors);
void dump_ast(FILE* file, const ASTNode& self, int spaces_indent);
void dump_ast_errors(FILE* file, const std::vector<ASTError>& errors, const LexInput& lex);
void draw_error_caret_at(FILE* out, const LexInput& lex, const char* error_location, const char* error_reason);
