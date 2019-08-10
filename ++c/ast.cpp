#include "ast.h"
#include "lex.h"

bool function_definition(TokenStream& tokens, std::vector<ASTError>& o_errors, ASTNode& parent);
bool declaration_specifier(TokenStream& tokens, std::vector<ASTError>& o_errors, DeclSpec& out);
bool statement(TokenStream& tokens, std::vector<ASTError>& o_errors, ASTNode& parent);
bool expression(TokenStream& tokens, std::vector<ASTError>& o_errors, ASTNode& parent);

void append_error(std::vector<ASTError>& o_errors, const Token* token, const char* reason);

bool function_definition(TokenStream& io_tokens, std::vector<ASTError>& o_errors, ASTNode& parent)
{
	TokenStream tokens = io_tokens;
	std::unique_ptr<AST_Function> func(new AST_Function);

	// decl_spec
	if (!declaration_specifier(tokens, o_errors, func->return_type))
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
	if (tokens.next->type != eToken::open_parens)
	{
		append_error(o_errors, tokens.next, "expected (");
		return false;
	}
	++tokens.next;
	if (tokens.next == tokens.end)
		return false;
	if (tokens.next->type != eToken::closed_parens)
	{
		append_error(o_errors, tokens.next, "expected )");
		return false;
	}
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
		if (!statement(tokens, o_errors, *func))
		{
			if (!o_errors.empty())
				return false;
			break;
		}
	}

	// }
	if (tokens.next == tokens.end)
	{
		append_error(o_errors, tokens.next, "expected } but no more tokens");
		return false;
	}
	if (tokens.next->type != eToken::closed_curly)
	{
		append_error(o_errors, tokens.next, "expected }");
		return false;
	}
	++tokens.next;
	
	parent.children.push_back(std::move(func));
	io_tokens = tokens;
	return true;
}

bool declaration_specifier(TokenStream& tokens, std::vector<ASTError>& o_errors, DeclSpec& out)
{
	if (tokens.next->type == eToken::keyword_int)
	{
		out.type = eDeclarationSpecifier::ds_int;
		++tokens.next;
		return true;
	}
	return false;
}

bool return_statement(TokenStream& io_tokens, std::vector<ASTError>& o_errors, ASTNode& parent)
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
	if (!expression(tokens, o_errors, *r))
	{
		append_error(o_errors, tokens.next, "expected expression after return");
		return false;
	}

	// semicolon
	if (tokens.next == tokens.end)
	{
		append_error(o_errors, tokens.next, "expected ; but no more tokens");
		return false;
	}
	if (tokens.next->type != eToken::semicolon)
	{
		append_error(o_errors, tokens.next, "expected ; at end of return expression");
		return false;
	}
	++tokens.next;

	parent.children.push_back(std::move(r));
	io_tokens = tokens;
	return true;
}

bool statement(TokenStream& tokens, std::vector<ASTError>& o_errors, ASTNode& parent)
{
	if (return_statement(tokens, o_errors, parent))
		return true;
	return false;
}

bool expression(TokenStream& io_tokens, std::vector<ASTError>& o_errors, ASTNode& parent)
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

			if (!expression(tokens, o_errors, *uop))
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
		if (!function_definition(tokens, out.errors, out.root))
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

void append_error(std::vector<ASTError>& o_errors, const Token* token, const char* reason)
{
	ASTError e;
	e.token = token;
	e.reason = reason;
	o_errors.push_back(e);
}

void dump_ast_errors(FILE* file, const std::vector<ASTError>& errors, const LexInput& lex)
{
	fprintf(file, "\nBEGIN ERRORS=====\n");
	for (size_t i = 0; i < errors.size(); ++i)
	{
		const ASTError& error = errors[i];

		const char* const file_start = lex.stream;
		const char* const file_end = lex.stream + lex.length;

		const char* error_location = error.token->start;

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

		// HACK - this is a dangerous way to get previous token.
		const Token* prev_token = error.token - 1;
		// END HACK
		if (line_start < prev_token->end)
		{
			// generate squiggles from previous token to error.
			const char* draw_cursor = line_start;
			while (draw_cursor < prev_token->end)
			{
				fputc(' ', file);
				++draw_cursor;
			}
			while (draw_cursor < error_location)
			{
				fputc('~', file);
				++draw_cursor;
			}
		}
		else
		{
			while (char_num)
			{
				fputc(' ', file);
				--char_num;
			}
		}

		fputc('^', file);
		fputc('\n', file);
	}
	fprintf(file, "======END ERRORS\n");
}
