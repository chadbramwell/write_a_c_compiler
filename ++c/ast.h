#pragma once
#include "lex.h"

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
	virtual ~ASTNode() {}
	std::vector<std::unique_ptr<ASTNode>> children;
};

struct AST_Program : public ASTNode
{

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

struct AST_Function : public ASTNode
{
	DeclSpec return_type;
	std::string name;
	std::vector<ASTNode*> statements;
};

struct AST_ReturnStatement : public ASTNode
{
};

struct AST_ConstantNumber : public ASTNode
{
	uint64_t number;
};

struct AST_UnaryOperation : public ASTNode
{
	eToken uop;
};

struct AST
{
	AST_Program root;
	std::vector<ASTError> errors;
};

bool ast(TokenStream& tokens, AST& out);
void dump_ast(FILE* file, const AST& a);
void dump_ast_errors(FILE* file, const std::vector<ASTError>& errors, const LexInput& lex);
