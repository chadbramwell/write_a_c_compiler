#include "gen.h"

bool gen_asm(const AsmInput& input, AsmOutput& output)
{
/*
	.text
	.def	 main;
	.scl	2;
	.type	32;
	.endef
	.globl	main                    # -- Begin function main
	.p2align	4, 0x90
main:                                   # @main
# %bb.0:
	movl	$2, %eax
	retq
										# -- End function

*/
	output.main = nullptr;

	for (size_t i = 0; i < input.p->children.size(); ++i)
	{
		if (auto f = dynamic_cast<const AST_Function*>(input.p->children[i].get()))
		{
			output.main = f;
		}
	}

	return output.main != nullptr;
}

void dump_asm_return(FILE* file, const AST_ReturnStatement* r)
{
	for (size_t i = 0; i < r->children.size(); ++i)
	{
		if (auto n = dynamic_cast<const AST_Number*>(r->children[i].get()))
		{
			fprintf(file, "  mov $%" PRIu64 ", %%eax\n", n->value);
		}
	}

	fprintf(file, "  ret\n");
}

void dump_asm_func(FILE* file, const AST_Function* f)
{
	for (size_t i = 0; i < f->children.size(); ++i)
	{
		if (auto r = dynamic_cast<const AST_ReturnStatement*>(f->children[i].get()))
		{
			dump_asm_return(file, r);
		}
	}
}

void dump_asm(FILE* file, const AsmOutput& output)
{
	fprintf(file, "  .globl %s\n", output.main->name.c_str());
	fprintf(file, "%s:\n", output.main->name.c_str());
	dump_asm_func(file, output.main);
}
