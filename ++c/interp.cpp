#include "interp.h"
#include "debug.h"
#include "strings.h"

#define RETURN_INTERP_FAILURE do{ debug_break(); return false; } while(0)

struct stack_var
{
	// NOTE assumption here: all ids exist in the same string pool (see "strings.h") and thus we can
	// simply store the pointer and compare on the pointer for equality.
	const char* id;
	int value;
};

struct interp_context
{
	stack_var stack[256]; // uses sentinal values for stack frames
	int stack_top;

	int loop_depth;
	bool return_triggered;
	bool break_triggered;
	bool continue_triggered;
};

bool push_frame(interp_context* ctx)
{
	if (ctx->stack_top == 256)
	{
		debug_break(); // no room
		return false;
	}

	ctx->stack[ctx->stack_top++].id = NULL;
	return true;
}

bool pop_frame(interp_context* ctx)
{
	stack_var* iter = ctx->stack + ctx->stack_top - 1;
	while (iter >= ctx->stack && iter->id != NULL)
		--iter;

	if (iter < ctx->stack || iter->id != NULL)
	{
		debug_break();
		return false; // need to push_frame before pop_frame
	}
	ctx->stack_top = int(iter - ctx->stack);
	return true;
}

bool push_var(interp_context* ctx, const char* id)
{
	if (ctx->stack_top >= 256)
	{
		debug_break();
		return NULL; // no room
	}

	// ensure valid frame as been pushed
	{
		stack_var* iter = ctx->stack + ctx->stack_top - 1;
		while (iter >= ctx->stack && iter->id != NULL)
			--iter;

		if (iter < ctx->stack || iter->id != NULL)
		{
			debug_break(); // need to push a stack frame first!
			return false;
		}
	}

	stack_var* sv = ctx->stack + ctx->stack_top;
	sv->id = id;
	++ctx->stack_top;
	return true;
}

bool read_var(interp_context* ctx, const char* id, int* out_var)
{
	for(stack_var* iter = ctx->stack + ctx->stack_top - 1;
		iter >= ctx->stack;
		--iter)
	{
		if (iter->id == id)
		{
			*out_var = iter->value;
			return true;
		}
	}
	debug_break();
	return false;
}

bool write_var(interp_context* ctx, const char* id, int value)
{
	for (stack_var* iter = ctx->stack + ctx->stack_top - 1;
		iter >= ctx->stack;
		--iter)
	{
		if (iter->id == id)
		{
			iter->value = value;
			return true;
		}
	}
	debug_break();
	return false;
}

bool interp(ASTNode* root, interp_context* ctx, int* out_result)
{
	if (root->is_empty)
	{
		*out_result = 0;
		return true;
	}
	else if (root->is_break_or_continue_op)
	{
		if (root->op == eToken::keyword_break)
		{
			*out_result = 0;
			ctx->break_triggered = true;
			return true;
		}
		else if (root->op == eToken::keyword_continue)
		{
			*out_result = 0;
			ctx->continue_triggered = true;
			return true;
		}
		RETURN_INTERP_FAILURE;
	}
	else if (root->is_number)
	{
		*out_result = (int)root->number;
		return true;
	}
	else if (root->is_unary_op)
	{
		if (root->children.size() != 1) RETURN_INTERP_FAILURE;
		if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;

		switch (root->op)
		{
		case '+': return true;
		case '-': *out_result = -*out_result; return true;
		case '~': *out_result = ~*out_result; return true;
		case '!': *out_result = !*out_result; return true;
		}
		
		RETURN_INTERP_FAILURE;
	}
	else if (root->is_binary_op)
	{
		if (root->children.size() != 2) RETURN_INTERP_FAILURE;

		// || and && are special in C. They short-circuit evaluation.
		// * If left-side of || is true, right-side should NOT be evaluated.
		// * If left-side of && is false, right-side should NOT be evaluated.
		if (root->op == eToken::logical_or)
		{
			if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (*out_result) return true;
			if (!interp(root->children[1], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (*out_result) *out_result = 1; // convert whatever the value of out_result is (could be -1 or whatever) to 1
			return true;
		}
		if (root->op == eToken::logical_and)
		{
			if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (!*out_result) return true;
			if (!interp(root->children[1], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (*out_result) *out_result = 1; // convert whatever the value of out_result is (could be -1 or whatever) to 1
			return true;
		}


		int lhs, rhs;
		if (!interp(root->children[0], ctx, &lhs) || !interp(root->children[1], ctx, &rhs)) RETURN_INTERP_FAILURE;

		switch (root->op)
		{
		case '%': *out_result = lhs % rhs; return true;
		case '*': *out_result = lhs * rhs; return true;
		case '+': *out_result = lhs + rhs; return true;
		case '-': *out_result = lhs - rhs; return true;
		case '/': *out_result = lhs / rhs; return true;
		case '<': *out_result = lhs < rhs; return true;
		case '>': *out_result = lhs > rhs; return true;
		case eToken::logical_and: RETURN_INTERP_FAILURE; // never should have gotten here. See special cases above.
		case eToken::logical_or: RETURN_INTERP_FAILURE; //never should have gotten here. See special cases above.
		case eToken::logical_equal:			*out_result = lhs == rhs; return true;
		case eToken::logical_not_equal:		*out_result = lhs != rhs; return true;
		case eToken::less_than_or_equal:	*out_result = lhs <= rhs; return true;
		case eToken::greater_than_or_equal: *out_result = lhs >= rhs; return true;
		}

		RETURN_INTERP_FAILURE;
	}
	else if (root->is_if
		|| root->is_ternery_op)
	{
		if (root->children.size() == 2)
		{
			// if(expr) statement;
			if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (*out_result)
			{
				if (!interp(root->children[1], ctx, out_result)) RETURN_INTERP_FAILURE;
			}
			return true;
		}
		else if (root->children.size() == 3)
		{
			// if(expr) statement; else statement;
			if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (*out_result)
			{
				if (!interp(root->children[1], ctx, out_result)) RETURN_INTERP_FAILURE;
			}
			else
			{
				if (!interp(root->children[2], ctx, out_result)) RETURN_INTERP_FAILURE;
			}
			return true;
		}
		RETURN_INTERP_FAILURE;
	}
	else if (root->is_for)
	{
		assert(root->children.size() == 4);
		if (!push_frame(ctx)) RETURN_INTERP_FAILURE;

		// init
		if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;

		assert(!ctx->return_triggered);
		assert(!ctx->break_triggered);
		assert(!ctx->continue_triggered);
		++ctx->loop_depth;
		while (true)
		{
			ctx->break_triggered = false;
			ctx->continue_triggered = false;

			// condition
			if (!root->children[1]->is_empty)
			{
				if (!interp(root->children[1], ctx, out_result)) RETURN_INTERP_FAILURE;
				if (!*out_result) break;
			}

			// body
			if (!interp(root->children[3], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (ctx->return_triggered
				|| ctx->break_triggered)
				break;

			// update
			if (!interp(root->children[2], ctx, out_result)) RETURN_INTERP_FAILURE;
		}
		--ctx->loop_depth;
		ctx->break_triggered = false;
		ctx->continue_triggered = false;

		if (!pop_frame(ctx)) RETURN_INTERP_FAILURE;
		return true;
	}
	else if (root->is_while)
	{
		assert(root->children.size() == 2);
		if (!push_frame(ctx)) RETURN_INTERP_FAILURE;

		assert(!ctx->return_triggered);
		assert(!ctx->break_triggered);
		assert(!ctx->continue_triggered);
		++ctx->loop_depth;
		while (true)
		{
			ctx->break_triggered = false;
			ctx->continue_triggered = false;

			// condition
			if (!root->children[0]->is_empty)
			{
				if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;
				if (!*out_result)
					break;
			}

			// body
			if (!interp(root->children[1], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (ctx->return_triggered
				|| ctx->break_triggered)
				break;
		}
		--ctx->loop_depth;
		ctx->break_triggered = false;
		ctx->continue_triggered = false;

		if (!pop_frame(ctx)) RETURN_INTERP_FAILURE;
		return true;
	}
	else if (root->is_do_while)
	{
		assert(root->children.size() == 2);
		if (!push_frame(ctx)) RETURN_INTERP_FAILURE;

		assert(!ctx->return_triggered);
		assert(!ctx->break_triggered);
		assert(!ctx->continue_triggered);
		++ctx->loop_depth;
		while (true)
		{
			ctx->break_triggered = false;
			ctx->continue_triggered = false;

			// body
			if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (ctx->return_triggered
				|| ctx->break_triggered)
				break;

			// condition
			if (!root->children[1]->is_empty)
			{
				if (!interp(root->children[1], ctx, out_result)) RETURN_INTERP_FAILURE;
				if (!*out_result)
					break;
			}
		}
		--ctx->loop_depth;
		ctx->break_triggered = false;
		ctx->continue_triggered = false;

		if (!pop_frame(ctx)) RETURN_INTERP_FAILURE;
		return true;
	}
	else if (root->type == AST_var)
	{
		if (root->var.is_variable_declaration && root->var.is_variable_assignment)
		{
			if (!push_var(ctx, root->var.name.nts)) RETURN_INTERP_FAILURE;
			if (!interp(root->var.assign_expression, ctx, out_result)) RETURN_INTERP_FAILURE;
			if (!write_var(ctx, root->var.name.nts, *out_result)) RETURN_INTERP_FAILURE;
			return true;
		}
		else if(root->var.is_variable_declaration)
		{
			assert(!root->var.assign_expression);
			if (!push_var(ctx, root->var.name.nts)) RETURN_INTERP_FAILURE;
			return true;
		}
		else if (root->var.is_variable_assignment)
		{
			if (!interp(root->var.assign_expression, ctx, out_result)) RETURN_INTERP_FAILURE;
			if (!write_var(ctx, root->var.name.nts, *out_result)) RETURN_INTERP_FAILURE;
			return true;
		}
		else if (root->var.is_variable_usage)
		{
			if (!read_var(ctx, root->var.name.nts, out_result)) RETURN_INTERP_FAILURE;
			return true;
		}

		RETURN_INTERP_FAILURE;
	}
	else if (root->is_block_list)
	{
		if (!push_frame(ctx)) RETURN_INTERP_FAILURE;
		if (root->children.size() <= 0) RETURN_INTERP_FAILURE;
		for (size_t i = 0; i < root->children.size(); ++i)
		{
			if (!interp(root->children[i], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (ctx->return_triggered)
				break;
			if (ctx->break_triggered || ctx->continue_triggered)
			{
				assert(ctx->loop_depth > 0);
				break;
			}
		}
		if (!pop_frame(ctx)) RETURN_INTERP_FAILURE;
		return true;
	}
	else if (root->type == AST_ret)
	{
		if (root->ret.expression && !interp(root->ret.expression, ctx, out_result)) RETURN_INTERP_FAILURE;
		ctx->return_triggered = true;
		return true;
	}
	else if (root->is_function)
	{
		assert(ctx->return_triggered == false);
		assert(ctx->break_triggered == false);
		assert(ctx->continue_triggered == false);
		if (!push_frame(ctx)) RETURN_INTERP_FAILURE;
		for (size_t i = 0; i < root->children.size(); ++i)
		{
			if (!interp(root->children[i], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (ctx->return_triggered)
				break;
		}
		assert(ctx->break_triggered == false);
		assert(ctx->continue_triggered == false);
		if (!pop_frame(ctx)) RETURN_INTERP_FAILURE;

		// HANDLE SPECIAL CASE: C standard says if main() does not have a return than it should return 0
		if (root->children.size() == 0)
		{
			if (root->func_name.nts == strings_insert_nts("main").nts)
			{
				// technically valid by C standard.
				*out_result = 0;
				return true;
			}
			// C standard: undefined behavior
			RETURN_INTERP_FAILURE;
		}

		return true;
	}
	else if (root->is_program)
	{
		if (root->children.size() <= 0) RETURN_INTERP_FAILURE;
		if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;
		return true;
	}
	
	RETURN_INTERP_FAILURE;
}

bool interp_return_value(ASTNode* root, int* out_result)
{
	interp_context ctx;
	ctx.stack_top = 0;
	ctx.loop_depth = 0;
	ctx.return_triggered = false;
	ctx.break_triggered = false;
	ctx.continue_triggered = false;

	return interp(root, &ctx, out_result);
}
