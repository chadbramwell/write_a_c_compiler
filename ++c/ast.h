#pragma once
#include "lex.h"

enum eAST : char
{
	program,
	function,
	statement,
	expression,
};

struct ASTNode
{
	eAST type;
	std::vector<ASTNode*> children;

	Token* return_type;
	Token* function_name;
	Token* params_start; //(
	Token* params_end; //)
	Token* body_start;//{
	Token* body_end; //}
};

struct ASTInput
{
	const Token* tokens;
	uint64_t num_tokens;
};

struct ASTOutput
{
	ASTNode root;
	const char* failure_reason;
};

bool ast(const ASTInput& input, ASTOutput& output);
void dump_ast(const ASTOutput& output);
