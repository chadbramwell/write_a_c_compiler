#include "gen.h"
#include "debug.h"

struct gen_ctx
{
	FILE* out;
	uint64_t label_index;
};

bool gen_asm_node(gen_ctx* ctx, const ASTNode* n)
{
	if (n->is_function)
	{
		for (size_t i = 0; i < n->children.size(); ++i)
		{
			if (!gen_asm_node(ctx, n->children[i]))
				return false;
		}
		return true;
	}

	if (n->is_return)
	{
		for (size_t i = 0; i < n->children.size(); ++i)
		{
			if (!gen_asm_node(ctx, n->children[i]))
				return false;
		}
		fprintf(ctx->out, "  ret\n");
		return true;
	}

	if (n->is_number)
	{
		fprintf(ctx->out, "  mov $%" PRIi64 ", %%rax\n", n->number);
		return true;
	}
	
	if (n->is_unary_op)
	{
		for (size_t i = 0; i < n->children.size(); ++i)
		{
			if (!gen_asm_node(ctx, n->children[i]))
				return false;
		}

		switch (n->op)
		{
		case '-': fprintf(ctx->out, "  neg %%rax\n"); return true;
		case '~': fprintf(ctx->out, "  not %%rax\n"); return true;
		}

		if (n->op == '!')
		{
			fprintf(ctx->out, "  cmp $0, %%rax\n");	// set ZF on if exp == 0, set it off otherwise
			fprintf(ctx->out, "  mov $0, %%rax\n"); // zero out EAX (doesn't change FLAGS), xor %eax %eax is better because it sets a flag we can't use it because we depend on the ZF flag on the next line
			fprintf(ctx->out, "  sete %%al\n"); //set AL register (the lower byte of EAX) to 1 iff ZF is on
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
			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  push %%rax\n");
			gen_asm_node(ctx, n->children[1]);
			fprintf(ctx->out, "  pop %%rcx\n");
			fprintf(ctx->out, "  add %%rcx, %%rax\n");
		} break;
		case eToken::dash:
		{
			gen_asm_node(ctx, n->children[1]); // note swapped children. sub src, dst => dst - src => stored in dst
			fprintf(ctx->out, "  push %%rax\n");
			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  pop %%rcx\n");
			fprintf(ctx->out, "  sub %%rcx, %%rax\n");
		} break;
		case eToken::star:
		{
			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  push %%rax\n");
			gen_asm_node(ctx, n->children[1]);
			fprintf(ctx->out, "  pop %%rcx\n");
			fprintf(ctx->out, "  imul %%rcx, %%rax\n");
		} break;
		case eToken::forward_slash:
		{
			gen_asm_node(ctx, n->children[1]); // note swapped children.
			fprintf(ctx->out, "  push %%rax\n");
			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  pop %%rcx\n");
			fprintf(ctx->out, "  xor %%rdx, %%rdx\n"); //note dividend is combo of EDX:EAX. If we don't 0 out EDX we could get an integer overflow exception because RAX won't be big enough to store the result of the DIV
			fprintf(ctx->out, "  idiv %%rcx\n"); // output stored in rax
		} break;
		case '<':
		{
			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  push %%rax\n");
			gen_asm_node(ctx, n->children[1]);
			fprintf(ctx->out, "  pop %%rcx\n");
			fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
			fprintf(ctx->out, "  setl %%al\n");
		} break;
		case '>':
		{
			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  push %%rax\n");
			gen_asm_node(ctx, n->children[1]);
			fprintf(ctx->out, "  pop %%rcx\n");
			fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
			fprintf(ctx->out, "  setg %%al\n");
		} break;
		case eToken::logical_and:
		{
			uint64_t label_index_rightside = ++ctx->label_index;
			uint64_t label_index_end = ++ctx->label_index;

			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  cmp $0, %%rax\n");
			fprintf(ctx->out, "  jne check_right_of_and_%" PRIu64 "\n", label_index_rightside);
			fprintf(ctx->out, "  jmp end_and_%" PRIu64 "\n", label_index_end);
			fprintf(ctx->out, "check_right_of_and_%" PRIu64 ":\n", label_index_rightside);
			gen_asm_node(ctx, n->children[1]);
			fprintf(ctx->out, "  cmp $0, %%rax\n");
			fprintf(ctx->out, "  mov $0, %%rax\n");
			fprintf(ctx->out, "  setne %%al\n");
			fprintf(ctx->out, "end_and_%" PRIu64 ":\n", label_index_end);
		} break;
		case eToken::logical_or:
		{
			uint64_t label_index_rightside = ++ctx->label_index;
			uint64_t label_index_end = ++ctx->label_index;

			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  cmp $0, %%rax\n");
			fprintf(ctx->out, "  je check_right_of_or_%" PRIu64 "\n", label_index_rightside);
			fprintf(ctx->out, "  mov $1, %%rax\n");
			fprintf(ctx->out, "  jmp end_or_%" PRIu64 "\n", label_index_end);
			fprintf(ctx->out, "check_right_of_or_%" PRIu64 ":\n", label_index_rightside);
			gen_asm_node(ctx, n->children[1]);
			fprintf(ctx->out, "  cmp $0, %%rax\n");
			fprintf(ctx->out, "  mov $0, %%rax\n");
			fprintf(ctx->out, "  setne %%al\n");
			fprintf(ctx->out, "end_or_%" PRIu64 ":\n", label_index_end);
		} break;
		case eToken::logical_equal:
		{
			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  push %%rax\n");
			gen_asm_node(ctx, n->children[1]);
			fprintf(ctx->out, "  pop %%rcx\n");
			fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
			fprintf(ctx->out, "  mov $0, %%rax\n");
			fprintf(ctx->out, "  sete %%al\n");
		} break;
		case eToken::logical_not_equal:
		{
			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  push %%rax\n");
			gen_asm_node(ctx, n->children[1]);
			fprintf(ctx->out, "  pop %%rcx\n");
			fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
			fprintf(ctx->out, "  mov $0, %%rax\n");
			fprintf(ctx->out, "  setne %%al\n");
		} break;
		case eToken::less_than_or_equal:
		{
			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  push %%rax\n");
			gen_asm_node(ctx, n->children[1]);
			fprintf(ctx->out, "  pop %%rcx\n");
			fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
			fprintf(ctx->out, "  mov $0, %%rax\n");
			fprintf(ctx->out, "  setle %%al\n");
		} break;
		case eToken::greater_than_or_equal:
		{
			gen_asm_node(ctx, n->children[0]);
			fprintf(ctx->out, "  push %%rax\n");
			gen_asm_node(ctx, n->children[1]);
			fprintf(ctx->out, "  pop %%rcx\n");
			fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
			fprintf(ctx->out, "  mov $0, %%rax\n");
			fprintf(ctx->out, "  setge %%al\n");
		} break;
		default:
			debug_break();
			return false;
		}
		
		return true;
	}

	debug_break();
	return false;
}

bool gen_asm(FILE* file, const AsmInput& input)
{
	gen_ctx ctx;
	ctx.out = file;
	ctx.label_index = 0;

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

	fprintf(ctx.out, "  .globl %s\n", main->func_name.nts);
	fprintf(ctx.out, "%s:\n", main->func_name.nts);
	//fprintf(file, "  int $3\n"); // debug break, makes it easier to start step-by-step debugging with visual studio
	return gen_asm_node(&ctx, main);
}
