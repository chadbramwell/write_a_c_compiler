#pragma once
#include "stdint.h"

struct ASTNodeArray
{
    uint32_t size;
    struct ASTNode** nodes;
};

void astn_push(ASTNodeArray* a, struct ASTNode* n);
