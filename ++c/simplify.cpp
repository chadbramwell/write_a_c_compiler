#include "simplify.h"
#include "debug.h"

ASTNode* simplify(const ASTNode* root, int* reductions)
{
    if (root->type == AST_unop && 
        root->unop.op == '-' && 
        root->unop.on->type == AST_num)
    {
        ASTNode* n = new ASTNode();
        n->type = AST_num;
        n->num.value = -root->unop.on->num.value;
        ++*reductions;
        return n;
    }

    if (root->type == AST_binop && 
        root->binop.op == '+' && 
        root->binop.left->type == AST_num && 
        root->binop.right->type == AST_num)
    {
        ASTNode* n = new ASTNode();
        n->type = AST_num;
        n->num.value = root->binop.left->num.value + root->binop.right->num.value;
        ++*reductions;
        return n;
    }

    debug_break(); // TODO, fill out switch below

    // deep copy while simplifying...
    ASTNode* n = new ASTNode(*root);
    switch (root->type)
    {
        case AST_program:
        {
            n->program.nodes = NULL;
            n->program.size = 0;
            for (uint32_t i = 0; i < root->program.size; ++i)
            {
                astn_push(&n->program, simplify(root->program.nodes[i], reductions));
            }
        } break;
        case AST_blocklist:
        case AST_ret: n->ret.expression = simplify(root->ret.expression, reductions); break;
        case AST_var:
        case AST_num:
        case AST_fdef:
        case AST_fcall:
        case AST_if:
        case AST_for:
        case AST_while:
        case AST_dowhile:
        case AST_unop:
        case AST_binop:
        case AST_terop:
        case AST_break:
        case AST_continue:
        case AST_empty:
        default:
            debug_break();
    }
    return n;
}

void dump_simplify(FILE* out, const ASTNode* root)
{
    if (root->type == AST_program)
    {
        for (uint32_t i = 0; i < root->program.size; ++i)
        {
            dump_simplify(out, root->program.nodes[i]);
        }
        fprintf(out, "\n");
        return;
    }
    else if (root->type == AST_fdef)
    {
        const char* func_return_type = "int";
        const char* func_name = root->fdef.name.nts;
        fprintf(out, "%s %s(){", func_return_type, func_name);
        for (uint32_t i = 0; i < root->fdef.body.size; ++i)
        {
            dump_simplify(out, root->fdef.body.nodes[i]);
        }
        fprintf(out, "}");
        return;
    }
    else if (root->type == AST_ret)
    {
        if (root->ret.expression)
        {
            fprintf(out, "return ");
            dump_simplify(out, root->ret.expression);
            fprintf(out, ";");
        }
        else
            fprintf(out, "return;");
        return;
    }
    else if (root->type == AST_num)
    {
        fprintf(out, "%" PRIi64, root->num.value);
        return;
    }

    debug_break();
}
