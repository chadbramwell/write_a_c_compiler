#pragma once
#include "ast.h"

ASTNode* simplify(const ASTNode* root, int* reductions);
void dump_simplify(FILE* out, const ASTNode* root);