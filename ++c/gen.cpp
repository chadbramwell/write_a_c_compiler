#include "gen.h"
#include "debug.h"

bool gen_asm_node(FILE* file, const ASTNode* n)
{
	if (n->is_number)
	{
		fprintf(file, "  mov $%" PRIu64 ", %%rax\n", n->number);
		return true;
	}
	
	if (n->is_unary_op)
	{
		for (size_t i = 0; i < n->children.size(); ++i)
		{
			if (!gen_asm_node(file, n->children[i]))
				return false;
		}

		switch (n->op)
		{
		case '-': fprintf(file, "  neg %%rax\n"); return true;
		case '~': fprintf(file, "  not %%rax\n"); return true;
		}

		if (n->op == '!')
		{
			fprintf(file, "  cmp $0, %%rax\n");	// set ZF on if exp == 0, set it off otherwise
			fprintf(file, "  mov $0, %%rax\n"); // zero out EAX (doesn't change FLAGS), xor %eax %eax is better because it sets a flag we can't use it because we depend on the ZF flag on the next line
			fprintf(file, "  sete %%al\n"); //set AL register (the lower byte of EAX) to 1 iff ZF is on
			return true;
		}
	}

	if (n->is_binary_op)
	{
		assert(n->children.size() == 2);
		switch (n->op)
		{
		case eToken::plus:
		{
			gen_asm_node(file, n->children[0]);
			fprintf(file, "  push %%rax\n");
			gen_asm_node(file, n->children[1]);
			fprintf(file, "  pop %%rcx\n");
			fprintf(file, "  add %%rcx, %%rax\n");
		} break;
		case eToken::dash:
		{
			gen_asm_node(file, n->children[1]); // note swapped children. sub src, dst => dst - src => stored in dst
			fprintf(file, "  push %%rax\n");
			gen_asm_node(file, n->children[0]);
			fprintf(file, "  pop %%rcx\n");
			fprintf(file, "  sub %%rcx, %%rax\n");
		} break;
		case eToken::star:
		{
			gen_asm_node(file, n->children[0]);
			fprintf(file, "  push %%rax\n");
			gen_asm_node(file, n->children[1]);
			fprintf(file, "  pop %%rcx\n");
			fprintf(file, "  imul %%rcx, %%rax\n");
		} break;
		case eToken::forward_slash:
		{
			gen_asm_node(file, n->children[1]); // note swapped children.
			fprintf(file, "  push %%rax\n");
			gen_asm_node(file, n->children[0]);
			fprintf(file, "  pop %%rcx\n");
			fprintf(file, "  xor %%rdx, %%rdx\n"); //note dividend is combo of EDX:EAX. If we don't 0 out EDX we could get an integer overflow exception because RAX won't be big enough to store the result of the DIV
			fprintf(file, "  idiv %%rcx\n"); // output stored in rax
		} break;
		default:
			debug_break();
			return false;
		}
		
		return true;
	}

	return false;
}

bool gen_asm_return(FILE* file, const ASTNode* r)
{
	for (size_t i = 0; i < r->children.size(); ++i)
	{
		if (!gen_asm_node(file, r->children[i]))
			return false;
	}
	fprintf(file, "  ret\n");
	return true;
}

bool gen_asm_func(FILE* file, const ASTNode* f)
{
	for (size_t i = 0; i < f->children.size(); ++i)
	{
		if (f->children[i]->is_return)
		{
			if (!gen_asm_return(file, f->children[i]))
				return false;
		}
	}
	return true;
}

bool gen_asm(FILE* file, const AsmInput& input)
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
	ASTNode* main = NULL;

	for (size_t i = 0; i < input.root->children.size(); ++i)
	{
		if (input.root->children[i]->is_function)
		{
			main = input.root->children[i];
			break;
		}
	}

	if (!main)
		return false;

	fprintf(file, "  .globl %s\n", main->func_name.c_str());
	fprintf(file, "%s:\n", main->func_name.c_str());
	//fprintf(file, "  int $3\n"); // debug break, makes it easier to start step-by-step debugging with visual studio
	return gen_asm_func(file, main);
}
