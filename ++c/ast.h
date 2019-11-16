#pragma once
#include "lex.h"
#include "ast_alloc.h"

// Below is an attempt at a simple AST generator. It's meant to be simple.
// note: ASTNodes are dynamically allocated and never freed.

enum ASTType
{
    AST_UNKNOWN,
    AST_program,
    AST_blocklist,
    AST_ret,
    AST_var,
    AST_num,
    AST_fdecl,
    AST_fdef,
    AST_fcall,
    AST_if,
    AST_for,
    AST_while,
    AST_dowhile,
    AST_unop,
    AST_binop,
    AST_terop,
    AST_break,
    AST_continue,
    AST_empty
};

struct ASTNode
{
    ASTType type;
    union
    {
        ASTNodeArray program;
        ASTNodeArray blocklist;

        struct {
            ASTNode* expression;
        } ret;

        struct {
            bool is_variable_declaration;
            bool is_variable_assignment;
            bool is_variable_usage;
            str name;
            ASTNode* assign_expression;
            ASTNode* var_decl; // Which node the var was declared with. A var decl points to itself. Helpful info for gen phase.
            const Token* debug_token;
        } var;

        struct {
            str name;
            ASTNodeArray params;
        } fdecl;

        struct {
            str name;
            eToken return_type;
            ASTNodeArray params;
            ASTNodeArray body;
        } fdef;

        struct {
            str name;
            ASTNodeArray args;
        } fcall;

        struct {
            ASTNode* condition;
            ASTNode* if_true;
            ASTNode* if_false;
        } ifdef;

        struct {
            ASTNode* init;
            ASTNode* condition;
            ASTNode* update;
            ASTNode* body;
        } forloop;

        struct {
            ASTNode* condition;
            ASTNode* body;
        } whileloop;

        struct {
            eToken op;
            ASTNode* on;
        } unop;

        struct {
            eToken op;
            ASTNode* left;
            ASTNode* right;
        } binop;

        struct {
            ASTNode* condition;
            ASTNode* if_true;
            ASTNode* if_false;
        } terop;

        struct {
            int64_t value;
        } num;
    }; // end union
}; // end struct ASTNode

struct ASTOut
{
    bool failure;
    ASTNode* root;
};

bool ast(const Token* tokens, uint64_t num_tokens, ASTOut* out); // returns true on success
void dump_ast(FILE* file, const ASTNode* root, int spaces_indent);
