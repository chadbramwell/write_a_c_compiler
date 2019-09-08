#include "gen.h"
#include "debug.h"


struct gen_ctx
{
	FILE* out;
	uint64_t label_index; // every label needs to be unique, this is appended to every label to ensure that's the case.

	// stack data
	str stack_frames[256][256];
	uint8_t sv_frame_len[256];
	uint8_t sv_num_frames;
};

bool push_stack_frame(gen_ctx* ctx)
{
	if (ctx->sv_num_frames == 256)
	{
		debug_break(); // error! out of memory!
		return false;
	}

	ctx->sv_frame_len[ctx->sv_num_frames] = 0;
	++ctx->sv_num_frames;
	return true;
}

bool pop_stack_frame(gen_ctx* ctx)
{
	if (ctx->sv_num_frames == 0)
	{
		debug_break(); // error! too many pops!
		return false;
	}

	--ctx->sv_num_frames;
	return true;
}

bool push_vars(gen_ctx* ctx, const str* const vars, uint8_t num_vars)
{
	if (ctx->sv_num_frames == 0)
	{
		debug_break(); // error! push a stack frame first!
		return false;
	}

	uint8_t sf_index = ctx->sv_num_frames - 1;

	if (ctx->sv_frame_len[sf_index] >= 256 - num_vars)
	{
		debug_break(); // error! no more room for var on stack!
		return false;
	}

	str* sf_iter = ctx->stack_frames[ctx->sv_num_frames - 1];
	str* sf_end = sf_iter + ctx->sv_frame_len[ctx->sv_num_frames - 1];
	const str* vars_iter = vars;
	const str* vars_end = vars + num_vars;

	// make sure each var is not already on stack
	for(; sf_iter != sf_end; ++sf_iter)
	{
		for(vars_iter = vars; vars_iter != vars_end; ++vars_iter)
		{
			if (sf_iter->nts == vars_iter->nts)
			{
				return false;
			}
		}
	}
	
	memcpy(sf_end, vars, sizeof(vars[0]) * num_vars);
	ctx->sv_frame_len[sf_index] += num_vars;
	return true;
}

uint64_t get_stack_offset(gen_ctx* ctx, const str& s)
{
	for (int64_t i = ctx->sv_num_frames - 1; i >= 0; --i)
	{
		const str* const sf_start = ctx->stack_frames[i];
		const str* sf_iter = sf_start;
		const str* const sf_end = sf_iter + ctx->sv_frame_len[i];

		while (sf_iter != sf_end)
		{
			if (sf_iter->nts == s.nts)
			{
				int64_t offset = (1 + sf_iter - sf_start) * 8;
				--i;
				while (i >= 0)
				{
					offset += ctx->sv_frame_len[i] * 8;
					--i;
				}

				return offset;
			}
			++sf_iter;
		}
	}

	debug_break();
	return 0;
}

bool gen_asm_node(gen_ctx* ctx, const ASTNode* n)
{
	if (n->is_function)
	{
		bool ok = push_stack_frame(ctx);
		if (!ok)
		{
			debug_break();
			return false;
		}

		// function prologue
		fprintf(ctx->out, "  push %%rbp\n"); // save old value of EBP
		fprintf(ctx->out, "  mov %%rsp, %%rbp\n"); // current top of stack is bottom of new stack frame

		for (size_t i = 0; i < n->children.size(); ++i)
		{
			if (!gen_asm_node(ctx, n->children[i]))
				return false;
		}
		
		// Handle main() that does not have a return statement.
		if (n->children.size() == 0 || !n->children.back()->is_return)
		{
			// According to the C11 Standard, if main doesn't have a return statement than it should return 0.
			//  If a function other than main does not have a return statement than it is undefined behavior.
			//  Thus the assert. TODO: add error handling and let user know about undefined behavior.
			assert(n->func_name.nts == strings_insert_nts("main").nts);
			fprintf(ctx->out, "  mov $0, %%rax\n");

			// function epilogue
			fprintf(ctx->out, "  mov %%rbp, %%rsp\n"); // restore ESP; now it points to old EBP
			fprintf(ctx->out, "  pop %%rbp\n"); // restore old EBP; now ESP is where it was before prologue
			fprintf(ctx->out, "  ret\n");
		}

		ok = pop_stack_frame(ctx);
		assert(ok);
		return ok;
	}

	if (n->is_block_list)
	{
		bool ok = push_stack_frame(ctx);
		if (!ok)
		{
			debug_break();
			return false;
		}

		for (size_t i = 0; i < n->children.size(); ++i)
		{
			if (!gen_asm_node(ctx, n->children[i]))
				return false;
		}

		ok = pop_stack_frame(ctx);
		assert(ok);
		return ok;
	}

	if (n->is_return)
	{
		for (size_t i = 0; i < n->children.size(); ++i)
		{
			if (!gen_asm_node(ctx, n->children[i]))
				return false;
		}

		// function epilogue
		fprintf(ctx->out, "  mov %%rbp, %%rsp\n"); // restore ESP; now it points to old EBP
		fprintf(ctx->out, "  pop %%rbp\n"); // restore old EBP; now ESP is where it was before prologue
		fprintf(ctx->out, "  ret\n");

		return true;
	}

	if (n->var_name.nts)
	{
		if (n->is_variable_declaration)
		{
			bool ok = push_vars(ctx, &n->var_name, 1);
			if (!ok)
			{
				debug_break();
				return false;
			}
			fprintf(ctx->out, "  sub $8, %%rsp\n");
		}

		if (n->is_variable_assignment)
		{
			assert(n->children.size() == 1);

			// special case, assigning a literal
			// THIS DOESN'T WORK!!!
			// EXAMPLE WHY THIS WONT WORK: (a=1)|| ... // the gist is that assigning a value also has a value itself. 
			// THIS MIGHT STILL WORK IF: we are not part of an expression. Best test for that? Perhaps if our parent is a function or block list?
			//if (n->children[0]->is_number)
			//{
			//	uint64_t stack_offset = get_stack_offset(ctx, n->var_name);
			//	if (!stack_offset)
			//		return false;
			//	fprintf(ctx->out, "  movq $%" PRIi64 ", -%" PRIu64 "(%%rbp)\n", n->children[0]->number, stack_offset);
			//	return true;
			//}

			if (!gen_asm_node(ctx, n->children[0]))
				return false;
			uint64_t stack_offset = get_stack_offset(ctx, n->var_name);
			if (!stack_offset)
				return false;
			fprintf(ctx->out, "  mov %%rax, -%" PRIu64 "(%%rbp)\n", stack_offset);
			return true;
		}

		if (n->is_variable_usage)
		{
			assert(n->children.size() == 0);
			fprintf(ctx->out, "  mov -%" PRIu64 "(%%rbp), %%rax\n", get_stack_offset(ctx, n->var_name));
			return true;
		}
		
		// I guess it's just a decl this time...
		return true;
	}

	if (n->is_if)
	{
		assert(n->children.size() > 1);
		bool has_else = n->children.size() > 2;
		if (has_else) assert(n->children.size() == 3);

		char label_else[32];
		sprintf_s(label_else, "else_%" PRIu64, ctx->label_index++);
		char label_end[32];
		sprintf_s(label_end, "fi_%" PRIu64, ctx->label_index++);

		if (!gen_asm_node(ctx, n->children[0])) // if conditional expression
			return false;
		fprintf(ctx->out, "  cmp $0, %%rax\n");
		if (!has_else)
		{
			fprintf(ctx->out, "  je %s\n", label_end);
			if (!gen_asm_node(ctx, n->children[1])) // if statement
				return false;
		}
		else
		{
			fprintf(ctx->out, "  je %s\n", label_else);
			if (!gen_asm_node(ctx, n->children[1])) // if statement
				return false;
			fprintf(ctx->out, "  jmp %s\n", label_end);

			fprintf(ctx->out, "%s:\n", label_else);
			if (!gen_asm_node(ctx, n->children[2])) // else statement
				return false;
		}
		fprintf(ctx->out, "%s:\n", label_end);
		return true;
	}

	if (n->is_ternery_op)
	{
		assert(n->children.size() == 3);

		char label_else[32];
		sprintf_s(label_else, "ter_false_%" PRIu64, ctx->label_index++);
		char label_end[32];
		sprintf_s(label_end, "ter_end_%" PRIu64, ctx->label_index++);

		if (!gen_asm_node(ctx, n->children[0]))
			return false;
		fprintf(ctx->out, "  cmp $0, %%rax\n");
		fprintf(ctx->out, "  je %s\n", label_else);
		if (!gen_asm_node(ctx, n->children[1]))
			return false;
		fprintf(ctx->out, "  jmp %s\n", label_end);
		fprintf(ctx->out, "%s:\n", label_else);
		if (!gen_asm_node(ctx, n->children[2]))
			return false;
		fprintf(ctx->out, "%s:\n", label_end);
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
		case '!':
			fprintf(ctx->out, "  cmp $0, %%rax\n");	// set ZF on if exp == 0, set it off otherwise
			fprintf(ctx->out, "  mov $0, %%rax\n"); // zero out EAX (doesn't change FLAGS), xor %eax %eax is better because it sets a flag we can't use it because we depend on the ZF flag on the next line
			fprintf(ctx->out, "  sete %%al\n"); //set AL register (the lower byte of EAX) to 1 iff ZF is on
			return true;
		}

		debug_break();
		return false;
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
	ctx.sv_num_frames = 0;

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
