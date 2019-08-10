#pragma once
#include "ast.h"

struct AsmInput
{
	const ASTNode* p;
};

struct AsmOutput
{
	const ASTNode* main;
};

bool gen_asm(const AsmInput& input, AsmOutput& output);
void dump_asm(FILE* file, const AsmOutput& output);
