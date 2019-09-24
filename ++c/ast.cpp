#include "ast.h"
#include "lex.h"
#include "debug.h"

// STAGE 5 grammar from: https://norasandler.com/2018/01/08/Write-a-Compiler-5.html
// Updates from other stages including: https://norasandler.com/2018/03/14/Write-a-Compiler-7.html
// QUICK HELP: https://en.cppreference.com/w/cpp/language/operator_precedence
// (XX) <-- operator precendence
//      <program> ::= { <function> | <declaration> }
//      <function> ::= "int" <id> "(" [ "int" <id> { "," "int" <id> } ] ")" ( "{" { <block-item> } "}" | ";" )
//      <block-item> ::= <statement> | <declaration>
//      <declaration> ::= "int" <id> [ = <exp> ] ";"
//      <statement> ::= "return" <exp> ";"
//                    | <exp> ";"
//                    | "if" "(" <exp> ")" <statement> [ "else" <statement> ]
//                    | "{" { <block-item> } "}
//                    | "for" "(" <exp-option> ";" <exp-option> ";" <exp-option> ")" <statement>
//                    | "for" "(" <declaration> <exp-option> ";" <exp-option> ")" <statement>
//                    | "while" "(" <exp> ")" <statement>
//                    | "do" <statement> "while" <exp> ";"
//                    | "break" ";"
//                    | "continue" ";"
//                    | ";"
//      <exp> ::= <id> "=" <exp> | <conditional-exp>
// (16) <conditional-exp> ::= <logical-or-exp> [ "?" <exp> ":" <conditional-exp> ]
// (15) <logical-or-exp> ::= <logical-and-exp> { "||" <logical-and-exp> }
// (14) <logical-and-exp> ::= <equality-exp> { "&&" <equality-exp> }
// (10) <equality-exp> ::= <relational-exp> { ("!=" | "==") <relational-exp> }
// ( 9) <relational-exp> ::= <additive-exp> { ("<" | ">" | "<=" | ">=") <additive-exp> } .. 
// ( 6) <additive-exp> ::= <term>{ ("+" | "-") <term> }
// ( 5) <term> ::= <factor> { ("*" | "/" | "%") <factor> }
//      <factor> ::= "(" <exp> ")" | <unary_op> <factor> | <int> | <id>
// ( 3) <unary_op> ::= "!" | "~" | "-"

// GENERAL RULE FOR FUNCTIONS BELOW: update io_tokens only if succesfully parsed
//    - This makes it easier to move the instruction pointer during debugging and it should allow for simpler function call structure
// GENERAL RULE FOR FUNCTIONS BELOW: assume there's at least one item in the token stream

ASTNode* parse_program(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_function(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_function_call(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_block_item(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_declaration(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_declaration_with_semicolon(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_statement(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_expression(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_conditional_exp(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_logical_or_expression(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_logical_and_expression(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_equality_expression(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_relational_expression(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_additive_expression(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_term(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_factor(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_for_loop(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_while_loop(TokenStream& io_tokens, std::vector<ASTError>& errors);
ASTNode* parse_do_while_loop(TokenStream& io_tokens, std::vector<ASTError>& errors);

bool expect_and_advance(TokenStream& io_tokens, eToken expected_token, std::vector<ASTError>& errors);
void append_error(std::vector<ASTError>& errors, const Token* token, const char* reason);

ASTNode* parse_program(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    // <program> ::= { <function> | <declaration> }
    if (io_tokens.next == io_tokens.end)
    {
        debug_break();
        return NULL;
    }
    TokenStream tokens = io_tokens;

    ASTNode n = {};
    n.type = AST_program;

    while (tokens.next != tokens.end)
    {
        if (ASTNode* f = parse_function(tokens, errors))
        {
            astn_push(&n.program, f);
            continue;
        }
        if (ASTNode* d = parse_declaration_with_semicolon(tokens, errors))
        {
            astn_push(&n.program, d);
            continue;
        }
        break;
    }

    // success if we parsed all tokens
    if (tokens.next == tokens.end)
    {
        io_tokens = tokens;
        return new ASTNode(n);
    }

    return NULL;
}

ASTNode* parse_function(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    // <function> :: = "int" <id> "("["int" <id> { "," "int" <id> }] ")" ("{" { <block - item> } "}" | ";")
    assert(io_tokens.next != io_tokens.end);
    TokenStream tokens = io_tokens;

    str func_name;
    ASTNodeArray func_params = {};

    // int
    if (!expect_and_advance(tokens, eToken::keyword_int, errors)) return NULL;
    if (tokens.next == tokens.end)
        return NULL;

    // identifier
    func_name = tokens.next->identifier;
    if (tokens.next->type != eToken::identifier)
        return NULL;
    ++tokens.next;
    if (tokens.next == tokens.end)
        return NULL;

    // (
    if (!expect_and_advance(tokens, eToken::open_parens, errors)) return NULL;

    // params
    while (tokens.next != tokens.end)
    {
        ASTNode* decl = parse_declaration(tokens, errors);
        if (!decl) break;
        if (decl->type != AST_var)
        {
            debug_break();
            return NULL;
        }
        astn_push(&func_params, decl);

        if (tokens.next == tokens.end)
        {
            append_error(errors, tokens.next, "out of tokens while parsing params");
            return NULL;
        }

        if (tokens.next->type == eToken::comma)
            ++tokens.next;
    }

    // )
    if (!expect_and_advance(tokens, eToken::closed_parens, errors)) return NULL;

    // either end with ; or we get a func body
    if (tokens.next == tokens.end)
        return NULL;
    if (tokens.next->type == eToken::semicolon)
    {
        ++tokens.next;

        io_tokens = tokens;
        ASTNode* n = new ASTNode;
        n->type = AST_fdecl;
        n->fdecl.name = func_name;
        n->fdecl.params = func_params;
        return n;
    }

    // func body! {
    if (!expect_and_advance(tokens, eToken::open_curly, errors)) return NULL;

    // body
    ASTNodeArray func_body = {};
    while (tokens.next != tokens.end)
    {
        ASTNode* bi = parse_block_item(tokens, errors);
        if (!bi)
        {
            if (errors.size() > 0)
                return NULL;
            break;
        }

        astn_push(&func_body, bi);
    }

    // }
    if (!expect_and_advance(tokens, eToken::closed_curly, errors)) return NULL;

    io_tokens = tokens;
    ASTNode* n = new ASTNode;
    n->type = AST_fdef;
    n->fdef.name = func_name;
    n->fdef.params = func_params;
    n->fdef.body = func_body;
    return n;
}

ASTNode* parse_function_call(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    assert(io_tokens.next != io_tokens.end);
    TokenStream tokens = io_tokens;

    if (tokens.next + 3 >= tokens.end) // id()
        return NULL;

    if (tokens.next[0].type != eToken::identifier ||
        tokens.next[1].type != eToken::open_parens)
        return NULL;

    ASTNode n = {};
    n.type = AST_fcall;
    n.fcall.name = tokens.next->identifier;
    
    tokens.next += 2; // skip id and (

    while (tokens.next != tokens.end)
    {
        ASTNode* arg = parse_expression(tokens, errors);
        if (!arg) break;
        astn_push(&n.fcall.args, arg);

        if (tokens.next != tokens.end &&
            tokens.next->type == eToken::comma)
            ++tokens.next;
    }

    if (!expect_and_advance(tokens, eToken::closed_parens, errors)) return NULL;

    io_tokens = tokens;
    return new ASTNode(n);
}

ASTNode* parse_block_item(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    // <block-item> ::= <statement> | <declaration>
    assert(io_tokens.next != io_tokens.end);
    TokenStream tokens = io_tokens;

    ASTNode* n = parse_statement(tokens, errors);
    if (n)
    {
        io_tokens = tokens;
        return n;
    }

    n = parse_declaration_with_semicolon(tokens, errors);
    if (n)
    {
        io_tokens = tokens;
        return n;
    }

    return NULL;
}

ASTNode* parse_declaration(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    // <declaration> ::= "int" <id> [ = <exp> ]
    assert(io_tokens.next != io_tokens.end);
    TokenStream tokens = io_tokens;

    if (tokens.next->type == eToken::keyword_int)
    {
        ASTNode n = {};
        n.type = AST_var;
        n.var.is_variable_declaration = true;

        ++tokens.next;
        if (tokens.next == tokens.end)
            return NULL;

        if (tokens.next->type != eToken::identifier)
        {
            append_error(errors, tokens.next, "expected identifier after variable type");
            return NULL;
        }

        n.var.name = tokens.next->identifier;
        ++tokens.next;

        if (tokens.next == tokens.end)
            return NULL;

        if (tokens.next->type == eToken::assignment)
        {
            n.var.is_variable_assignment = true;
            ++tokens.next;

            n.var.assign_expression = parse_expression(tokens, errors);
            if (!n.var.assign_expression)
            {
                append_error(errors, tokens.next, "expected expression after =");
                return NULL;
            }
        }

        io_tokens = tokens;
        return new ASTNode(n);
    }

    return NULL;
}

ASTNode* parse_declaration_with_semicolon(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    // <declaration_with_semicolon> ::= <declaration> ";"
    assert(io_tokens.next != io_tokens.end);
    TokenStream tokens = io_tokens;

    ASTNode* decl = parse_declaration(tokens, errors);
    if (!decl) return NULL;
    if (!expect_and_advance(tokens, eToken::semicolon, errors)) return NULL;

    io_tokens = tokens;
    return decl;
}

ASTNode* parse_statement(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    // <statement> ::= "return" <exp> ";"
    //               | <exp> ";"
    //               | "if" "(" <exp> ")" <statement> [ "else" <statement> ]
    //               | "{" { <block-item> } "}
    //               | "for" "(" <exp-option> ";" <exp-option> ";" <exp-option> ")" <statement>
    //               | "for" "(" <declaration> <exp-option> ";" <exp-option> ")" <statement>
    //               | "while" "(" <exp> ")" <statement>
    //               | "do" <statement> "while" <exp> ";"
    //               | "break" ";"
    //               | "continue" ";"
    //               | ";"
    assert(io_tokens.next != io_tokens.end);
    TokenStream& tokens = io_tokens;

    // return
    if (tokens.next->type == eToken::keyword_return)
    {
        ASTNode n = {};
        n.type = AST_ret;

        ++tokens.next;
        if (tokens.next == tokens.end)
            return NULL;

        n.ret.expression = parse_expression(tokens, errors);
        if (!n.ret.expression)
        {
            append_error(errors, tokens.next, "expected expression after return");
            return NULL;
        }

        if (!expect_and_advance(tokens, eToken::semicolon, errors)) return NULL;

        io_tokens = tokens;
        return new ASTNode(n);
    }

    // expression
    {
        ASTNode* expr = parse_expression(tokens, errors);
        if (expr && tokens.next->type == eToken::semicolon)
        {
            ++tokens.next;

            io_tokens = tokens;
            return expr;
        }
    }

    // if statement
    if (tokens.next->type == eToken::keyword_if)
    {
        ASTNode n = {};
        n.type = AST_if;

        ++tokens.next;

        // (
        if (!expect_and_advance(tokens, eToken::open_parens, errors)) return NULL;
        if (tokens.next == tokens.end) return NULL;

        n.ifdef.condition = parse_expression(tokens, errors);
        if (!n.ifdef.condition)
        {
            append_error(errors, tokens.next, "expected expression ater if");
            return NULL;
        }

        // )
        if (!expect_and_advance(tokens, eToken::closed_parens, errors)) return NULL;
        if (tokens.next == tokens.end) return NULL;

        n.ifdef.if_true = parse_statement(tokens, errors);
        if (!n.ifdef.if_true)
        {
            append_error(errors, tokens.next, "expected statement after if");
            return NULL;
        }

        if (tokens.next != tokens.end && tokens.next->type == eToken::keyword_else)
        {
            ++tokens.next;
            if (tokens.next == tokens.end) return NULL;

            n.ifdef.if_false = parse_statement(tokens, errors);
            if (!n.ifdef.if_false)
            {
                append_error(errors, tokens.next, "expected statement after else");
                return NULL;
            }
        }

        io_tokens = tokens;
        return new ASTNode(n);
    }

    // block list
    if (tokens.next->type == eToken::open_curly)
    {
        ASTNode n = {};
        n.type = AST_blocklist;
        ++tokens.next;

        while (tokens.next != tokens.end)
        {
            ASTNode* bi = parse_block_item(tokens, errors);
            if (!bi)
            {
                if (errors.size() != 0)
                    return NULL;
                break;
            }
            astn_push(&n.blocklist, bi);
        }

        if (!expect_and_advance(tokens, eToken::closed_curly, errors))
            return NULL;

        io_tokens = tokens;
        return new ASTNode(n);
    }

    // for
    if (tokens.next->type == eToken::keyword_for)
    {
        ASTNode* n = parse_for_loop(tokens, errors);
        if (!n)
            return NULL;
        io_tokens = tokens;
        return n;
    }

    // while
    if (tokens.next->type == eToken::keyword_while)
    {
        ASTNode* n = parse_while_loop(tokens, errors);
        if (!n)
            return NULL;
        io_tokens = tokens;
        return n;
    }

    // do-while
    if (tokens.next->type == eToken::keyword_do)
    {
        ASTNode* n = parse_do_while_loop(tokens, errors);
        if (!n)
            return NULL;
        io_tokens = tokens;
        return n;
    }

    // break;
    if (tokens.next->type == eToken::keyword_break)
    {
        ASTNode n = {};
        n.type = AST_break;
        ++tokens.next;

        if (tokens.next == tokens.end)
            return NULL;
        if (tokens.next->type != eToken::semicolon)
        {
            append_error(errors, tokens.next, "expected ; after break/continue");
            return NULL;
        }

        ++tokens.next;
        io_tokens = tokens;
        return new ASTNode(n);
    }

    // continue;
    if (tokens.next->type == eToken::keyword_continue)
    {
        ASTNode n = {};
        n.type = AST_continue;
        ++tokens.next;

        if (tokens.next == tokens.end)
            return NULL;
        if (tokens.next->type != eToken::semicolon)
        {
            append_error(errors, tokens.next, "expected ; after break/continue");
            return NULL;
        }

        ++tokens.next;
        io_tokens = tokens;
        return new ASTNode(n);
    }

    // empty statement
    if (tokens.next->type == eToken::semicolon)
    {
        ASTNode n = {};
        n.type = AST_empty;
        ++tokens.next;

        io_tokens = tokens;
        return new ASTNode(n);
    }

    return NULL;
}

ASTNode* parse_expression(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    // <exp> ::= <id> "=" <exp> | <conditional-exp>
    assert(io_tokens.next != io_tokens.end);
    TokenStream tokens = io_tokens;

    if (tokens.next + 2 < tokens.end
        && tokens.next[0].type == eToken::identifier
        && tokens.next[1].type == eToken::assignment)
    {
        ASTNode n = {};
        n.type = AST_var;
        n.var.is_variable_assignment = true;
        n.var.name = tokens.next->identifier;

        tokens.next += 2; //skip id and assignment

        n.var.assign_expression = parse_expression(tokens, errors);
        if (!n.var.assign_expression)
        {
            append_error(errors, tokens.next, "expected expression after =");
            return NULL;
        }

        io_tokens = tokens;
        return new ASTNode(n);
    }

    ASTNode* n = parse_conditional_exp(tokens, errors);
    if (n)
    {
        io_tokens = tokens;
        return n;
    }

    return NULL;
}

ASTNode* parse_conditional_exp(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    // <conditional-exp> ::= <logical-or-exp> [ "?" <exp> ":" <conditional-exp> ]
    assert(io_tokens.next != io_tokens.end);
    TokenStream tokens = io_tokens;

    ASTNode* expr = parse_logical_or_expression(tokens, errors);
    if (!expr)
        return NULL;

    if (tokens.next == tokens.end)
        return NULL;

    if (tokens.next->type != eToken::question_mark)
    {
        io_tokens = tokens;
        return expr;
    }

    ASTNode n = {};
    n.type = AST_terop;
    n.terop.condition = expr;

    if (!expect_and_advance(tokens, eToken::question_mark, errors)) return NULL;

    n.terop.if_true = parse_expression(tokens, errors);
    if (!n.terop.if_true)
    {
        append_error(errors, tokens.next, "expected expression after ?");
        return NULL;
    }

    if (!expect_and_advance(tokens, eToken::colon, errors)) return NULL;

    n.terop.if_false = parse_conditional_exp(tokens, errors);
    if (!n.terop.if_false)
    {
        append_error(errors, tokens.next, "expected conditional expression after :");
        return NULL;
    }

    io_tokens = tokens;
    return new ASTNode(n);
}

ASTNode* parse_logical_or_expression(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    // <logical-or-exp> ::= <logical-and-exp> { "||" <logical-and-exp> }
    assert(io_tokens.next != io_tokens.end);
    TokenStream tokens = io_tokens;

    ASTNode* left_node = parse_logical_and_expression(tokens, errors);
    if (!left_node)
        return NULL;

    if (tokens.next->type != eToken::logical_or)
    {
        io_tokens = tokens;
        return left_node;
    }

    ASTNode op = {};
    op.type = AST_binop;

    for (;;)
    {
        op.binop.op = tokens.next->type;

        ++tokens.next;
        if (tokens.next == tokens.end)
        {
            append_error(errors, tokens.next, "expected term after || but no more tokens");
            return NULL;
        }

        ASTNode* right_node = parse_logical_and_expression(tokens, errors);
        if (!right_node)
        {
            append_error(errors, tokens.next, "expected term after ||");
            return NULL;
        }

        op.binop.left = left_node;
        op.binop.right = right_node;

        // Gross, but it works. We will wrap around, set our binary_op to next token type and
        // use previous binop as term for next binop
        if (tokens.next != tokens.end && (tokens.next->type == eToken::logical_or))
        {
            left_node = new ASTNode(op);
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
    assert(io_tokens.next != io_tokens.end);
    TokenStream tokens = io_tokens;

    ASTNode* left_node = parse_equality_expression(tokens, errors);
    if (!left_node)
        return NULL;

    if (tokens.next->type != eToken::logical_and)
    {
        io_tokens = tokens;
        return left_node;
    }

    ASTNode op = {};
    op.type = AST_binop;

    for (;;)
    {
        op.binop.op = tokens.next->type;

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

        op.binop.left = left_node;
        op.binop.right = right_node;

        if (tokens.next != tokens.end &&
            (tokens.next->type == eToken::logical_and))
        {
            left_node = new ASTNode(op);
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
    assert(io_tokens.next != io_tokens.end);
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

    ASTNode op = {};
    op.type = AST_binop;

    for (;;)
    {
        op.binop.op = tokens.next->type;

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

        op.binop.left = left_node;
        op.binop.right = right_node;

        // Gross, but it works. We will wrap around, set our binary_op to next token type and
        // use previous binop as term for next binop
        if (tokens.next != tokens.end &&
            (tokens.next->type == eToken::logical_not_equal ||
                tokens.next->type == eToken::logical_equal))
        {
            left_node = new ASTNode(op);
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
    assert(io_tokens.next != io_tokens.end);
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

    ASTNode op = {};
    op.type = AST_binop;

    for (;;)
    {
        op.binop.op = tokens.next->type;

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

        op.binop.left = left_node;
        op.binop.right = right_node;

        // Gross, but it works. We will wrap around, set our binary_op to next token type and
        // use previous binop as term for next binop
        if (tokens.next != tokens.end &&
            (tokens.next->type == '<' ||
                tokens.next->type == '>' ||
                tokens.next->type == eToken::less_than_or_equal ||
                tokens.next->type == eToken::greater_than_or_equal))
        {
            left_node = new ASTNode(op);
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
    assert(io_tokens.next != io_tokens.end);
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

    ASTNode op = {};
    op.type = AST_binop;

    for (;;)
    {
        op.binop.op = tokens.next->type;

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

        op.binop.left = left_node;
        op.binop.right = right_node;

        // Gross, but it works. We will wrap around, set our binary_op to next token type and
        // use previous binop as term for next binop
        if (tokens.next != tokens.end && (tokens.next->type == '-' || tokens.next->type == '+'))
        {
            left_node = new ASTNode(op);
            continue;
        }

        break;
    }

    io_tokens = tokens;
    return new ASTNode(op);
}

ASTNode* parse_term(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    // <term> :: = <factor> { ("*" | "/" | "%") <factor> }
    assert(io_tokens.next != io_tokens.end);
    TokenStream tokens = io_tokens;

    ASTNode* left_node = parse_factor(tokens, errors);
    if (!left_node)
        return NULL;

    if (tokens.next->type != '*' &&
        tokens.next->type != '/' &&
        tokens.next->type != '%')
    {
        io_tokens = tokens;
        return left_node;
    }

    ASTNode op = {};
    op.type = AST_binop;

    for (;;)
    {
        op.binop.op = tokens.next->type;

        ++tokens.next;
        if (tokens.next == tokens.end)
        {
            append_error(errors, tokens.next, "expected factor after *, /, or % but no more tokens");
            return NULL;
        }

        ASTNode* right_node = parse_factor(tokens, errors);
        if (!right_node)
        {
            append_error(errors, tokens.next, "expected factor after *, /, or %");
            return NULL;
        }

        op.binop.left = left_node;
        op.binop.right = right_node;

        // Gross, but it works. We will wrap around, set our binary_op to next token type and
        // use previous binop as term for next binop
        if (tokens.next != tokens.end &&
            (tokens.next->type == '*' || tokens.next->type == '/' || tokens.next->type == '%'))
        {
            left_node = new ASTNode(op);
            continue;
        }

        break;
    }

    io_tokens = tokens;
    return new ASTNode(op);
}

ASTNode* parse_factor(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    // <factor> ::= <function-call> | "(" <exp> ")" | <unary_op> <factor> | <int> | <id>
    assert(io_tokens.next != io_tokens.end);
    TokenStream tokens = io_tokens;

    ASTNode* f = parse_function_call(tokens, errors);
    if (f)
    {
        io_tokens = tokens;
        return f;
    }


    if (tokens.next->type == eToken::open_parens)
    {
        ++tokens.next;
        if (tokens.next == tokens.end)
            return NULL;

        ASTNode* expression = parse_expression(tokens, errors);
        if (!expression)
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

    if (tokens.next->type == '!' ||
        tokens.next->type == '-' ||
        tokens.next->type == '~')
    {
        // unary operator, expects factor
        ASTNode n = {};
        n.type = AST_unop;
        n.unop.op = io_tokens.next->type;

        ++tokens.next;

        if (tokens.next == tokens.end)
            return NULL;

        ASTNode* factor = parse_factor(tokens, errors);
        if (!factor)
        {
            append_error(errors, tokens.next, "expected factor after unary operator");
            return NULL;
        }
        n.unop.on = factor;

        // "optimization" attempt to combine unary op with number
        if (factor->type == AST_num)
        {
            switch (n.unop.op)
            {
            case '!':
                factor->num.value = !factor->num.value;
                io_tokens = tokens;
                return factor;
            case '-':
                factor->num.value = -factor->num.value;
                io_tokens = tokens;
                return factor;
            case '~':
                factor->num.value = ~factor->num.value;
                io_tokens = tokens;
                return factor;
            }

            debug_break(); // not handling all unary ops, fall through to normal route
        }

        io_tokens = tokens;
        return new ASTNode(n);
    }

    if (tokens.next->type == eToken::constant_number)
    {
        ASTNode n = {};
        n.type = AST_num;
        n.num.value = tokens.next->number;
        ++tokens.next;

        io_tokens = tokens;
        return new ASTNode(n);
    }

    if (tokens.next->type == eToken::identifier)
    {
        ASTNode n = {};
        n.type = AST_var;
        n.var.is_variable_usage = true;
        n.var.name = tokens.next->identifier;
        ++tokens.next;

        io_tokens = tokens;
        return new ASTNode(n);
    }

    return NULL;
}

ASTNode* parse_for_loop(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    assert(io_tokens.next != io_tokens.end);
    if (io_tokens.next + 6 >= io_tokens.end) // 5 = "for(;;);" (min required tokens for for loop)
        return NULL;
    TokenStream tokens = io_tokens;

    if (tokens.next->type != eToken::keyword_for)
    {
        debug_break(); // this function expects that you've checked for 'for'
        return NULL;
    }
    ++tokens.next;

    ASTNode n = {};
    n.type = AST_for;

    if (!expect_and_advance(tokens, eToken::open_parens, errors))
        return NULL;

    // parse init
    if (tokens.next->type == eToken::semicolon)
    {
        ++tokens.next;
        assert(n.forloop.init == NULL);
    }
    else
    {
        n.forloop.init = parse_declaration_with_semicolon(tokens, errors);
        if (!n.forloop.init)
        {
            n.forloop.init = parse_expression(tokens, errors);
            if (!n.forloop.init)
            {
                append_error(errors, tokens.next, "failed parsing init section of for loop");
                return NULL;
            }

            if (!expect_and_advance(tokens, eToken::semicolon, errors))
                return NULL;
        }
        assert(n.forloop.init);
    }

    // parse condition
    if (tokens.next->type == eToken::semicolon)
    {
        ++tokens.next;
        assert(n.forloop.condition == NULL);
    }
    else
    {
        n.forloop.condition = parse_expression(tokens, errors);
        if (!n.forloop.condition)
        {
            append_error(errors, tokens.next, "failed parsing condition section of for loop");
            return NULL;
        }

        if (!expect_and_advance(tokens, eToken::semicolon, errors))
            return NULL;

        assert(n.forloop.condition);
    }

    // parse update
    if (tokens.next->type == eToken::closed_parens)
    {
        ++tokens.next;
        assert(n.forloop.update == NULL);
    }
    else
    {
        n.forloop.update = parse_expression(tokens, errors);
        if (!n.forloop.update)
        {
            append_error(errors, tokens.next, "failed parsing update section of for loop");
            return NULL;
        }

        if (!expect_and_advance(tokens, eToken::closed_parens, errors))
            return NULL;

        assert(n.forloop.update);
    }

    // for loop body
    n.forloop.body = parse_statement(tokens, errors);
    if (n.forloop.body)
    {
        io_tokens = tokens;
        return new ASTNode(n);
    }

    append_error(errors, tokens.next, "expected loop body after for loop");
    return NULL;
}

ASTNode* parse_while_loop(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    assert(io_tokens.next != io_tokens.end);
    if (io_tokens.next + 4 >= io_tokens.end) //X = while(...);
        return NULL;
    TokenStream tokens = io_tokens;

    if (tokens.next->type != eToken::keyword_while)
    {
        debug_break(); // this function expects that you've checked for 'while'
        return NULL;
    }
    ++tokens.next;

    ASTNode n = {};
    n.type = AST_while;

    if (!expect_and_advance(tokens, eToken::open_parens, errors))
        return NULL;

    n.whileloop.condition = parse_expression(tokens, errors);
    if(!n.whileloop.condition)
    {
        append_error(errors, tokens.next, "expected conditional expression inside while()");
        return NULL;
    }

    if (!expect_and_advance(tokens, eToken::closed_parens, errors))
        return NULL;

    n.whileloop.body = parse_statement(tokens, errors);
    if(!n.whileloop.body)
    {
        append_error(errors, tokens.next, "expected body after while(...)");
        return NULL;
    }

    io_tokens = tokens;
    return new ASTNode(n);
}

ASTNode* parse_do_while_loop(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    assert(io_tokens.next != io_tokens.end);
    if (io_tokens.next + 7 >= io_tokens.end) //X = do{...}while(...);
        return NULL;
    TokenStream tokens = io_tokens;

    if (tokens.next->type != eToken::keyword_do)
    {
        debug_break(); // this function expects that you've checked for 'do'
        return NULL;
    }
    ++tokens.next;

    ASTNode n = {};
    n.type = AST_dowhile;

    n.whileloop.body = parse_statement(tokens, errors);
    if(!n.whileloop.body)
    {
        append_error(errors, tokens.next, "expected loop body after do");
        return NULL;
    }

    if (!expect_and_advance(tokens, eToken::keyword_while, errors))
        return NULL;

    if (!expect_and_advance(tokens, eToken::open_parens, errors))
        return NULL;

    n.whileloop.condition = parse_expression(tokens, errors);
    if(!n.whileloop.condition)
    {
        append_error(errors, tokens.next, "expected condition inside while()");
        return NULL;
    }

    if (!expect_and_advance(tokens, eToken::closed_parens, errors))
        return NULL;

    if (!expect_and_advance(tokens, eToken::semicolon, errors))
        return NULL;

    io_tokens = tokens;
    return new ASTNode(n);
}

ASTNode* ast(TokenStream& io_tokens, std::vector<ASTError>& errors)
{
    return parse_program(io_tokens, errors);
}

void dump_ast(FILE* file, const ASTNode& self, int spaces_indent)
{
    if (self.type == AST_fdecl)
    {
        fprintf(file, "%*cFDECL %s\n", spaces_indent, ' ', self.fdecl.name.nts);
        for (uint32_t i = 0; i < self.fcall.args.size; ++i)
        {
            dump_ast(file, *self.fdecl.params.nodes[i], spaces_indent + 2);
        }
        fprintf(file, "%*c)\n", spaces_indent, ' ');
        return;
    }
    else if (self.type == AST_fcall)
    {
        fprintf(file, "%*cCALL %s(\n", spaces_indent, ' ', self.fcall.name.nts);
        for (uint32_t i = 0; i < self.fcall.args.size; ++i)
        {
            dump_ast(file, *self.fcall.args.nodes[i], spaces_indent + 2);
        }
        fprintf(file, "%*c)\n", spaces_indent, ' ');
        return;
    }
    else if (self.type == AST_fdef)
    {
        fprintf(file, "%*cFUNC %s %s(\n", spaces_indent, ' ', "INT", self.fdef.name.nts);
        for (uint32_t i = 0; i < self.fdef.params.size; ++i)
        {
            dump_ast(file, *self.fdef.params.nodes[i], spaces_indent + 2);
        }
        fprintf(file, "%*c)==[\n", spaces_indent, ' ');
        for (uint32_t i = 0; i < self.fdef.body.size; ++i)
        {
            dump_ast(file, *self.fdef.body.nodes[i], spaces_indent + 2);
        }
        fprintf(file, "%*c]==END FUNC %s\n", spaces_indent, ' ', self.fdef.name.nts);
        return;
    }
    else if (self.type == AST_ret)
    {
        fprintf(file, "%*cRETURN\n", spaces_indent, ' ');
        if(self.ret.expression) dump_ast(file, *self.ret.expression, spaces_indent + 2);
        return;
    }
    else if (self.type == AST_program)
    {
        fprintf(file, "%*cPROGRAM_START_BLOCK==[\n", spaces_indent, ' ');
        for (uint32_t i = 0; i < self.program.size; ++i)
        {
            dump_ast(file, *self.program.nodes[i], spaces_indent + 2);
        }
        fprintf(file, "%*c]==PROGRAM_END_BLOCK\n", spaces_indent, ' ');
        return;
    }
    else if (self.type == AST_blocklist)
    {
        fprintf(file, "%*cSTART_BLOCK==[\n", spaces_indent, ' ');
        for (uint32_t i = 0; i < self.blocklist.size; ++i)
        {
            dump_ast(file, *self.blocklist.nodes[i], spaces_indent + 2);
        }
        fprintf(file, "%*c]==END_BLOCK\n", spaces_indent, ' ');
        return;
    }
    else if (self.type == AST_if)
    {
        fprintf(file, "%*cIF\n", spaces_indent, ' ');
        dump_ast(file, *self.ifdef.condition, spaces_indent + 2);
        fprintf(file, "%*cTHEN\n", spaces_indent, ' ');
        dump_ast(file, *self.ifdef.if_true, spaces_indent + 2);
        if (self.ifdef.if_false)
        {
            fprintf(file, "%*cELSE\n", spaces_indent, ' ');
            dump_ast(file, *self.ifdef.if_false, spaces_indent + 2);
        }
        return;
    }
    else if (self.type == AST_for)
    {
        fprintf(file, "%*cFOR(\n", spaces_indent, ' ');
        dump_ast(file, *self.forloop.init, spaces_indent + 2);
        dump_ast(file, *self.forloop.condition, spaces_indent + 2);
        dump_ast(file, *self.forloop.update, spaces_indent + 2);
        fprintf(file, "%*c)\n", spaces_indent, ' ');
        dump_ast(file, *self.forloop.body, spaces_indent + 2);
        return;
    }
    else if (self.type == AST_while)
    {
        fprintf(file, "%*cWHILE(\n", spaces_indent, ' ');
        dump_ast(file, *self.whileloop.condition, spaces_indent + 2);
        dump_ast(file, *self.whileloop.body, spaces_indent + 2);
        fprintf(file, "%*c)\n", spaces_indent, ' ');
        return;
    }
    else if (self.type == AST_dowhile)
    {
        fprintf(file, "%*cDO(\n", spaces_indent, ' ');
        dump_ast(file, *self.whileloop.body, spaces_indent + 2);
        fprintf(file, "%*cWHILE\n", spaces_indent, ' ');
        dump_ast(file, *self.whileloop.condition, spaces_indent + 2);
        fprintf(file, "%*c)\n", spaces_indent, ' ');
        return;
    }
    else if (self.type == AST_num)
    {
        fprintf(file, "%*cInt<%" PRIi64 ">\n", spaces_indent, ' ', self.num.value);
        return;
    }
    else if (self.type == AST_unop)
    {
        fprintf(file, "%*cUnOp(%c,\n", spaces_indent, ' ', self.unop.op);
        dump_ast(file, *self.unop.on, spaces_indent + 2);
        fprintf(file, "%*c)\n", spaces_indent, ' ');
        return;
    }
    else if (self.type == AST_binop)
    {
        fprintf(file, "%*cBinOp(", spaces_indent, ' ');
        switch (self.binop.op)
        {
        case '%': fputc(self.binop.op, file); break;
        case '*': fputc(self.binop.op, file); break;
        case '+': fputc(self.binop.op, file); break;
        case '-': fputc(self.binop.op, file); break;
        case '/': fputc(self.binop.op, file); break;
        case '<': fputc(self.binop.op, file); break;
        case '>': fputc(self.binop.op, file); break;
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
        fprintf(file, "\n");
        dump_ast(file, *self.binop.left, spaces_indent + 2);
        dump_ast(file, *self.binop.right, spaces_indent + 2);
        fprintf(file, "%*c)\n", spaces_indent, ' ');
        return;
    }
    else if (self.type == AST_terop)
    {
        fprintf(file, "%*c?:(", spaces_indent, ' ');
        dump_ast(file, *self.terop.condition, spaces_indent + 2);
        dump_ast(file, *self.terop.if_true, spaces_indent + 2);
        dump_ast(file, *self.terop.if_false, spaces_indent + 2);
        fprintf(file, ")\n");
        return;
    }
    else if (self.type == AST_var)
    {
        if (self.var.is_variable_declaration && self.var.is_variable_assignment)
        {
            fprintf(file, "%*cVar<%s:%s>=\n", spaces_indent, ' ', "INT", self.var.name.nts);
            dump_ast(file, *self.var.assign_expression, spaces_indent + 2);
            return;
        }
        else if (self.var.is_variable_assignment)
        {
            fprintf(file, "%*cVar<%s>=\n", spaces_indent, ' ', self.var.name.nts);
            dump_ast(file, *self.var.assign_expression, spaces_indent + 2);
            return;
        }
        else if (self.var.is_variable_declaration)
        {
            assert(!self.var.assign_expression);
            fprintf(file, "%*cVar<%s:%s>\n", spaces_indent, ' ', "INT", self.var.name.nts);
            return;
        }
        else if (self.var.is_variable_usage)
        {
            assert(!self.var.assign_expression);
            fprintf(file, "%*cVar<%s>\n", spaces_indent, ' ', self.var.name.nts);
            return;
        }

        // unknown var_name usage
        debug_break();
        fprintf(file, "%*c???%s???\n", spaces_indent, ' ', self.var.name.nts);
        return;
    }
    else if (self.type == AST_break)
    {
        fprintf(file, "%*cBREAK;\n", spaces_indent, ' ');
        return;
    }
    else if (self.type == AST_continue)
    {
        fprintf(file, "%*cCONTINUE;\n", spaces_indent, ' ');
        return;
    }
    else if (self.type == AST_empty)
    {
        fprintf(file, "%*c;\n", spaces_indent, ' ');
        return;
    }

    // UNKNOWN VALUE
    debug_break();
    fprintf(file, "%*c?????\n", spaces_indent, ' ');
}

bool expect_and_advance(TokenStream& tokens, eToken expected_token, std::vector<ASTError>& errors)
{
    if (tokens.next == tokens.end)
    {
        append_error(errors, tokens.next, "out of tokens");
        return false;
    }

    if (tokens.next->type != expected_token)
    {
        switch (expected_token)
        {
        case '!': append_error(errors, tokens.next, "expected '!'"); break;
        case '%': append_error(errors, tokens.next, "expected '%'"); break;
        case '&': append_error(errors, tokens.next, "expected '&'"); break;
        case '(': append_error(errors, tokens.next, "expected '('"); break;
        case ')': append_error(errors, tokens.next, "expected ')'"); break;
        case '*': append_error(errors, tokens.next, "expected '*'"); break;
        case '+': append_error(errors, tokens.next, "expected '+'"); break;
        case ',': append_error(errors, tokens.next, "expected ','"); break;
        case '-': append_error(errors, tokens.next, "expected '-'"); break;
        case '/': append_error(errors, tokens.next, "expected '/'"); break;
        case ':': append_error(errors, tokens.next, "expected ':'"); break;
        case ';': append_error(errors, tokens.next, "expected ';'"); break;
        case '<': append_error(errors, tokens.next, "expected '<'"); break;
        case '=': append_error(errors, tokens.next, "expected '='"); break;
        case '>': append_error(errors, tokens.next, "expected '>'"); break;
        case '?': append_error(errors, tokens.next, "expected '?'"); break;
        case '{': append_error(errors, tokens.next, "expected '{'"); break;
        case '}': append_error(errors, tokens.next, "expected '}'"); break;
        case '~': append_error(errors, tokens.next, "expected '~'"); break;
        case eToken::logical_and:           append_error(errors, tokens.next, "expected '&&'"); break;
        case eToken::logical_or:            append_error(errors, tokens.next, "expected '||'"); break;
        case eToken::logical_equal:         append_error(errors, tokens.next, "expected '=='"); break;
        case eToken::logical_not_equal:     append_error(errors, tokens.next, "expected '!='"); break;
        case eToken::less_than_or_equal:    append_error(errors, tokens.next, "expected '<='"); break;
        case eToken::greater_than_or_equal: append_error(errors, tokens.next, "expected '>='"); break;
        case eToken::keyword_int:           append_error(errors, tokens.next, "expected 'int'"); break;
        case eToken::keyword_return:        append_error(errors, tokens.next, "expected 'return'"); break;
        case eToken::keyword_if:            append_error(errors, tokens.next, "expected 'if'"); break;
        case eToken::keyword_else:          append_error(errors, tokens.next, "expected 'else'"); break;
        case eToken::keyword_for:           append_error(errors, tokens.next, "expected 'for'"); break;
        case eToken::keyword_while:         append_error(errors, tokens.next, "expected 'while'"); break;
        case eToken::keyword_do:            append_error(errors, tokens.next, "expected 'do'"); break;
        case eToken::keyword_break:         append_error(errors, tokens.next, "expected 'break'"); break;
        case eToken::keyword_continue:      append_error(errors, tokens.next, "expected 'continue'"); break;
        default:
            debug_break();
            append_error(errors, tokens.next, "<UNKNOWN> token");
        }
        return false;
    }

    ++tokens.next;
    return true;
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
    fprintf(file, "\nBEGIN AST ERRORS=====\n");
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
    fprintf(file, "======END AST ERRORS\n");
}
