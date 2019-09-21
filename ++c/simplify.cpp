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
        {
            n->blocklist.nodes = NULL;
            n->blocklist.size = 0;
            for (uint32_t i = 0; i < root->blocklist.size; ++i)
            {
                astn_push(&n->blocklist, simplify(root->blocklist.nodes[i], reductions));
            }
        } break;
        case AST_ret: n->ret.expression = simplify(root->ret.expression, reductions); break;
        case AST_var:
        {
            if (root->var.assign_expression)
                n->var.assign_expression = simplify(root->var.assign_expression, reductions); break;
        } break;
        case AST_num: break;
        case AST_fdef:
        {
            n->fdef.body.nodes = NULL;
            n->fdef.body.size = 0;
            for (uint32_t i = 0; i < root->fdef.body.size; ++i)
            {
                astn_push(&n->fdef.body, simplify(root->fdef.body.nodes[i], reductions));
            }
        } break;
        case AST_fcall:debug_break();
        case AST_if:debug_break();
        case AST_for:debug_break();
        case AST_while:debug_break();
        case AST_dowhile:debug_break();
        case AST_unop:
        {
            n->unop.on = simplify(root->unop.on, reductions);
        } break;
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
