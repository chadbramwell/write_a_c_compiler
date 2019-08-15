#pragma once
#include "ast.h"

struct AsmInput
{
	const ASTNode* root;
};

bool gen_asm(FILE* file, const AsmInput& input);
