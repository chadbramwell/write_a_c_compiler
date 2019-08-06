#pragma once
#include "ast.h"

struct AsmInput
{
	const AST_Program* p;
};

struct AsmOutput
{
	const AST_Function* main;
};

bool gen_asm(const AsmInput& input, AsmOutput& output);
void dump_asm(FILE* file, const AsmOutput& output);
