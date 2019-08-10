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
		if (input.p->children[i]->is_function)
		{
			output.main = input.p->children[i];
			break;
		}
	}

	return output.main != nullptr;
}

void dump_node(FILE* file, const ASTNode* n)
{
	if (n->is_number)
	{
		fprintf(file, "  mov $%" PRIu64 ", %%eax\n", n->number);
		return;
	}
	
	if (n->is_unary_op)
	{
		for (size_t i = 0; i < n->children.size(); ++i)
		{
			dump_node(file, n->children[i]);
		}

		switch (n->unary_op)
		{
		case '-': fprintf(file, "  neg %%eax\n"); return;
		case '~': fprintf(file, "  not %%eax\n"); return;
		}

		if (n->unary_op == '!')
		{
			fprintf(file, "  cmp $0, %%eax\n");	// set ZF on if exp == 0, set it off otherwise
			fprintf(file, "  mov $0, %%eax\n"); // zero out EAX (doesn't change FLAGS), xor %eax %eax is better because it sets a flag we can't use it because we depend on the ZF flag on the next line
			fprintf(file, "  sete %%al\n"); //set AL register (the lower byte of EAX) to 1 iff ZF is on
		}
	}
}

void dump_asm_return(FILE* file, const ASTNode* r)
{
	for (size_t i = 0; i < r->children.size(); ++i)
	{
		dump_node(file, r->children[i]);
	}

	fprintf(file, "  ret\n");
}

void dump_asm_func(FILE* file, const ASTNode* f)
{
	for (size_t i = 0; i < f->children.size(); ++i)
	{
		if (f->children[i]->is_return)
		{
			dump_asm_return(file, f->children[i]);
		}
	}
}

void dump_asm(FILE* file, const AsmOutput& output)
{
	fprintf(file, "  .globl %s\n", output.main->func_name.c_str());
	fprintf(file, "%s:\n", output.main->func_name.c_str());
	dump_asm_func(file, output.main);
}
