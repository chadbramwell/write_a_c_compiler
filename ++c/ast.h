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
	bool is_block_list; // many children
	bool is_return; // expects 1 child
	bool is_if; // expects 2-3 children: condition, true statement, [false statement]
	bool is_for; // expects 4 children: init, condition, update, loop body
	bool is_while; // expects 2 children: condition, loop body
	bool is_do_while; // expects 2 children: loop body, condition
	bool is_variable_declaration; // expects var_name, **type assumed to be int TODO**
	bool is_variable_assignment; // expects var_name and 1 child
	bool is_variable_usage; // expects var_name
	bool is_number;
	bool is_unary_op;
	bool is_binary_op;
	bool is_ternery_op; //?: (op ignored)
	bool is_break_or_continue_op; //break or continue in op
	bool is_empty;

	eToken op;
	str func_name;
	str var_name;
	int64_t number;

	std::vector<ASTNode*> children; // these leak, but who cares. we do our job and then end the program.

	ASTNode()
		: is_program(false)
		, is_function(false)
		, is_block_list(false)
		, is_return(false)
		, is_if(false)
		, is_for(false)
		, is_while(false)
		, is_do_while(false)
		, is_variable_declaration(false)
		, is_variable_assignment(false)
		, is_variable_usage(false)
		, is_number(false)
		, is_unary_op(false)
		, is_binary_op(false)
		, is_ternery_op(false)
		, is_break_or_continue_op(false)
		, is_empty(false)
		, op(eToken::UNKNOWN)
		, func_name({})
		, var_name({})
		, number(0)
	{
	}
};

ASTNode* ast(TokenStream& tokens, std::vector<ASTError>& errors);
void dump_ast(FILE* file, const ASTNode& self, int spaces_indent);
void dump_ast_errors(FILE* file, const std::vector<ASTError>& errors, const LexInput& lex);
void draw_error_caret_at(FILE* out, const LexInput& lex, const char* error_location, const char* error_reason);
