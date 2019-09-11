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
	if (root->is_number)
	{
		*out_result = (int)root->number;
		return true;
	}
	if (root->is_unary_op)
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
	if (root->is_binary_op)
	{
		if (root->children.size() != 2) RETURN_INTERP_FAILURE;

		int lhs, rhs;
		if (!interp(root->children[0], ctx, &lhs) || !interp(root->children[1], ctx, &rhs)) RETURN_INTERP_FAILURE;

		switch (root->op)
		{
		case '*': *out_result = lhs * rhs; return true;
		case '+': *out_result = lhs + rhs; return true;
		case '-': *out_result = lhs - rhs; return true;
		case '/': *out_result = lhs / rhs; return true;
		case '<': *out_result = lhs < rhs; return true;
		case '>': *out_result = lhs > rhs; return true;
		case eToken::logical_and:			*out_result = lhs && rhs; return true;
		case eToken::logical_or:			*out_result = lhs || rhs; return true;
		case eToken::logical_equal:			*out_result = lhs == rhs; return true;
		case eToken::logical_not_equal:		*out_result = lhs != rhs; return true;
		case eToken::less_than_or_equal:	*out_result = lhs <= rhs; return true;
		case eToken::greater_than_or_equal: *out_result = lhs >= rhs; return true;
		}

		RETURN_INTERP_FAILURE;
	}
	if (root->is_if
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
	if (root->var_name.nts)
	{
		if (root->is_variable_declaration && root->is_variable_assignment)
		{
			assert(root->children.size() == 1);
			if (!push_var(ctx, root->var_name.nts)) RETURN_INTERP_FAILURE;
			if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (!write_var(ctx, root->var_name.nts, *out_result)) RETURN_INTERP_FAILURE;
			return true;
		}
		else if(root->is_variable_declaration)
		{
			assert(root->children.size() == 0);
			if (!push_var(ctx, root->var_name.nts)) RETURN_INTERP_FAILURE;
			return true;
		}
		else if (root->is_variable_assignment)
		{
			assert(root->children.size() == 1);
			if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;
			if (!write_var(ctx, root->var_name.nts, *out_result)) RETURN_INTERP_FAILURE;
			return true;
		}
		else if (root->is_variable_usage)
		{
			if (!read_var(ctx, root->var_name.nts, out_result)) RETURN_INTERP_FAILURE;
			return true;
		}

		RETURN_INTERP_FAILURE;
	}
	if (root->is_block_list)
	{
		if (!push_frame(ctx)) RETURN_INTERP_FAILURE;
		if (root->children.size() <= 0) RETURN_INTERP_FAILURE;
		if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;
		if (!pop_frame(ctx)) RETURN_INTERP_FAILURE;
		return true;
	}
	if (root->is_return)
	{
		assert(root->children.size() != 0); // This should've been caught in "if(root->is_function)"
		if (root->children.size() <= 0) RETURN_INTERP_FAILURE;
		if (!interp(root->children[0], ctx, out_result)) RETURN_INTERP_FAILURE;
		return true;
	}
	if (root->is_function)
	{
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

		if (!push_frame(ctx)) RETURN_INTERP_FAILURE;		

		for (size_t i = 0; i < root->children.size(); ++i)
		{
			if (!interp(root->children[i], ctx, out_result)) RETURN_INTERP_FAILURE;
		}		

		if (!pop_frame(ctx)) RETURN_INTERP_FAILURE;
		return true;
	}
	if (root->is_program)
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

	return interp(root, &ctx, out_result);
}
