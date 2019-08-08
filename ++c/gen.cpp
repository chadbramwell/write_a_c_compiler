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

void dump_node(FILE* file, const ASTNode* n)
{
	if (auto c = dynamic_cast<const AST_ConstantNumber*>(n))
	{
		fprintf(file, "  mov $%" PRIu64 ", %%eax\n", c->number);
		return;
	}
	
	if (auto u = dynamic_cast<const AST_UnaryOperation*>(n))
	{
		for (size_t i = 0; i < n->children.size(); ++i)
		{
			dump_node(file, n->children[i].get());
		}

		switch (u->uop)
		{
		case '-': fprintf(file, "  neg %%eax\n"); return;
		case '~': fprintf(file, "  not %%eax\n"); return;
		}

		if (u->uop == '!')
		{
			fprintf(file, "  cmp $0, %%eax\n");	// set ZF on if exp == 0, set it off otherwise
			fprintf(file, "  mov $0, %%eax\n"); // zero out EAX (doesn't change FLAGS), xor %eax %eax is better because it sets a flag we can't use it because we depend on the ZF flag on the next line
			fprintf(file, "  sete %%al\n"); //set AL register (the lower byte of EAX) to 1 iff ZF is on
		}
	}
}

void dump_asm_return(FILE* file, const AST_ReturnStatement* r)
{
	for (size_t i = 0; i < r->children.size(); ++i)
	{
		dump_node(file, r->children[i].get());
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
