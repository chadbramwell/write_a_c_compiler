#include "ast.h"
#include "lex.h"

bool function_definition(TokenStream& tokens, ASTNode& parent);
bool declaration_specifier(TokenStream& tokens, DeclSpec& out);
bool statement(TokenStream& tokens, ASTNode& parent);
bool expression(TokenStream& tokens, ASTNode& parent);

bool function_definition(TokenStream& io_tokens, ASTNode& parent)
{
	TokenStream tokens = io_tokens;
	std::unique_ptr<AST_Function> func(new AST_Function);

	// decl_spec
	if (!declaration_specifier(tokens, func->return_type))
		return false;
	if (tokens.next == tokens.end)
		return false;

	// identifier
	func->name = tokens.next->identifier;
	if (tokens.next->type != eToken::identifier)
		return false;
	++tokens.next;
	if (tokens.next == tokens.end)
		return false;

	// parens
	if(tokens.next->type != eToken::open_parens)
		return false;
	++tokens.next;
	if (tokens.next == tokens.end)
		return false;
	if (tokens.next->type != eToken::closed_parens)
		return false;
	++tokens.next;
	if (tokens.next == tokens.end)
		return false;

	// open brace
	if (tokens.next->type != eToken::open_brace)
		return false;
	++tokens.next;
	if (tokens.next == tokens.end)
		return false;

	// body
	while (tokens.next != tokens.end)
	{
		if (!statement(tokens, *func))
			break;
	}

	// close brace
	if (tokens.next == tokens.end)
		return false;
	if (tokens.next->type != eToken::closed_brace)
		return false;
	++tokens.next;
	
	parent.children.push_back(std::move(func));
	io_tokens = tokens;
	return true;
}

bool declaration_specifier(TokenStream& tokens, DeclSpec& out)
{
	if (tokens.next->type == eToken::keyword_int)
	{
		out.type = eDeclarationSpecifier::ds_int;
		++tokens.next;
		return true;
	}
	return false;
}

bool return_statement(TokenStream& io_tokens, ASTNode& parent)
{
	TokenStream& tokens = io_tokens;
	std::unique_ptr<AST_ReturnStatement> r(new AST_ReturnStatement);

	// return
	if (tokens.next->type != eToken::keyword_return)
		return false;
	++tokens.next;
	if (tokens.next == tokens.end)
		return false;

	// expression
	if (!expression(tokens, *r))
		return false;

	// semicolon
	if (tokens.next == tokens.end)
		return false;
	if (tokens.next->type != eToken::semicolon)
		return false;
	++tokens.next;

	parent.children.push_back(std::move(r));
	io_tokens = tokens;
	return true;
}

bool statement(TokenStream& tokens, ASTNode& parent)
{
	if (return_statement(tokens, parent))
		return true;
	return false;
}

bool expression(TokenStream& tokens, ASTNode& parent)
{
	if (tokens.next->type == eToken::constant_number)
	{
		std::unique_ptr<AST_Number> n(new AST_Number);
		n->value = tokens.next->number;
		parent.children.push_back(std::move(n));
		++tokens.next;
		return true;
	}
	return false;
}

bool ast(TokenStream& tokens, AST& out)
{
	while (tokens.next != tokens.end)
	{
		if (!function_definition(tokens, out.root))
			break;
	}

	// success if we parsed all tokens
	return tokens.next == tokens.end;
}

static void dump_ast_recursive(FILE* file, const ASTNode& self, int tabs)
{
	printf("%*c", tabs, '\t');
	
	if (auto f = dynamic_cast<const AST_Function*>(&self))
		printf("%s\n", f->name.c_str());
	else if (auto r = dynamic_cast<const AST_ReturnStatement*>(&self))
		printf("return\n");
	else if (auto n = dynamic_cast<const AST_Number*>(&self))
		printf("%" PRIu64 "\n", n->value);
	else
		printf("?????\n");

	for (size_t i = 0; i < self.children.size(); ++i)
	{
		dump_ast_recursive(file, *self.children[i], tabs + 1);
	}
}

void dump_ast(FILE* file, const AST& a)
{
	dump_ast_recursive(file, a.root, 0);
}
