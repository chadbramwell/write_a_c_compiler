#include "ast.h"
#include "lex.h"
#include "debug.h"

// STAGE 3 grammar from: https://norasandler.com/2017/12/15/Write-a-Compiler-3.html
// <program> :: = <function>
// <function> :: = "int" <id> "(" ")" "{" <statement> "}"
// <statement> :: = "return" <exp> ";"
// <exp> :: = <term>{ ("+" | "-") < term > }
// <term> :: = <factor>{ ("*" | "/") < factor > }
// <factor> :: = "(" <exp> ")" | <unary_op> <factor> | <int>

ASTNode* parse_function(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_statement(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_expression(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_logical_and_expression(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_equality_expression(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_relational_expression(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_additive_expression(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_term(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_factor(TokenStream& io_tokens, std::vector<ASTError>& errors);

void append_error(std::vector<ASTError>& errors, const Token* token, const char* reason);

ASTNode* parse_function(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
	// <function> :: = "int" <id> "(" ")" "{" <statement> "}"

	TokenStream tokens = io_tokens;
	ASTNode n;
	n.is_function = true;

	// int
	if (tokens.next->type != eToken::keyword_int)
		return NULL;
	++tokens.next;
	if (tokens.next == tokens.end)
		return NULL;

	// identifier
	n.func_name = tokens.next->identifier;
	if (tokens.next->type != eToken::identifier)
		return NULL;
	++tokens.next;
	if (tokens.next == tokens.end)
		return NULL;

	// parens
	if (tokens.next->type != eToken::open_parens)
	{
		append_error(errors, tokens.next, "expected (");
		return NULL;
	}
	++tokens.next;
	if (tokens.next == tokens.end)
		return NULL;
	if (tokens.next->type != eToken::closed_parens)
	{
		append_error(errors, tokens.next, "expected )");
		return NULL;
	}
	++tokens.next;
	if (tokens.next == tokens.end)
		return NULL;

	// {
	if (tokens.next->type != eToken::open_curly)
		return NULL;
	++tokens.next;
	if (tokens.next == tokens.end)
		return NULL;

	// body
	while (tokens.next != tokens.end && tokens.next->type != eToken::closed_curly)
	{
		ASTNode* s = parse_statement(tokens, errors);
		if (!s)
		{
			if (errors.size() > 0)
				return NULL;
			break;
		}
		
		n.children.push_back(s);
	}

	// }
	if (tokens.next == tokens.end)
	{
		append_error(errors, tokens.next, "expected } but no more tokens");
		return NULL;
	}
	if (tokens.next->type != eToken::closed_curly)
	{
		append_error(errors, tokens.next, "expected }");
		return NULL;
	}
	++tokens.next;
	
	io_tokens = tokens;
	return new ASTNode(n);
}

ASTNode* parse_statement(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
	// <statement> :: = "return" <exp> ";"
	TokenStream& tokens = io_tokens;

	// return
	if (tokens.next->type != eToken::keyword_return)
		return NULL;
	++tokens.next;
	if (tokens.next == tokens.end)
		return NULL;

	ASTNode return_statement;
	return_statement.is_return = true;

	// expression
	ASTNode* expr = parse_expression(tokens, errors);
	if(!expr)
	{
		append_error(errors, tokens.next, "expected expression after return");
		return NULL;
	}
	return_statement.children.push_back(expr);

	// semicolon
	if (tokens.next == tokens.end)
	{
		append_error(errors, tokens.next, "expected ; but no more tokens");
		return NULL;
	}
	if (tokens.next->type != eToken::semicolon)
	{
		append_error(errors, tokens.next, "expected ; at end of return expression");
		return NULL;
	}

	++tokens.next;	
	
	io_tokens = tokens;
	return new ASTNode(return_statement);
}

ASTNode* parse_expression(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
	// <exp> ::= <logical-and-exp> { "||" <logical-and-exp> }

	TokenStream tokens = io_tokens;
	ASTNode* term = parse_logical_and_expression(tokens, errors);

	if(!term)
		return NULL;

	if (tokens.next->type != eToken::logical_or)
	{
		io_tokens = tokens;
		return term;
	}

	ASTNode op;
	op.is_binary_op = true;

	for(;;)
	{
		op.op = tokens.next->type;

		++tokens.next;
		if (tokens.next == tokens.end)
		{
			append_error(errors, tokens.next, "expected term after || but no more tokens");
			return NULL;
		}

		ASTNode* term2 = parse_logical_and_expression(tokens, errors);
		if (!term2)
		{
			append_error(errors, tokens.next, "expected term after ||");
			return NULL;
		}

		op.children.push_back(term);
		op.children.push_back(term2);

		// Gross, but it works. We will wrap around, set our binary_op to next token type and
		// use previous binop as term for next binop
		if (tokens.next != tokens.end && (tokens.next->type == eToken::logical_or))
		{
			term = new ASTNode(op);
			op.children.clear();
			continue;
		}

		break;
	}

	io_tokens = tokens;
	return new ASTNode(op);
}

ASTNode* parse_logical_and_expression(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
	// <logical-and-exp> ::= <equality-exp> { "&&" <equality-exp> }

	TokenStream tokens = io_tokens;
	ASTNode* left_node = parse_equality_expression(tokens, errors);

	if (!left_node)
		return NULL;

	if (tokens.next->type != eToken::logical_and)
	{
		io_tokens = tokens;
		return left_node;
	}

	ASTNode op;
	op.is_binary_op = true;

	for (;;)
	{
		op.op = tokens.next->type;

		++tokens.next;
		if (tokens.next == tokens.end)
		{
			append_error(errors, tokens.next, "expected additive expression after && but no more tokens");
			return NULL;
		}

		ASTNode* right_node = parse_equality_expression(tokens, errors);
		if (!right_node)
		{
			append_error(errors, tokens.next, "expected additive expression after &&");
			return NULL;
		}

		op.children.push_back(left_node);
		op.children.push_back(right_node);

		if (tokens.next != tokens.end &&
			(tokens.next->type == eToken::logical_and))
		{
			left_node = new ASTNode(op);
			op.children.clear();
			continue;
		}

		break;
	}

	io_tokens = tokens;
	return new ASTNode(op);
}

ASTNode* parse_equality_expression(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
	// <equality-exp> ::= <relational-exp> { ("!=" | "==") <relational-exp> }

	TokenStream tokens = io_tokens;
	ASTNode* left_node = parse_relational_expression(tokens, errors);

	if (!left_node)
		return NULL;

	if (tokens.next->type != eToken::logical_not_equal &&
		tokens.next->type != eToken::logical_equal)
	{
		io_tokens = tokens;
		return left_node;
	}

	ASTNode op;
	op.is_binary_op = true;

	for (;;)
	{
		op.op = tokens.next->type;

		++tokens.next;
		if (tokens.next == tokens.end)
		{
			append_error(errors, tokens.next, "expected additive expression after != or == but no more tokens");
			return NULL;
		}

		ASTNode* right_node = parse_relational_expression(tokens, errors);
		if (!right_node)
		{
			append_error(errors, tokens.next, "expected additive expression after != or ==");
			return NULL;
		}

		op.children.push_back(left_node);
		op.children.push_back(right_node);

		// Gross, but it works. We will wrap around, set our binary_op to next token type and
		// use previous binop as term for next binop
		if (tokens.next != tokens.end &&
			(tokens.next->type == eToken::logical_not_equal ||
				tokens.next->type == eToken::logical_equal))
		{
			left_node = new ASTNode(op);
			op.children.clear();
			continue;
		}

		break;
	}

	io_tokens = tokens;
	return new ASTNode(op);
}

ASTNode* parse_relational_expression(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
	// <relational-exp> ::= <additive-exp> { ("<" | ">" | "<=" | ">=") <additive-exp> }

	TokenStream tokens = io_tokens;
	ASTNode* left_node = parse_additive_expression(tokens, errors);

	if (!left_node)
		return NULL;

	if (tokens.next->type != '<' &&
		tokens.next->type != '>' &&
		tokens.next->type != eToken::less_than_or_equal &&
		tokens.next->type != eToken::greater_than_or_equal)
	{
		io_tokens = tokens;
		return left_node;
	}

	ASTNode op;
	op.is_binary_op = true;

	for (;;)
	{
		op.op = tokens.next->type;

		++tokens.next;
		if (tokens.next == tokens.end)
		{
			append_error(errors, tokens.next, "expected additive expression after <, >, <=, or >= but no more tokens");
			return NULL;
		}

		ASTNode* right_node = parse_additive_expression(tokens, errors);
		if (!right_node)
		{
			append_error(errors, tokens.next, "expected additive expression after <, >, <=, or >=");
			return NULL;
		}

		op.children.push_back(left_node);
		op.children.push_back(right_node);

		// Gross, but it works. We will wrap around, set our binary_op to next token type and
		// use previous binop as term for next binop
		if (tokens.next != tokens.end && 
			(tokens.next->type == '<' || 
				tokens.next->type == '>' || 
				tokens.next->type == eToken::less_than_or_equal ||
				tokens.next->type == eToken::greater_than_or_equal))
		{
			left_node = new ASTNode(op);
			op.children.clear();
			continue;
		}

		break;
	}

	io_tokens = tokens;
	return new ASTNode(op);
}

ASTNode* parse_additive_expression(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
	// <additive-exp> ::= <term> { ("+" | "-") <term> }

	TokenStream tokens = io_tokens;
	ASTNode* left_node = parse_term(tokens, errors);

	if (!left_node)
		return NULL;

	if (tokens.next->type != '+' &&
		tokens.next->type != '-')
	{
		io_tokens = tokens;
		return left_node;
	}

	ASTNode op;
	op.is_binary_op = true;

	for (;;)
	{
		op.op = tokens.next->type;

		++tokens.next;
		if (tokens.next == tokens.end)
		{
			append_error(errors, tokens.next, "expected term after + or - but no more tokens");
			return NULL;
		}

		ASTNode* right_node = parse_term(tokens, errors);
		if (!right_node)
		{
			append_error(errors, tokens.next, "expected term after + or -");
			return NULL;
		}

		op.children.push_back(left_node);
		op.children.push_back(right_node);

		// Gross, but it works. We will wrap around, set our binary_op to next token type and
		// use previous binop as term for next binop
		if (tokens.next != tokens.end && (tokens.next->type == '-' || tokens.next->type == '+'))
		{
			left_node = new ASTNode(op);
			op.children.clear();
			continue;
		}

		break;
	}

	io_tokens = tokens;
	return new ASTNode(op);
}

ASTNode* parse_term(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
	// <term> :: = <factor>{ ("*" | "/") < factor > }

	TokenStream tokens = io_tokens;

	ASTNode* left_node = parse_factor(tokens, errors);
	if(!left_node)
		return NULL;

	if (tokens.next->type != '*' &&
		tokens.next->type != '/')
	{
		io_tokens = tokens;
		return left_node;
	}

	ASTNode op;
	op.is_binary_op = true;

	for (;;)
	{
		op.op = tokens.next->type;

		++tokens.next;
		if (tokens.next == tokens.end)
		{
			append_error(errors, tokens.next, "expected factor after * or / but no more tokens");
			return NULL;
		}

		ASTNode* right_node = parse_factor(tokens, errors);
		if (!right_node)
		{
			append_error(errors, tokens.next, "expected factor after * or /");
			return NULL;
		}

		op.children.push_back(left_node);
		op.children.push_back(right_node);

		// Gross, but it works. We will wrap around, set our binary_op to next token type and
		// use previous binop as term for next binop
		if (tokens.next != tokens.end && (tokens.next->type == '*' || tokens.next->type == '/'))
		{
			left_node = new ASTNode(op);
			op.children.clear();
			continue;
		}

		break;
	}

	io_tokens = tokens;
	return new ASTNode(op);
}

ASTNode* parse_factor(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
	// <factor> :: = "(" <exp> ")" | <unary_op> <factor> | <int>
	if (io_tokens.next == io_tokens.end)
		return NULL;

	if (io_tokens.next->type == eToken::open_parens)
	{
		TokenStream tokens = io_tokens;
		++tokens.next;
		if (tokens.next == tokens.end)
			return NULL;

		ASTNode* expression = parse_expression(tokens, errors);
		if(!expression)
		{
			append_error(errors, tokens.next, "expected expression after (");
			return NULL;
		}

		if (tokens.next->type != eToken::closed_parens)
		{
			append_error(errors, tokens.next, "expected ) after expression");
			return NULL;
		}

		++tokens.next;

		io_tokens = tokens;
		return expression;
	}

	if (io_tokens.next->type == '!' ||
		io_tokens.next->type == '-' ||
		io_tokens.next->type == '~')
	{
		// unary operator, expects factor
		ASTNode n;
		n.is_unary_op = true;
		n.op = io_tokens.next->type;

		TokenStream tokens = io_tokens;
		++tokens.next;

		if (tokens.next == tokens.end)
			return NULL;

		ASTNode* factor = parse_factor(tokens, errors);
		if(!factor)
			return NULL;
		n.children.push_back(factor);

		io_tokens = tokens;
		return new ASTNode(n);
	}

	if (io_tokens.next->type == eToken::constant_number)
	{
		ASTNode n;
		n.is_number = true;
		n.number = io_tokens.next->number;
		++io_tokens.next;
		return new ASTNode(n);
	}

	return NULL;
}

ASTNode* ast(TokenStream& tokens, std::vector<ASTError>& errors)
{
	ASTNode n;
	n.is_program = true;

	while (tokens.next != tokens.end)
	{
		if (ASTNode* f = parse_function(tokens, errors))
		{
			n.children.push_back(f);
			continue;
		}
		break;
	}

	// success if we parsed all tokens
	if (tokens.next == tokens.end)
	{
		return new ASTNode(n);
	}

	return NULL;
}

void dump_ast(FILE* file, const ASTNode& self, int spaces_indent)
{
	if (self.is_function)
	{
		fprintf(file, "%*cFUN %s %s:\n", spaces_indent, ' ', "INT", self.func_name.c_str());
		spaces_indent += 2;
		fprintf(file, "%*cparams: ()\n", spaces_indent, ' ');
		fprintf(file, "%*cbody:\n", spaces_indent, ' ');
		spaces_indent += 2;
	}
	else if (self.is_return)
		fprintf(file, "%*cRETURN ", spaces_indent, ' ');
	else if (self.is_number)
		fprintf(file, "Int<%" PRIi64 ">", self.number);
	else if (self.is_program)
		fprintf(file, "program\n");
	else if (self.is_unary_op)
	{
		fprintf(file, "UnOp(%c, ", self.op);
		for (size_t i = 0; i < self.children.size(); ++i)
		{
			dump_ast(file, *self.children[i], 0);
		}
		fputc(')', file);
		return;
	}
	else if (self.is_binary_op)
	{
		assert(self.children.size() == 2);
		fprintf(file, "BinOp(");
		switch (self.op)
		{
		case '*': fputc(self.op, file); break;
		case '+': fputc(self.op, file); break;
		case '-': fputc(self.op, file); break;
		case '/': fputc(self.op, file); break;
		case '<': fputc(self.op, file); break;
		case '>': fputc(self.op, file); break;
		case eToken::logical_and: fprintf(file, "&&"); break;
		case eToken::logical_or: fprintf(file, "||"); break;
		case eToken::logical_equal: fprintf(file, "=="); break;
		case eToken::logical_not_equal: fprintf(file, "!="); break;
		case eToken::less_than_or_equal: fprintf(file, "<="); break;
		case eToken::greater_than_or_equal: fprintf(file, ">="); break;
		default:
			debug_break();
			fprintf(file, "???");
		}
		fprintf(file, ", ");
		dump_ast(file, *self.children[0], 0);
		fprintf(file, ", ");
		dump_ast(file, *self.children[1], 0);
		fputc(')', file);
		return;
	}
	else
		fprintf(file, "%*c?????\n", spaces_indent, ' ');

	for (size_t i = 0; i < self.children.size(); ++i)
	{
		dump_ast(file, *self.children[i], spaces_indent + 2);
	}

	if (self.is_program)
		fputc('\n', file);
}

void append_error(std::vector<ASTError>& errors, const Token* token, const char* reason)
{
	ASTError e;
	e.token = token;
	e.reason = reason;
	errors.push_back(e);

	debug_break();
}

void draw_error_caret_at(FILE* out, const LexInput& lex, const char* error_location, const char* error_reason)
{
	const char* const file_start = lex.stream;
	const char* const file_end = lex.stream + lex.length;

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

	uint64_t char_num = uint64_t(error_location - line_start);

	fprintf(out, "%s:%" PRIi64 ":%" PRIi64 ": error: %s\n",
		lex.filename,
		line_num,
		char_num,
		error_reason);
	fprintf(out, "%.*s\n",
		int(line_end - line_start),
		line_start);

	while (char_num)
	{
		fputc(' ', out);
		--char_num;
	}

	fputc('^', out);
	fputc('\n', out);
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

		uint64_t char_num = uint64_t(error_location - line_start);

		fprintf(file, "%s:%" PRIi64 ":%" PRIi64 ": error: %s\n",
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
