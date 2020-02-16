#include "gen.h"
#include "debug.h"
#include <stdlib.h>

static const bool GENERATE_DEBUG_BREAK_AT_START_OF_MAIN = false;
static const int MAX_FRAME_SIZE = 32;
static const int MAX_VARS_SIZE = 32;
static const int MAX_LOOP_LABELS_SIZE = 32;

struct loop_label
{
    const char* end_label; // for break/return
    const char* update_label; // for continue/end of body
};

struct global_var
{
    ASTNode* node;
};

struct stack_var
{
    ASTNode* id;
    char location[32]; // ex: 32(%rsp)
};

struct stack_frame
{
    stack_var vars[MAX_VARS_SIZE];
    int64_t num_vars;
    int64_t frame_size_in_bytes;
};


struct gen_ctx
{
    FILE* out;
    uint64_t label_index; // every label needs to be unique, this is appended to every label to ensure that's the case.

    // stack data
    stack_frame stack_frames[MAX_FRAME_SIZE];
    int64_t num_frames;

    // global vars
    global_var global_vars[MAX_VARS_SIZE];
    int64_t num_global_vars;

    // labels for break/continue/return inside of loop
    loop_label loop_labels[MAX_LOOP_LABELS_SIZE];
    int64_t num_loop_labels;
};

void declare_global_var(gen_ctx* ctx, ASTNode* node)
{
    assert(node->type == AST_var);
    const char* id = node->var.name.nts;

    // find it first, if already declared than ignore
    for (int64_t i = 0; i < ctx->num_global_vars; ++i)
    {
        if (ctx->global_vars[i].node->var.name.nts == id)
        {
            printf("WARNING? DOUBLE VAR DECL\n");
            return;
        }
    }

    // need room
    if (ctx->num_global_vars >= 32)
    {
        debug_break();
        return;
    }

    global_var* v = &ctx->global_vars[ctx->num_global_vars++];
    v->node = node;
}

void define_global_var(gen_ctx* ctx, ASTNode* node)
{
    assert(node->type == AST_var);
    const char* id = node->var.name.nts;

    // try to find it, if it exists ensure this is the first time we are defining it
    for (int64_t i = 0; i < ctx->num_global_vars; ++i)
    {
        ASTNode* gv = ctx->global_vars[i].node;
        if (gv->var.name.nts != id) continue;
        if (gv->var.assign_expression != NULL)
        {
            printf("ERROR? DOUBLE GLOBAL VAR DEFINITION\n");
            debug_break();
            return;
        }

        ctx->global_vars[i].node = node;
        return;
    }

    // first time for var, make sure we have room
    if (ctx->num_global_vars >= 32)
    {
        debug_break();
        return;
    }

    global_var* v = &ctx->global_vars[ctx->num_global_vars++];
    v->node = node;
}

stack_frame* push_stack_frame(gen_ctx* ctx)
{
    if (ctx->num_frames == MAX_FRAME_SIZE)
    {
        debug_break();
        return NULL;
    }

    stack_frame* f = &ctx->stack_frames[ctx->num_frames++];
    f->num_vars = 0;
    f->frame_size_in_bytes = 0;

    return f;
}

void push_vars_recursive(stack_frame* frame, ASTNode* n);
void push_vars_recursive_ASTNodeArray(stack_frame* frame, const ASTNodeArray* na)
{
    for (uint32_t i = 0; i < na->size; ++i)
    {
        ASTNode* n = na->nodes[i];
        push_vars_recursive(frame, n);
    }
}

void push_vars_recursive(stack_frame* frame, ASTNode* n)
{
    if (n == NULL)
        return;

    if (n->type == AST_var && n->var.is_variable_declaration)
    {
        // NOTE: there's one other place we do this. See AST_binop below.
        assert(frame->num_vars != MAX_VARS_SIZE);
        int64_t stack_offset = 32 + frame->num_vars * 8; // TODO: calc size of type
        stack_var* sv = &frame->vars[frame->num_vars++];
        sv->id = n;
        sprintf_s(sv->location, "%" PRIi64 "(%%rsp)", stack_offset);
        frame->frame_size_in_bytes += 8; // TODO: calc size of type
    }

    switch (n->type)
    {
    case AST_program:
        debug_break(); // we shouldn't hit this case, the initial caller should be a fdef
        return;
    case AST_blocklist:
        push_vars_recursive_ASTNodeArray(frame, &n->blocklist);
        return;
    case AST_ret:
        push_vars_recursive(frame, n->ret.expression);
        return;
    case AST_var:
        push_vars_recursive(frame, n->var.assign_expression);
        return;
    case AST_num:
        return;
    case AST_fdecl:
        debug_break(); // we shouldn't hit this case, the initial caller should be a fdef
        return;
    case AST_fdef:
        debug_break(); // we shouldn't hit this case, the initial caller should be a fdef
        return;
    case AST_fcall:
        push_vars_recursive_ASTNodeArray(frame, &n->fcall.args);
        return;
    case AST_if:
        push_vars_recursive(frame, n->ifdef.condition);
        push_vars_recursive(frame, n->ifdef.if_true);
        push_vars_recursive(frame, n->ifdef.if_false);
        return;
    case AST_for:
        push_vars_recursive(frame, n->forloop.init);
        push_vars_recursive(frame, n->forloop.condition);
        push_vars_recursive(frame, n->forloop.update);
        push_vars_recursive(frame, n->forloop.body);
        return;
    case AST_while: // fall-through
    case AST_dowhile:
        push_vars_recursive(frame, n->whileloop.condition);
        push_vars_recursive(frame, n->whileloop.body);
        return;
    case AST_binop: // BINOP REQUIRES A TEMPORARY LOCATION FOR STORAGE OF LEFT WHILE EVALUATING RIGHT. NOTE: We are doing this so we don't touch the stack. Previous binops would push/pop. Hopefully changing to a IR w/ infinite registers would simplify all this. 
        {
            assert(frame->num_vars != MAX_VARS_SIZE);
            int64_t stack_offset = 32 + frame->num_vars * 8; // TODO: calc size of type
            stack_var* sv = &frame->vars[frame->num_vars++];
            sv->id = n;
            sprintf_s(sv->location, "%" PRIi64 "(%%rsp)", stack_offset);
            frame->frame_size_in_bytes += 8; // TODO: calc size of type

            push_vars_recursive(frame, n->binop.left);
            push_vars_recursive(frame, n->binop.right);
        }
        return;
    case AST_unop:
        push_vars_recursive(frame, n->unop.on);
        return;
    case AST_terop:
        push_vars_recursive(frame, n->terop.condition);
        push_vars_recursive(frame, n->terop.if_true);
        push_vars_recursive(frame, n->terop.if_false);
    case AST_break: // fall-through
    case AST_continue: // fall-through
    case AST_empty:
        return;
    };

    debug_break(); // new AST type?
}

enum ePopType
{
    PT_gen_asm,
    PT_gen_asm_and_free,
    PT_no_asm_only_free,
};

bool pop_scope(gen_ctx* ctx, stack_frame* sf, ePopType pt)
{
    if (ctx->num_frames == 0)
    {
        debug_break();
        return false;
    }

    if (!sf)
    {
        sf = &ctx->stack_frames[ctx->num_frames - 1];
    }
    else if (sf != &ctx->stack_frames[ctx->num_frames - 1])
    {
        debug_break();
        return false;
    }

    bool gen_asm = (pt == PT_gen_asm || pt == PT_gen_asm_and_free);
    bool free_scope = (pt == PT_gen_asm_and_free || pt == PT_no_asm_only_free);

    if (gen_asm)
    {
        if (sf->frame_size_in_bytes > 0)
            fprintf(ctx->out, "  addq $%" PRIi64 ", %%rsp # pop function scope\n", sf->frame_size_in_bytes);
        fprintf(ctx->out, "  ret\n");
    }

    if (free_scope)
    {
        --ctx->num_frames;
    }
    return true;
}

bool emit_var_location(gen_ctx* ctx, const ASTNode* n)
{
    stack_frame* frame = ctx->stack_frames + ctx->num_frames - 1;
    for (uint32_t i = 0; i < frame->num_vars; ++i)
    {
        if (frame->vars[i].id == n)
        {
            fprintf(ctx->out, "%s", frame->vars[i].location);
            return true;
        }
    }

    // not on the stack frame, check global vars
    if (n->type != AST_var)
    {
        debug_break();
        return false;
    }

    for (int64_t i = 0; i < ctx->num_global_vars; ++i)
    {
        global_var* v = &ctx->global_vars[i];
        if (v->node->var.name.nts == n->var.name.nts)
        {
            // my_var_name(%rip) MORE INFO: https://stackoverflow.com/questions/56262889/why-are-global-variables-in-x86-64-accessed-relative-to-the-instruction-pointer
            fprintf(ctx->out, "%s(%%rip)", n->var.name.nts);
            return true;
        }
    }

    debug_break();
    return false;
}

bool copy_xxx_to_var(gen_ctx* ctx, const char* xxx, const ASTNode* n)
{
    fprintf(ctx->out, "  mov %s, ", xxx);

    if (!emit_var_location(ctx, n))
        return false;

    fprintf(ctx->out, "\n");
    return true;
}

bool copy_var_to_xxx(gen_ctx* ctx, const ASTNode* n, const char* xxx)
{
    fprintf(ctx->out, "  mov ");
    if (!emit_var_location(ctx, n))
        return false;

    fprintf(ctx->out, ", %s\n", xxx);
    return true;
}

bool gen_asm_node(gen_ctx* ctx, const ASTNode* n)
{
    assert(n);

    if (n->type == AST_empty)
        return true;

    if (n->type == AST_fcall)
    {
        // calling convention for x86/x64 on windows: https://en.wikipedia.org/wiki/X86_calling_conventions
        // rcx, rdx, r8, r9, then spill into stack
        const uint32_t numArgs = n->fcall.args.size;
        if (numArgs > 0)
        {
            if (!gen_asm_node(ctx, n->fcall.args.nodes[0])) return false;
            fprintf(ctx->out, "  mov %%rax, %%rcx\n");
        }
        if (numArgs > 1)
        {
            if (!gen_asm_node(ctx, n->fcall.args.nodes[1])) return false;
            fprintf(ctx->out, "  mov %%rax, %%rdx\n");
        }
        if (numArgs > 2)
        {
            if (!gen_asm_node(ctx, n->fcall.args.nodes[2])) return false;
            fprintf(ctx->out, "  mov %%rax, %%r8\n");
        }
        if (numArgs > 3)
        {
            if (!gen_asm_node(ctx, n->fcall.args.nodes[3])) return false;
            fprintf(ctx->out, "  mov %%rax, %%r9\n");
        }
        if (numArgs > 4)
        {
            debug_break(); // TODO: we only support 4 args at the moment. Would need to spill the rest into the stack and handle it...
        }

        fprintf(ctx->out, "  callq %s\n", n->fcall.name.nts);
        return true;
    }

    if (n->type == AST_fdef)
    {
        bool is_main = n->fdef.name.nts == strings_insert_nts("main").nts;
        stack_frame* func_sf = push_stack_frame(ctx);
        push_vars_recursive_ASTNodeArray(func_sf, &n->fdef.params);
        push_vars_recursive_ASTNodeArray(func_sf, &n->fdef.body);

        // start of function stack frame
        {
            fprintf(ctx->out, "%s:\n", n->fdef.name.nts);
            if (is_main && GENERATE_DEBUG_BREAK_AT_START_OF_MAIN)
            {
                fprintf(ctx->out, "  int $3\n"); // debug break, makes it easier to start step-by-step debugging with visual studio
            }
            func_sf->frame_size_in_bytes += 32; // because Windows https://en.wikipedia.org/wiki/X86_calling_conventions
            fprintf(ctx->out, "  subq $%" PRIu64 ", %%rsp\n", func_sf->frame_size_in_bytes);
        }

        // move all function params into the stack
        //  - reason: simplicity. we won't have to worry about these values getting trounced if this function calls another function
        // calling convention for x86/x64 on windows: https://en.wikipedia.org/wiki/X86_calling_conventions
        // rcx, rdx, r8, r9, then spill into stack
        {
            const uint32_t numArgs = n->fdef.params.size;
            if (numArgs > 0)
            {
                ASTNode* var_node = n->fdef.params.nodes[0];
                bool ok = copy_xxx_to_var(ctx, "%rcx", var_node);
                if (!ok) { debug_break(); return false; }
            }
            if (numArgs > 1)
            {
                ASTNode* var_node = n->fdef.params.nodes[1];
                bool ok = copy_xxx_to_var(ctx, "%rdx", var_node);
                if (!ok) { debug_break(); return false; }
            }
            if (numArgs > 2)
            {
                ASTNode* var_node = n->fdef.params.nodes[2];
                bool ok = copy_xxx_to_var(ctx, "%r8", var_node);
                if (!ok) { debug_break(); return false; }
            }
            if (numArgs > 3)
            {
                ASTNode* var_node = n->fdef.params.nodes[3];
                bool ok = copy_xxx_to_var(ctx, "%r9", var_node);
                if (!ok) { debug_break(); return false; }
            }
            if (numArgs > 4)
            {
                debug_break(); // TODO: this code only supports 4 params for calling functions. Need to update to "spill" into stack.
            }
        }

        // gen asm for body of function
        for(uint32_t i = 0; i < n->fdef.body.size; ++i)
        {
            if (!gen_asm_node(ctx, n->fdef.body.nodes[i]))
                return false;
        }

        // end of function stack frame
        {
            bool last_statement_is_return = false;
            if (n->fdef.body.size > 0 && n->fdef.body.nodes[n->fdef.body.size - 1]->type == AST_ret)
                last_statement_is_return = true;

            if (last_statement_is_return)
            {
                bool ok = pop_scope(ctx, func_sf, PT_no_asm_only_free);
                if (!ok)
                {
                    debug_break();
                    return false;
                }
                func_sf = NULL;
            }
            else
            {
                // According to the C11 Standard, if main doesn't have a return statement than it should return 0.
                //  Note: If this were not main than a missing return is UB (undefined behavior).
                if (is_main)
                {
                    fprintf(ctx->out, "  mov $0, %%rax\n");
                }
                else if (n->fdef.return_type == eToken::keyword_void)
                {
                }
                else
                {
                    // TODO: handle this case of UB (stage_9/valid/fib.c:fib does not have a return at end of function)
                    fprintf(ctx->out, "  int $3 # should never hit this!\n");
                    //debug_break();
                    //return false; // This is a case of UB! Replace this with a proper error message instead of complete failure.
                }

                bool ok = pop_scope(ctx, func_sf, PT_gen_asm_and_free);
                if (!ok)
                {
                    debug_break();
                    return false;
                }
                func_sf = NULL;
            }
        }
        return true;
    }

    if (n->type == AST_blocklist)
    {
        for (uint32_t i = 0; i < n->blocklist.size; ++i)
        {
            if (!gen_asm_node(ctx, n->blocklist.nodes[i]))
                return false;
        }

        return true;
    }

    if (n->type == AST_ret)
    {
        if (n->ret.expression && !gen_asm_node(ctx, n->ret.expression))
            return false;

        return pop_scope(ctx, NULL, PT_gen_asm);
    }

    if (n->type == AST_var)
    {
        if (n->var.is_variable_assignment)
        {
            if (!gen_asm_node(ctx, n->var.assign_expression))
                return false;
            return copy_xxx_to_var(ctx, "%rax", n->var.var_decl);
        }

        if (n->var.is_variable_usage)
        {
            return copy_var_to_xxx(ctx, n->var.var_decl, "%rax");
        }
        
        // I guess it's just a decl this time...
        assert(n->var.is_variable_declaration);
        return true;
    }

    if (n->type == AST_if)
    {
        bool has_else = n->ifdef.if_false;

        char label_else[32];
        sprintf_s(label_else, "else_%" PRIu64, ctx->label_index++);
        char label_end[32];
        sprintf_s(label_end, "fi_%" PRIu64, ctx->label_index++);

        fprintf(ctx->out, "# if\n");
        if (!gen_asm_node(ctx, n->ifdef.condition))
            return false;
        fprintf(ctx->out, "  cmp $0, %%rax\n");
        if (!has_else)
        {
            fprintf(ctx->out, "  je %s\n", label_end);
            if (!gen_asm_node(ctx, n->ifdef.if_true))
                return false;
        }
        else
        {
            fprintf(ctx->out, "# else\n");
            fprintf(ctx->out, "  je %s\n", label_else);
            if (!gen_asm_node(ctx, n->ifdef.if_true))
                return false;
            fprintf(ctx->out, "  jmp %s\n", label_end);

            fprintf(ctx->out, "%s:\n", label_else);
            if (!gen_asm_node(ctx, n->ifdef.if_false))
                return false;
        }
        fprintf(ctx->out, "%s:\n", label_end);
        return true;
    }

    if (n->type == AST_break)
    {
        assert(ctx->num_loop_labels > 0);
        loop_label* ll = &ctx->loop_labels[ctx->num_loop_labels - 1];
        fprintf(ctx->out, "  jmp %s\n", ll->end_label);
        return true;
    }
    if (n->type == AST_continue)
    {
        assert(ctx->num_loop_labels > 0);
        loop_label* ll = &ctx->loop_labels[ctx->num_loop_labels - 1];
        fprintf(ctx->out, "  jmp %s\n", ll->update_label);
        return true;
    }

    if (n->type == AST_for)
    {
        char label_for_update[32];
        sprintf_s(label_for_update, "for_update_%" PRIu64, ctx->label_index++);
        char label_for_cond[32];
        sprintf_s(label_for_cond, "for_cond_%" PRIu64, ctx->label_index++);
        char label_for_end[32];
        sprintf_s(label_for_end, "for_end_%" PRIu64, ctx->label_index++);

        assert(ctx->num_loop_labels < MAX_LOOP_LABELS_SIZE);
        loop_label* ll = &ctx->loop_labels[ctx->num_loop_labels++];
        ll->end_label = label_for_end;
        ll->update_label = label_for_update;
        {
            // init
            if (n->forloop.init)
            {
                if (!gen_asm_node(ctx, n->forloop.init))
                    return false;
            }

            // condition
            fprintf(ctx->out, "%s:\n", label_for_cond);
            if (n->forloop.condition)
            {
                if (!gen_asm_node(ctx, n->forloop.condition))
                    return false;
                fprintf(ctx->out, "  cmp $0, %%rax\n");
                fprintf(ctx->out, "  je %s\n", label_for_end);
            }

            // body
            if (!gen_asm_node(ctx, n->forloop.body))
                return false;

            // update - roll into from body or jump on continue
            fprintf(ctx->out, "%s:\n", label_for_update);
            if (n->forloop.update)
            {
                if (!gen_asm_node(ctx, n->forloop.update))
                    return false;
            }
            fprintf(ctx->out, "  jmp %s\n", label_for_cond);

            // end, jump here on break or return
            fprintf(ctx->out, "%s:\n", label_for_end);
        }
        --ctx->num_loop_labels;

        return true;
    }

    if (n->type == AST_while)
    {
        char label_while[32];
        sprintf_s(label_while, "while_%" PRIu64, ctx->label_index++);
        char label_while_end[32];
        sprintf_s(label_while_end, "while_end_%" PRIu64, ctx->label_index++);

        assert(ctx->num_loop_labels < MAX_LOOP_LABELS_SIZE);
        loop_label* ll = &ctx->loop_labels[ctx->num_loop_labels++];
        ll->end_label = label_while_end;
        ll->update_label = label_while;
        {

            // condition - jump here at end of body or on continue
            fprintf(ctx->out, "%s:\n", label_while);
            if (!gen_asm_node(ctx, n->whileloop.condition))
                return false;
            fprintf(ctx->out, "  cmp $0, %%rax\n");
            fprintf(ctx->out, "  je %s\n", label_while_end);

            // body
            if (!gen_asm_node(ctx, n->whileloop.body))
                return false;
            fprintf(ctx->out, "  jmp %s\n", label_while); // after body, return to start

            // end, jump here on break or return
            fprintf(ctx->out, "%s:\n", label_while_end);

        }
        --ctx->num_loop_labels;

        return true;
    }

    if (n->type == AST_dowhile)
    {
        char label_do_while_start[32];
        sprintf_s(label_do_while_start, "do_while_start_%" PRIu64, ctx->label_index++);
        char label_update_do_while[32];
        sprintf_s(label_update_do_while, "do_while_%" PRIu64, ctx->label_index++);
        char label_do_while_end[32];
        sprintf_s(label_do_while_end, "do_while_end_%" PRIu64, ctx->label_index++);

        assert(ctx->num_loop_labels < MAX_LOOP_LABELS_SIZE);
        loop_label* ll = &ctx->loop_labels[ctx->num_loop_labels++];
        ll->end_label = label_do_while_end;
        ll->update_label = label_update_do_while;
        {

            // loop start - jump here after checking condition
            fprintf(ctx->out, "%s:\n", label_do_while_start);

            // body
            if (!gen_asm_node(ctx, n->whileloop.body))
                return false;

            // condition - jump here on continue
            fprintf(ctx->out, "%s:\n", label_update_do_while);
            if (!gen_asm_node(ctx, n->whileloop.condition))
                return false;
            fprintf(ctx->out, "  cmp $0, %%rax\n");
            fprintf(ctx->out, "  je %s\n", label_do_while_end);
            fprintf(ctx->out, "  jmp %s\n", label_do_while_start);

            // end, jump here on break or return
            fprintf(ctx->out, "%s:\n", label_do_while_end);
        }
        --ctx->num_loop_labels;

        return true;
    }

    if (n->type == AST_terop)
    {
        char label_else[32];
        sprintf_s(label_else, "ter_false_%" PRIu64, ctx->label_index++);
        char label_end[32];
        sprintf_s(label_end, "ter_end_%" PRIu64, ctx->label_index++);

        if (!gen_asm_node(ctx, n->terop.condition))
            return false;
        fprintf(ctx->out, "  cmp $0, %%rax\n");
        fprintf(ctx->out, "  je %s\n", label_else);
        if (!gen_asm_node(ctx, n->terop.if_true))
            return false;
        fprintf(ctx->out, "  jmp %s\n", label_end);
        fprintf(ctx->out, "%s:\n", label_else);
        if (!gen_asm_node(ctx, n->terop.if_false))
            return false;
        fprintf(ctx->out, "%s:\n", label_end);
        return true;
    }

    if (n->type == AST_num)
    {
        fprintf(ctx->out, "  mov $%" PRIi64 ", %%rax\n", n->num.value);
        return true;
    }
    
    if (n->type == AST_unop)
    {
        if (!gen_asm_node(ctx, n->unop.on))
            return false;

        switch (n->unop.op)
        {
        case '-': fprintf(ctx->out, "  neg %%rax\n"); return true;
        case '~': fprintf(ctx->out, "  not %%rax\n"); return true;
        case '!':
            fprintf(ctx->out, "  cmp $0, %%rax\n");    // set ZF on if exp == 0, set it off otherwise
            fprintf(ctx->out, "  mov $0, %%rax\n"); // zero out EAX (doesn't change FLAGS), xor %eax %eax is better because it sets a flag we can't use it because we depend on the ZF flag on the next line
            fprintf(ctx->out, "  sete %%al\n"); //set AL register (the lower byte of EAX) to 1 iff ZF is on
            return true;
        }

        debug_break();
        return false;
    }

    if (n->type == AST_binop)
    {
        switch (n->binop.op)
        {
        case eToken::plus:
        {
            gen_asm_node(ctx, n->binop.left);
            copy_xxx_to_var(ctx, "%rax", n);
            gen_asm_node(ctx, n->binop.right);
            copy_var_to_xxx(ctx, n, "%rcx");
            fprintf(ctx->out, "  add %%rcx, %%rax\n");
        } break;
        case eToken::dash:
        {
            gen_asm_node(ctx, n->binop.right);
            copy_xxx_to_var(ctx, "%rax", n);
            gen_asm_node(ctx, n->binop.left);
            copy_var_to_xxx(ctx, n, "%rcx");
            fprintf(ctx->out, "  sub %%rcx, %%rax\n");
        } break;
        case eToken::star:
        {
            gen_asm_node(ctx, n->binop.left);
            copy_xxx_to_var(ctx, "%rax", n);
            gen_asm_node(ctx, n->binop.right);
            copy_var_to_xxx(ctx, n, "%rcx");
            fprintf(ctx->out, "  imul %%rcx, %%rax\n");
        } break;
        case eToken::forward_slash: case eToken::mod:
        {
            gen_asm_node(ctx, n->binop.right);
            copy_xxx_to_var(ctx, "%rax", n);
            gen_asm_node(ctx, n->binop.left);
            copy_var_to_xxx(ctx, n, "%rcx");
            fprintf(ctx->out, "  xor %%rdx, %%rdx\n"); //note dividend is combo of EDX:EAX. If we don't 0 out EDX we could get an integer overflow exception because RAX won't be big enough to store the result of the DIV
            fprintf(ctx->out, "  idiv %%rcx\n"); // quotient stored in rax, remainder in rdx
            if (n->binop.op == eToken::mod)
                fprintf(ctx->out, "  mov %%rdx, %%rax\n");
        } break;
        case '<':
        {
            gen_asm_node(ctx, n->binop.left);
            copy_xxx_to_var(ctx, "%rax", n);
            gen_asm_node(ctx, n->binop.right);
            copy_var_to_xxx(ctx, n, "%rcx");
            fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
            fprintf(ctx->out, "  mov $0, %%rax\n");
            fprintf(ctx->out, "  setl %%al\n");
        } break;
        case '>':
        {
            gen_asm_node(ctx, n->binop.left);
            copy_xxx_to_var(ctx, "%rax", n);
            gen_asm_node(ctx, n->binop.right);
            copy_var_to_xxx(ctx, n, "%rcx");
            fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
            fprintf(ctx->out, "  mov $0, %%rax\n");
            fprintf(ctx->out, "  setg %%al\n");
        } break;
        case eToken::logical_and:
        {
            uint64_t label_index_rightside = ++ctx->label_index;
            uint64_t label_index_end = ++ctx->label_index;

            gen_asm_node(ctx, n->binop.left);
            fprintf(ctx->out, "  cmp $0, %%rax\n");
            fprintf(ctx->out, "  jne check_right_of_and_%" PRIu64 "\n", label_index_rightside);
            fprintf(ctx->out, "  jmp end_and_%" PRIu64 "\n", label_index_end);
            fprintf(ctx->out, "check_right_of_and_%" PRIu64 ":\n", label_index_rightside);
            gen_asm_node(ctx, n->binop.right);
            fprintf(ctx->out, "  cmp $0, %%rax\n");
            fprintf(ctx->out, "  mov $0, %%rax\n");
            fprintf(ctx->out, "  setne %%al\n");
            fprintf(ctx->out, "end_and_%" PRIu64 ":\n", label_index_end);
        } break;
        case eToken::logical_or:
        {
            uint64_t label_index_rightside = ++ctx->label_index;
            uint64_t label_index_end = ++ctx->label_index;

            gen_asm_node(ctx, n->binop.left);
            fprintf(ctx->out, "  cmp $0, %%rax\n");
            fprintf(ctx->out, "  je check_right_of_or_%" PRIu64 "\n", label_index_rightside);
            fprintf(ctx->out, "  mov $1, %%rax\n");
            fprintf(ctx->out, "  jmp end_or_%" PRIu64 "\n", label_index_end);
            fprintf(ctx->out, "check_right_of_or_%" PRIu64 ":\n", label_index_rightside);
            gen_asm_node(ctx, n->binop.right);
            fprintf(ctx->out, "  cmp $0, %%rax\n");
            fprintf(ctx->out, "  mov $0, %%rax\n");
            fprintf(ctx->out, "  setne %%al\n");
            fprintf(ctx->out, "end_or_%" PRIu64 ":\n", label_index_end);
        } break;
        case eToken::logical_equal:
        {
            gen_asm_node(ctx, n->binop.left);
            copy_xxx_to_var(ctx, "%rax", n);
            gen_asm_node(ctx, n->binop.right);
            copy_var_to_xxx(ctx, n, "%rcx");
            fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
            fprintf(ctx->out, "  mov $0, %%rax\n");
            fprintf(ctx->out, "  sete %%al\n");
        } break;
        case eToken::logical_not_equal:
        {
            gen_asm_node(ctx, n->binop.left);
            copy_xxx_to_var(ctx, "%rax", n);
            gen_asm_node(ctx, n->binop.right);
            copy_var_to_xxx(ctx, n, "%rcx");
            fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
            fprintf(ctx->out, "  mov $0, %%rax\n");
            fprintf(ctx->out, "  setne %%al\n");
        } break;
        case eToken::less_than_or_equal:
        {
            gen_asm_node(ctx, n->binop.left);
            copy_xxx_to_var(ctx, "%rax", n);
            gen_asm_node(ctx, n->binop.right);
            copy_var_to_xxx(ctx, n, "%rcx");
            fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
            fprintf(ctx->out, "  mov $0, %%rax\n");
            fprintf(ctx->out, "  setle %%al\n");
        } break;
        case eToken::greater_than_or_equal:
        {
            gen_asm_node(ctx, n->binop.left);
            copy_xxx_to_var(ctx, "%rax", n);
            gen_asm_node(ctx, n->binop.right);
            copy_var_to_xxx(ctx, n, "%rcx");
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
    if (input.root->type != AST_program)
    {
        debug_break();
        return false;
    }
    gen_ctx stack_ctx = {};
    //gen_ctx* ctx = (gen_ctx*)calloc(1, sizeof(gen_ctx));
    gen_ctx* ctx = &stack_ctx;
    ctx->out = file;

    /* CLANG ASM of "int foo;int main(){return foo;}int foo = 3;"
        .text
        .def     main;
        .scl    2;
        .type    32;
        .endef
        .globl    main                    # -- Begin function main
        .p2align    4, 0x90
    main:                                   # @main
    .seh_proc main
    # %bb.0:
        pushq    %rax
        .seh_stackalloc 8
        .seh_endprologue
        movl    $0, 4(%rsp)
        movl    foo(%rip), %eax
        popq    %rcx
        retq
        .seh_handlerdata
        .text
        .seh_endproc
                                            # -- End function
        .data
        .globl    foo                     # @foo
        .p2align    2
    foo:
        .long    3                       # 0x3
    */

    // expose all global functions and find all global vars
    for (uint32_t i = 0; i < input.root->program.size; ++i)
    {
        ASTNode* n = input.root->program.nodes[i];
        if (n->type == AST_fdef)
        {
            fprintf(ctx->out, "  .globl %s\n", n->fdef.name.nts);
        }
        else if (n->type == AST_var)
        {
            // REVISIT: Ideally we would have a stage in-between to:
            // * collapse global defs and decls into a single node
            // * resolve complex const expressions at compile time
            if (n->var.assign_expression)
            {
                if (n->var.assign_expression->type != AST_num)
                {
                    debug_break();
                    free(ctx);
                    return false;
                }

                define_global_var(ctx, n);
            }
            else
            {
                declare_global_var(ctx, n);
            }
        }
    }

    // now write all global vars to asm (and init their location var)
    if (ctx->num_global_vars > 0) fprintf(ctx->out, "  .data\n");
    for (int64_t i = 0; i < ctx->num_global_vars; ++i)
    {
        global_var* gv = &ctx->global_vars[i];
        const char* name = gv->node->var.name.nts;
        ASTNode* assign_expression = gv->node->var.assign_expression;
        if (assign_expression && assign_expression->type != AST_num)
        {
            debug_break();
            free(ctx);
            return false;
        }
        fprintf(ctx->out, "  .global %s\n", name);
        fprintf(ctx->out, "  .p2align 3\n");
        if (assign_expression)
            fprintf(ctx->out, "%s:\n  .long %" PRIi64 "\n", name, assign_expression->num.value);
        else
            fprintf(ctx->out, "%s:\n  .zero 8\n", name);
    }
    if (ctx->num_global_vars > 0) fprintf(ctx->out, "  .text\n");

    for (uint32_t i = 0; i < input.root->program.size; ++i)
    {
        ASTNode* n = input.root->program.nodes[i];
        if (n->type == AST_fdef)
        {
            
            if (!gen_asm_node(ctx, n))
            {
                free(ctx);
                return false;
            }
        }
    }
    
    //free(ctx);
    return true;
}

bool gen_asm_from_ir(FILE* out, const IR** ir, size_t* ir_size)
{
    // clang -S return_2.c
    /* CLANG ASM of "int main(){return 2;}"
        .text
        .def     main;
        .scl    2;
        .type    32;
        .endef
        .globl    main                    # -- Begin function main
        .p2align    4, 0x90
    main:                                   # @main
    # %bb.0:
        movl    $2, %eax
        retq
                                            # -- End function

    */
    // clang -S -emit-llvm return_2.c
    /*
        ; ModuleID = 'return_2.c'
        source_filename = "return_2.c"
        target datalayout = "e-m:w-i64:64-f80:128-n8:16:32:64-S128"
        target triple = "x86_64-pc-windows-msvc19.16.27034"

        ; Function Attrs: noinline nounwind optnone uwtable
        define i32 @main() #0 {
          %1 = alloca i32, align 4
          store i32 0, i32* %1, align 4
          ret i32 2
        }

        attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

        !llvm.module.flags = !{!0, !1}
        !llvm.ident = !{!2}

        !0 = !{i32 1, !"wchar_size", i32 2}
        !1 = !{i32 7, !"PIC Level", i32 2}
        !2 = !{!"clang version 6.0.1 (tags/RELEASE_601/final)"}
    */
    fprintf(out, ".text\n");
    fprintf(out, ".def");

    return true;
}
