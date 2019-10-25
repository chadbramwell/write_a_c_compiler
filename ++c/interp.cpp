#include "interp.h"
#include "debug.h"
#include "strings.h"

#define RETURN_INTERP_FAILURE do{ debug_break(); return false; } while(0)

struct stack_var
{
    // NOTE assumption here: all ids exist in the same string pool (see "strings.h") and thus we can
    // simply store the pointer and compare on the pointer for equality.
    const char* id;
    int64_t value;
};

struct global_var
{
    const char* id; // same assumption ast stack_var here. See NOTE above.
    bool defined; // global vars may be forward-declared any number of times but they can only be defined once
    int64_t value;
};

struct interp_context
{
    stack_var stack[256]; // uses sentinal values for stack frames
    int64_t stack_top;

    int loop_depth;
    bool return_triggered;
    bool break_triggered;
    bool continue_triggered;

    ASTNodeArray global_funcs;
    global_var global_vars[256];
    int64_t num_global_vars;
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

void declare_global_var(interp_context* ctx, const char* id)
{
    // find it first, if already declared than ignore
    for (int64_t i = 0; i < ctx->num_global_vars; ++i)
    {
        if (ctx->global_vars[i].id == id)
        {
            return;
        }
    }

    // need room
    if (ctx->num_global_vars >= 256)
    {
        debug_break();
        return;
    }

    global_var* v = &ctx->global_vars[ctx->num_global_vars++];
    v->id = id;
    v->defined = false;
    v->value = 0; // global vars default to zero even if they are never defined
}

void define_global_var(interp_context* ctx, const char* id, int64_t value)
{
    // try to find it, if it exists ensure this is the first time we are defining it
    for (int64_t i = 0; i < ctx->num_global_vars; ++i)
    {
        global_var* v = &ctx->global_vars[i];
        if (v->id != id) continue;
        if (v->defined)
        {
            debug_break();
            return;
        }

        v->defined = true;
        v->value = value;
        return;
    }

    // first time for var, make sure we have room
    if (ctx->num_global_vars >= 256)
    {
        debug_break();
        return;
    }

    global_var* v = &ctx->global_vars[ctx->num_global_vars++];
    v->id = id;
    v->defined = true;
    v->value = value;
}

bool push_var(interp_context* ctx, const char* id)
{
    if (ctx->stack_top >= 256)
    {
        debug_break();
        return false; // no room
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

bool read_var(interp_context* ctx, const char* id, int64_t* out_var)
{
    // try reading from stack first
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

    // not found on stack so check global vars (no need to iterate backwards here, globals can't shadow themselves)
    for (int64_t i = 0; i < ctx->num_global_vars; ++i)
    {
        global_var* v = &ctx->global_vars[i];
        if (v->id == id)
        {
            *out_var = v->value;
            return true;
        }
    }

    debug_break();
    return false;
}

bool write_var(interp_context* ctx, const char* id, int64_t value)
{
    // try writing stack first
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

    // not found on stack so check global vars (no need to iterate backwards here, globals can't shadow themselves)
    for (int64_t i = 0; i < ctx->num_global_vars; ++i)
    {
        global_var* v = &ctx->global_vars[i];
        if (v->id == id)
        {
            v->value = value;
            return true;
        }
    }

    debug_break();
    return false;
}

bool push_func_var(interp_context* ctx, int64_t value)
{
    if (ctx->stack_top >= 256)
    {
        debug_break();
        return false; // no room
    }

    ctx->stack[ctx->stack_top].id = NULL;
    ctx->stack[ctx->stack_top].value = value;
    ++ctx->stack_top;

    return true;
}

bool pop_func_frame(interp_context* ctx, int64_t count)
{
    if (ctx->stack_top - count < 0)
    {
        debug_break();
        return false; // stack vars don't exist
    }

    // verify func frame
    for (int64_t i = ctx->stack_top - count; i < ctx->stack_top; ++i)
    {
        if (ctx->stack[i].id != NULL)
            return false; // all func vars should have an id of NULL. See push_func_var.
    }

    ctx->stack_top -= count;
    return true;
}

bool interp(ASTNode* root, interp_context* ctx, int64_t* out_result)
{
    assert(root);
    if (root->type == AST_empty)
    {
        *out_result = 0;
        return true;
    }
    if (root->type == AST_break)
    {
        *out_result = 0;
        ctx->break_triggered = true;
        return true;
    }
    else if (root->type == AST_continue)
    {
        *out_result = 0;
        ctx->continue_triggered = true;
        return true;
    }
    else if (root->type == AST_num)
    {
        *out_result = root->num.value;
        return true;
    }
    else if (root->type == AST_unop)
    {
        if (!interp(root->unop.on, ctx, out_result)) RETURN_INTERP_FAILURE;

        switch (root->unop.op)
        {
        case '+': return true;
        case '-': *out_result = -*out_result; return true;
        case '~': *out_result = ~*out_result; return true;
        case '!': *out_result = !*out_result; return true;
        }
        
        RETURN_INTERP_FAILURE;
    }
    else if (root->type == AST_binop)
    {
        // || and && are special in C. They short-circuit evaluation.
        // * If left-side of || is true, right-side should NOT be evaluated.
        // * If left-side of && is false, right-side should NOT be evaluated.
        if (root->binop.op == eToken::logical_or)
        {
            if (!interp(root->binop.left, ctx, out_result)) RETURN_INTERP_FAILURE;
            if (*out_result) return true;
            if (!interp(root->binop.right, ctx, out_result)) RETURN_INTERP_FAILURE;
            if (*out_result) *out_result = 1; // convert whatever the value of out_result is (could be -1 or whatever) to 1
            return true;
        }
        if (root->binop.op == eToken::logical_and)
        {
            if (!interp(root->binop.left, ctx, out_result)) RETURN_INTERP_FAILURE;
            if (!*out_result) return true;
            if (!interp(root->binop.right, ctx, out_result)) RETURN_INTERP_FAILURE;
            if (*out_result) *out_result = 1; // convert whatever the value of out_result is (could be -1 or whatever) to 1
            return true;
        }


        int64_t lhs, rhs;
        if (!interp(root->binop.left, ctx, &lhs) || !interp(root->binop.right, ctx, &rhs)) RETURN_INTERP_FAILURE;

        switch (root->binop.op)
        {
        case '%': *out_result = lhs % rhs; return true;
        case '*': *out_result = lhs * rhs; return true;
        case '+': *out_result = lhs + rhs; return true;
        case '-': *out_result = lhs - rhs; return true;
        case '/': *out_result = lhs / rhs; return true;
        case '<': *out_result = lhs < rhs; return true;
        case '>': *out_result = lhs > rhs; return true;
        case eToken::logical_and:           RETURN_INTERP_FAILURE; // never should have gotten here. See special cases above.
        case eToken::logical_or:            RETURN_INTERP_FAILURE; // never should have gotten here. See special cases above.
        case eToken::logical_equal:         *out_result = lhs == rhs; return true;
        case eToken::logical_not_equal:     *out_result = lhs != rhs; return true;
        case eToken::less_than_or_equal:    *out_result = lhs <= rhs; return true;
        case eToken::greater_than_or_equal: *out_result = lhs >= rhs; return true;
        }

        RETURN_INTERP_FAILURE;
    }
    else if (root->type == AST_if)
    {
        if (!interp(root->ifdef.condition, ctx, out_result)) RETURN_INTERP_FAILURE;
        if (*out_result)
        {
            if (!interp(root->ifdef.if_true, ctx, out_result)) RETURN_INTERP_FAILURE;
        }
        else if(root->ifdef.if_false)
        {
            if (!interp(root->ifdef.if_false, ctx, out_result)) RETURN_INTERP_FAILURE;
        }
        return true;
    }
    else if (root->type == AST_terop)
    {
        if (!interp(root->terop.condition, ctx, out_result)) RETURN_INTERP_FAILURE;
        if (*out_result)
        {
            if (!interp(root->terop.if_true, ctx, out_result)) RETURN_INTERP_FAILURE;
        }
        else
        {
            if (!interp(root->terop.if_false, ctx, out_result)) RETURN_INTERP_FAILURE;
        }
        return true;
    }
    else if (root->type == AST_for)
    {
        if (!push_frame(ctx)) RETURN_INTERP_FAILURE;

        // init
        if (root->forloop.init && !interp(root->forloop.init, ctx, out_result)) RETURN_INTERP_FAILURE;

        assert(!ctx->return_triggered);
        assert(!ctx->break_triggered);
        assert(!ctx->continue_triggered);
        ++ctx->loop_depth;
        while (true)
        {
            ctx->break_triggered = false;
            ctx->continue_triggered = false;

            // condition
            if (root->forloop.condition)
            {
                if (!interp(root->forloop.condition, ctx, out_result)) RETURN_INTERP_FAILURE;
                if (!*out_result) break;
            }

            // body
            if (!interp(root->forloop.body, ctx, out_result)) RETURN_INTERP_FAILURE;
            if (ctx->return_triggered || ctx->break_triggered)
                break;

            // update
            if (root->forloop.update && !interp(root->forloop.update, ctx, out_result)) RETURN_INTERP_FAILURE;
        }
        --ctx->loop_depth;
        ctx->break_triggered = false;
        ctx->continue_triggered = false;

        if (!pop_frame(ctx)) RETURN_INTERP_FAILURE;
        return true;
    }
    else if (root->type == AST_while)
    {
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
            if (!interp(root->whileloop.condition, ctx, out_result)) RETURN_INTERP_FAILURE;
            if (!*out_result)
                break;

            // body
            if (!interp(root->whileloop.body, ctx, out_result)) RETURN_INTERP_FAILURE;
            if (ctx->return_triggered || ctx->break_triggered)
                break;
        }
        --ctx->loop_depth;
        ctx->break_triggered = false;
        ctx->continue_triggered = false;

        if (!pop_frame(ctx)) RETURN_INTERP_FAILURE;
        return true;
    }
    else if (root->type == AST_dowhile)
    {
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
            if (!interp(root->whileloop.body, ctx, out_result)) RETURN_INTERP_FAILURE;
            if (ctx->return_triggered || ctx->break_triggered)
                break;

            // condition
            if (!interp(root->whileloop.condition, ctx, out_result)) RETURN_INTERP_FAILURE;
            if (!*out_result)
                break;
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
    else if (root->type == AST_blocklist)
    {
        if (!push_frame(ctx)) RETURN_INTERP_FAILURE;
        for (uint32_t i = 0; i < root->blocklist.size; ++i)
        {
            if (!interp(root->blocklist.nodes[i], ctx, out_result)) RETURN_INTERP_FAILURE;
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
    else if (root->type == AST_fdef)
    {
        assert(ctx->return_triggered == false);
        assert(ctx->break_triggered == false);
        assert(ctx->continue_triggered == false);
        if (!push_frame(ctx)) RETURN_INTERP_FAILURE;
        for (uint32_t i = 0; i < root->fdef.body.size; ++i)
        {
            if (!interp(root->fdef.body.nodes[i], ctx, out_result)) RETURN_INTERP_FAILURE;
            if (ctx->return_triggered)
                break;
        }
        assert(ctx->break_triggered == false);
        assert(ctx->continue_triggered == false);
        if (!pop_frame(ctx)) RETURN_INTERP_FAILURE;

        if (ctx->return_triggered)
            return true;

        if (root->fdef.return_type == eToken::keyword_void)
        {
            // ignore out_result
            ctx->return_triggered = true;
            return true;
        }

        // HANDLE SPECIAL CASE: C standard says if main() does not have a return than it should return 0
        if (root->fdef.name.nts == strings_insert_nts("main").nts)
        {
            // technically valid by C standard.
            *out_result = 0;
            return true;
        }

        // C standard: undefined behavior. TODO: Turn this into an error once we have error reporting.
        RETURN_INTERP_FAILURE;
    }
    else if (root->type == AST_program)
    {
        // hardcoded. find main. see also gen.cpp
        // also: populate global funcs
        str strMain = strings_insert_nts("main");
        ASTNode* main = NULL;
        for (uint32_t i = 0; i < root->program.size; ++i)
        {
            ASTNode* n = root->program.nodes[i];
            if (n->type == AST_fdef)
            {
                astn_push(&ctx->global_funcs, n);
                if(n->fdef.name.nts == strMain.nts)
                    main = n;
            }
            else if (n->type == AST_var)
            {

            }
        }

        if (!main) RETURN_INTERP_FAILURE;
        if (!interp(main, ctx, out_result)) RETURN_INTERP_FAILURE;

        // special case in standard. if main does not have a return, than it should return 0
        if (!ctx->return_triggered)
            *out_result = 0;

        return true;
    }
    else if (root->type == AST_fcall)
    {
        assert(root->fcall.name.nts != strings_insert_nts("main").nts); // I'm not sure if recurisve calls to main is okay...

        // special case (would normally be found when linking against stdandard library)
        if (root->fcall.name.nts == strings_insert_nts("putchar").nts)
        {
            assert(root->fcall.args.size == 1);
            if (!interp(root->fcall.args.nodes[0], ctx, out_result)) RETURN_INTERP_FAILURE;
            *out_result = putchar((int)*out_result);
            return true;
        }

        // find in global func list
        ASTNode* func = NULL;
        for (uint32_t i = 0; i < ctx->global_funcs.size; ++i)
        {
            ASTNode* potential = ctx->global_funcs.nodes[i];
            assert(potential->type == AST_fdef);
            if (potential->fdef.name.nts == root->fcall.name.nts)
            {
                func = ctx->global_funcs.nodes[i];
                break;
            }
        }
        if (!func) RETURN_INTERP_FAILURE;

        // verify call is cool, probably should verify this somewhere else or modify the AST to have cycles...
        if (root->fcall.args.size != func->fdef.params.size) RETURN_INTERP_FAILURE;

        // interp each arg and push on stack (labeled as function definitions var names)
        if (!push_frame(ctx)) RETURN_INTERP_FAILURE;
        for (uint32_t i = 0; i < root->fcall.args.size; ++i)
        {
            if (!interp(root->fcall.args.nodes[i], ctx, out_result)) RETURN_INTERP_FAILURE;
            //if (!push_func_var(ctx, *out_result)) RETURN_INTERP_FAILURE;
            assert(func->fdef.params.nodes[i]->type == AST_var);
            if (!push_var(ctx, func->fdef.params.nodes[i]->var.name.nts)) RETURN_INTERP_FAILURE;
            if (!write_var(ctx, func->fdef.params.nodes[i]->var.name.nts, *out_result)) RETURN_INTERP_FAILURE;
        }

        // call func
        if (!interp(func, ctx, out_result)) RETURN_INTERP_FAILURE;
        assert(ctx->return_triggered);
        ctx->return_triggered = false;

        // pop all vars we pushed on the stack for the func call
        //if (!pop_func_frame(ctx, root->fcall.args.size)) RETURN_INTERP_FAILURE;
        if (!pop_frame(ctx)) RETURN_INTERP_FAILURE;
        return true;
    }
    
    RETURN_INTERP_FAILURE;
}

bool interp_return_value(ASTNode* root, int64_t* out_result)
{
    interp_context ctx = {};

    if (root->type != AST_program)
    {
        debug_break();
        return false;
    }

    // initialize globals and find main
    str strMain = strings_insert_nts("main");
    ASTNode* main = NULL;
    for (uint32_t i = 0; i < root->program.size; ++i)
    {
        ASTNode* n = root->program.nodes[i];
        if (n->type == AST_fdef)
        {
            astn_push(&ctx.global_funcs, n);
            if (n->fdef.name.nts == strMain.nts)
                main = n;
        }
        else if (n->type == AST_var)
        {
            if (n->var.assign_expression)
            {
                int64_t v;
                bool ok = interp(n->var.assign_expression, NULL, &v);
                if (!ok)
                {
                    debug_break();
                    return false;
                }

                define_global_var(&ctx, n->var.name.nts, v);
            }
            else
            {
                declare_global_var(&ctx, n->var.name.nts);
            }
        }
    }

    if (!main) RETURN_INTERP_FAILURE;
    if (!interp(main, &ctx, out_result)) RETURN_INTERP_FAILURE;

    // special case in standard. if main does not have a return, than it should return 0
    if (!ctx.return_triggered)
        *out_result = 0;

    return true;
}
