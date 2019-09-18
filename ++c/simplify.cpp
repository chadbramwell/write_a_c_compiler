#include "simplify.h"
#include "debug.h"


ASTNode* simplify(const ASTNode* root, int* reductions)
{
    if (root->is_unary_op && 
        root->op == '-' && 
        root->children[0]->type == AST_num)
    {
        ASTNode* n = new ASTNode();
        n->type = AST_num;
        n->number = -root->children[0]->number;
        ++*reductions;
        return n;
    }

    if (root->is_binary_op && 
        root->op == '+' && 
        root->children[0]->type == AST_num && 
        root->children[1]->type == AST_num)
    {
        ASTNode* n = new ASTNode();
        n->type = AST_num;
        n->number = root->children[0]->number + root->children[1]->number;
        ++*reductions;
        return n;
    }

    ASTNode* n = new ASTNode(*root);
    if (root->type == AST_ret)
    {
        assert(root->children.size() == 0);
        n->ret.expression = simplify(root->ret.expression, reductions);
    }
    else
    {
        n->children.clear();

        for (size_t i = 0; i < root->children.size(); ++i)
        {
            n->children.push_back(simplify(root->children[i], reductions));
        }
    }
    return n;
}

void dump_simplify(FILE* out, const ASTNode* root)
{
    if (root->is_program)
    {
        for (size_t i = 0; i < root->children.size(); ++i)
        {
            dump_simplify(out, root->children[i]);
        }
        fprintf(out, "\n");
        return;
    }
    else if (root->is_function)
    {
        const char* func_return_type = "int";
        const char* func_name = root->func_name.nts;
        fprintf(out, "%s %s(){", func_return_type, func_name);
        for (size_t i = 0; i < root->children.size(); ++i)
        {
            dump_simplify(out, root->children[i]);
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
        fprintf(out, "%" PRIi64, root->number);
        return;
    }

    debug_break();
}
