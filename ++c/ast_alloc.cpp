#include "ast_alloc.h"
#include "stdlib.h" // realloc

void astn_push(ASTNodeArray* a, struct ASTNode* n)
{
    a->size += 1;
    a->nodes = (ASTNode**)realloc(a->nodes, sizeof(ASTNode*) * a->size);
    a->nodes[a->size - 1] = n;
}

void astn_free(ASTNodeArray* a)
{
    free(a->nodes);
    a->nodes = NULL;
    a->size = 0;
}