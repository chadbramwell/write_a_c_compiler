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

	// {
	if (tokens.next->type != eToken::open_curly)
		return false;
	++tokens.next;
	if (tokens.next == tokens.end)
		return false;

	// body
	while (tokens.next != tokens.end && tokens.next->type != eToken::closed_curly)
	{
		if (!statement(tokens, *func))
			return false;
	}

	// }
	if (tokens.next == tokens.end)
		return false;
	if (tokens.next->type != eToken::closed_curly)
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

bool expression(TokenStream& io_tokens, ASTNode& parent)
{
	if (io_tokens.next->type == eToken::constant_number)
	{
		std::unique_ptr<AST_ConstantNumber> n(new AST_ConstantNumber);
		n->number = io_tokens.next->number;
		parent.children.push_back(std::move(n));
		++io_tokens.next;
		return true;
	}
	
	switch (io_tokens.next->type)
	{
	case '!':
	case '-':
	case '~':
		{
			// unary operator, expects expression
			std::unique_ptr<AST_UnaryOperation> uop(new AST_UnaryOperation);
			uop->uop = io_tokens.next->type;

			TokenStream tokens = io_tokens;
			++tokens.next;

			if (tokens.next == tokens.end)
				return false;

			if (!expression(tokens, *uop))
				return false;

			parent.children.push_back(std::move(uop));
			io_tokens = tokens;
			return true;
		}
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

const char* to_string(eDeclarationSpecifier ds)
{
	switch (ds)
	{
	case ds_int: return "INT";
	}
	return "????";
}

static void dump_ast_recursive(FILE* file, const ASTNode& self, int spaces_indent)
{
	if (auto f = dynamic_cast<const AST_Function*>(&self))
	{
		fprintf(file, "%*cFUN %s %s:\n", spaces_indent, ' ', to_string(f->return_type.type), f->name.c_str());
		spaces_indent += 2;
		fprintf(file, "%*cparams: ()\n", spaces_indent, ' ');
		fprintf(file, "%*cbody:\n", spaces_indent, ' ');
		spaces_indent += 2;
	}
	else if (auto r = dynamic_cast<const AST_ReturnStatement*>(&self))
		fprintf(file, "%*cRETURN ", spaces_indent, ' ');
	else if (auto c = dynamic_cast<const AST_ConstantNumber*>(&self))
		fprintf(file, "Int<%" PRIu64 ">", c->number);
	else if (auto p = dynamic_cast<const AST_Program*>(&self))
		fprintf(file, "program\n");
	else if (auto u = dynamic_cast<const AST_UnaryOperation*>(&self))
	{
		fprintf(file, "UnOp(%c, ", u->uop);
		for (size_t i = 0; i < self.children.size(); ++i)
		{
			dump_ast_recursive(file, *self.children[i], 0);
		}
		fputc(')', file);
		return;
	}
	else
		fprintf(file, "%*c?????\n", spaces_indent, ' ');

	for (size_t i = 0; i < self.children.size(); ++i)
	{
		dump_ast_recursive(file, *self.children[i], spaces_indent + 2);
	}
}

void dump_ast(FILE* file, const AST& a)
{
	dump_ast_recursive(file, a.root, 2);
}
