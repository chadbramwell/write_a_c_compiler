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

enum eDeclarationSpecifier
{
	ds_int
};
struct DeclSpec
{
	eDeclarationSpecifier type;
	//TODO: alignment, pointer depth (i.e. how many "int *****...")
};

struct ASTNode
{
	bool is_program;
	bool is_function;
	bool is_return;
	bool is_expression;
	bool is_number;
	bool is_unary_op;
	std::vector<ASTNode*> children; // these leak, but who cares. we do our job and then end the program.

	DeclSpec func_return_type;
	std::string func_name;

	uint64_t number;

	eToken unary_op;
	
	// HACK. I really wish C/C++ zero-initialized by default, it would save so much programmer time.
	ASTNode() { memset(this, 0, sizeof(ASTNode)); children = std::vector<ASTNode*>(); func_name = std::string(); }
};

struct AST
{
	ASTNode root;
	std::vector<ASTError> errors;
};

bool ast(TokenStream& tokens, AST& out);
void dump_ast(FILE* file, const AST& a);
void dump_ast_errors(FILE* file, const std::vector<ASTError>& errors, const LexInput& lex);
