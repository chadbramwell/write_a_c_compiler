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
		const Token* statement_start = tokens.next;
		if (!statement(tokens, *func))
		{
			ASTError e;
			e.token = statement_start;
			e.reason = "expected statement in function";
			func->errors.push_back(e);
			parent.children.push_back(std::move(func));
			return false;
		}
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
	{
		ASTError e;
		e.token = tokens.next;
		e.reason = "expected semicolon for end of return statement";
		r->errors.push_back(e);
		parent.children.push_back(std::move(r));
		return false;
	}
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

void dump_ast_errors_recursive(FILE* file, const LexInput& lex, const ASTNode& self)
{
	for (size_t i = 0; i < self.children.size(); ++i)
	{
		dump_ast_errors_recursive(file, lex, *self.children[i]);
	}
	
	for (size_t i = 0; i < self.errors.size(); ++i)
	{
		const ASTError& error = self.errors[i];

		const char* const file_start = lex.stream;
		const char* const file_end = lex.stream + lex.length;		

		const char* error_location = error.token->location;
		
		// line_num & line_start
		uint64_t line_num = 0;
		const char* line_start = file_start;
		const char* line_end = file_end;
		while (line_start < error_location)
		{
			++line_num;
			const char* new_line_start = line_start;
			while (new_line_start < file_end && *new_line_start != '\n')
				++new_line_start;
			if (new_line_start < file_end) // we must be at '\n', skip it
				++new_line_start;
			if (new_line_start < error_location)
			{
				line_start = new_line_start;
				continue;
			}

			line_end = new_line_start - 2;
			break;
		}

		uint64_t char_num = (error_location - line_start);

		fprintf(file, "%s:%" PRIu64 ":%" PRIu64 ": error: %s\n",
			lex.filename,
			line_num,
			char_num,
			error.reason);
		fprintf(file, "%.*s\n", 
			int(line_end - line_start), 
			line_start);
		
		while (char_num)
		{
			fputc(' ', file);
			--char_num;
		}
		fputc('^', file);
		fputc('\n', file);
	}
}

void dump_ast_errors(FILE* file, const LexInput& lex, const AST& a)
{
	fprintf(file, "\nBEGIN ERRORS=====\n");
	dump_ast_errors_recursive(file, lex, a.root);
	fprintf(file, "======END ERRORS\n");
}
