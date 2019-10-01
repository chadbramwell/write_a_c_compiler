#include "gen.h"
#include "debug.h"

struct loop_label
{
    const char* end_label; // for break/return
    const char* update_label; // for continue/end of body
};

struct stack_var
{
    str name;
    int64_t offset; // negative if local var, positive if function argument (i.e. cdecl convention)
};

struct stack_frame
{
    stack_var* vars; // TODO: Fix Memory Leak here.
    int64_t num_vars;
};

struct gen_ctx
{
    FILE* out;
    uint64_t label_index; // every label needs to be unique, this is appended to every label to ensure that's the case.

    // stack data
    stack_frame stack_frames[256];
    int64_t num_frames;

    // labels for break/continue/return inside of loop
    std::vector<loop_label> loop_labels;
};

bool push_stack_frame(gen_ctx* ctx)
{
    if (ctx->num_frames == 256)
    {
        debug_break(); // error! out of memory!
        return false;
    }

    // calling convention for x86/x64 on windows: https://en.wikipedia.org/wiki/X86_calling_conventions
    // is super funky. there's a "shadow stack" we have to account for.

    if (ctx->num_frames > 0)
    {
        fprintf(ctx->out, "  push %%rbp\n"); // save old value of EBP
        fprintf(ctx->out, "  mov %%rsp, %%rbp\n"); // current top of stack is bottom of new stack frame
    }
    fprintf(ctx->out, "  subq $32, %%rsp # make room for \"shadow stack\" on windows\n");

    ctx->stack_frames[ctx->num_frames].vars = NULL;
    ctx->stack_frames[ctx->num_frames].num_vars = 0;
    ++ctx->num_frames;
    return true;
}

enum ePopType {
    pop_func_def,
    pop_ret,
    pop_func_def_no_shadow,
};

bool pop_stack_frame(gen_ctx* ctx, ePopType pt)
{
    if (ctx->num_frames == 0)
    {
        debug_break(); // error! too many pops!
        return false;
    }

    if (pt != pop_func_def_no_shadow)
        fprintf(ctx->out, "  addq $32, %%rsp # restore \"shadow stack\" for windows\n");

    if (ctx->num_frames > 1)
    {
        fprintf(ctx->out, "  mov %%rbp, %%rsp\n"); // restore ESP; now it points to old EBP
        fprintf(ctx->out, "  pop %%rbp\n"); // restore old EBP; now ESP is where it was before prologue
    }

    if (pt == pop_ret)
        return true;

    --ctx->num_frames;
    return true;
}

bool push_local_var(gen_ctx* ctx, str name)
{
    if (ctx->num_frames == 0)
    {
        debug_break(); // error! push a stack frame first!
        return false;
    }

    stack_frame* frame = &ctx->stack_frames[ctx->num_frames - 1];

    frame->vars = (stack_var*)realloc(frame->vars, size_t((frame->num_vars + 1) * sizeof(stack_var)));
    frame->vars[frame->num_vars].name = name;
    frame->vars[frame->num_vars].offset = (frame->num_vars + 1) * 8;
    frame->vars[frame->num_vars].offset += 32; // +32 = shadow stack
    frame->num_vars += 1;
    return true;
}

bool push_var(gen_ctx* ctx, str name, int64_t offset)
{
    offset += 32;  // +32 = shadow stack

    if (ctx->num_frames == 0)
    {
        debug_break(); // error! push a stack frame first!
        return false;
    }

    stack_frame* frame = &ctx->stack_frames[ctx->num_frames - 1];

    frame->vars = (stack_var*)realloc(frame->vars, size_t((frame->num_vars + 1) * sizeof(stack_var)));
    frame->vars[frame->num_vars].name = name;
    frame->vars[frame->num_vars].offset = offset;
    frame->num_vars += 1;
    return true;
}

int64_t get_stack_offset(gen_ctx* ctx, str s) // TODO??? : update this with what we know about function calling convention: https://en.wikipedia.org/wiki/X86_calling_conventions  namely that first four params passed into func are rcx, rdx, r8 and r9
{
    for (stack_frame* frame = ctx->stack_frames + ctx->num_frames;
        frame >= ctx->stack_frames;
        --frame)
    {
        for(stack_var* var = frame->vars + frame->num_vars;
            var >= frame->vars;
            --var)
        {
            if (var->name.nts == s.nts)
            {
                return var->offset;
            }
        }
    }

    debug_break();
    return 0;
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
        uint32_t numArgs = n->fcall.args.size;
        assert(numArgs >= 0 && numArgs <= 2);
        if (numArgs == 1 || numArgs == 2)
        {
            if (!gen_asm_node(ctx, n->fcall.args.nodes[0])) return false;
            fprintf(ctx->out, "  mov %%rax, %%rcx\n");
        }
        if (numArgs == 2)
        {
            if (!gen_asm_node(ctx, n->fcall.args.nodes[1])) return false;
            fprintf(ctx->out, "  mov %%rax, %%rdx\n");
        }

        fprintf(ctx->out, "  callq %s\n", n->fcall.name.nts);
        return true;
    }

    if (n->type == AST_fdef)
    {
        fprintf(ctx->out, "%s:\n", n->fdef.name.nts);

        bool ok = push_stack_frame(ctx);
        if (!ok)
        {
            debug_break();
            return false;
        }

        bool is_main = n->fdef.name.nts == strings_insert_nts("main").nts;
        if (is_main)
        {
            fprintf(ctx->out, "  int $3\n"); // debug break, makes it easier to start step-by-step debugging with visual studio
        }

        // calling convention for x86/x64 on windows: https://en.wikipedia.org/wiki/X86_calling_conventions
        // rcx, rdx, r8, r9, then spill into stack
        uint32_t numArgs = n->fdef.params.size;
        assert(numArgs >= 0 && numArgs <= 2);
        if (numArgs == 1 || numArgs == 2)
        {
            uint64_t stack_offset = 8;
            str var_name = n->fdef.params.nodes[0]->var.name;
            if (!push_var(ctx, var_name, stack_offset))
                return false;
            fprintf(ctx->out, "  mov %%rcx, %" PRIu64 "(%%rbp) #write %s\n", stack_offset, var_name.nts);
        }
        if (numArgs == 2)
        {
            uint64_t stack_offset = 16;
            str var_name = n->fdef.params.nodes[1]->var.name;
            if (!push_var(ctx, var_name, stack_offset))
                return false;
            fprintf(ctx->out, "  mov %%rdx, %" PRIu64 "(%%rbp) #write %s\n", stack_offset, var_name.nts);
        }

        for(uint32_t i = 0; i < n->fdef.body.size; ++i)
        {
            if (!gen_asm_node(ctx, n->fdef.body.nodes[i]))
                return false;
        }
        
        // Handle main() that does not have a return statement.
        if (is_main)
        {
            if (n->fdef.body.size == 0 || n->fdef.body.nodes[n->fdef.body.size - 1]->type != AST_ret)
            {
                // According to the C11 Standard, if main doesn't have a return statement than it should return 0.
                //  If a function other than main does not have a return statement than it is undefined behavior.
                //  Thus the assert. TODO: add error handling and let user know about undefined behavior.
                fprintf(ctx->out, "  mov $0, %%rax\n");
                ok = pop_stack_frame(ctx, pop_func_def);
                assert(ok);
                fprintf(ctx->out, "  ret\n");
            }
            else
            {
                ok = pop_stack_frame(ctx, pop_func_def_no_shadow);
                assert(ok);
            }
        }
        else
        {
            ok = pop_stack_frame(ctx, pop_func_def);
            assert(ok);
        }

        
        return ok;
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

        bool ok = pop_stack_frame(ctx, pop_ret);
        assert(ok);
        fprintf(ctx->out, "  ret\n");

        return ok;
    }

    if (n->type == AST_var)
    {
        if (n->var.is_variable_declaration)
        {
            bool ok = push_local_var(ctx, n->var.name);
            if (!ok)
            {
                debug_break();
                return false;
            }
            fprintf(ctx->out, "  sub $8, %%rsp #make room for %s\n", n->var.name.nts);
        }

        if (n->var.is_variable_assignment)
        {
            if (!gen_asm_node(ctx, n->var.assign_expression))
                return false;
            uint64_t stack_offset = get_stack_offset(ctx, n->var.name);
            if (!stack_offset)
                return false;
            fprintf(ctx->out, "  mov %%rax, %" PRIu64 "(%%rbp) #write %s\n", stack_offset, n->var.name.nts);
            return true;
        }

        if (n->var.is_variable_usage)
        {
            fprintf(ctx->out, "  mov %" PRIu64 "(%%rbp), %%rax #read %s\n", get_stack_offset(ctx, n->var.name), n->var.name.nts);
            return true;
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
        assert(ctx->loop_labels.size() > 0);
        fprintf(ctx->out, "  jmp %s\n", ctx->loop_labels.back().end_label);
        return true;
    }
    if (n->type == AST_continue)
    {
        assert(ctx->loop_labels.size() > 0);
        fprintf(ctx->out, "  jmp %s\n", ctx->loop_labels.back().update_label);
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

        loop_label ll;
        ll.end_label = label_for_end;
        ll.update_label = label_for_update;
        ctx->loop_labels.push_back(ll);

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

        ctx->loop_labels.pop_back();

        return true;
    }

    if (n->type == AST_while)
    {
        char label_while[32];
        sprintf_s(label_while, "while_%" PRIu64, ctx->label_index++);
        char label_while_end[32];
        sprintf_s(label_while_end, "while_end_%" PRIu64, ctx->label_index++);

        loop_label ll;
        ll.end_label = label_while_end;
        ll.update_label = label_while;
        ctx->loop_labels.push_back(ll);

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

        ctx->loop_labels.pop_back();

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

        loop_label ll;
        ll.end_label = label_do_while_end;
        ll.update_label = label_update_do_while;
        ctx->loop_labels.push_back(ll);

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
        
        ctx->loop_labels.pop_back();

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
            fprintf(ctx->out, "  push %%rax\n");
            gen_asm_node(ctx, n->binop.right);
            fprintf(ctx->out, "  pop %%rcx\n");
            fprintf(ctx->out, "  add %%rcx, %%rax\n");
        } break;
        case eToken::dash:
        {
            gen_asm_node(ctx, n->binop.right);
            fprintf(ctx->out, "  push %%rax\n");
            gen_asm_node(ctx, n->binop.left);
            fprintf(ctx->out, "  pop %%rcx\n");
            fprintf(ctx->out, "  sub %%rcx, %%rax\n");
        } break;
        case eToken::star:
        {
            gen_asm_node(ctx, n->binop.left);
            fprintf(ctx->out, "  push %%rax\n");
            gen_asm_node(ctx, n->binop.right);
            fprintf(ctx->out, "  pop %%rcx\n");
            fprintf(ctx->out, "  imul %%rcx, %%rax\n");
        } break;
        case eToken::forward_slash: case eToken::mod:
        {
            gen_asm_node(ctx, n->binop.right);
            fprintf(ctx->out, "  push %%rax\n");
            gen_asm_node(ctx, n->binop.left);
            fprintf(ctx->out, "  pop %%rcx\n");
            fprintf(ctx->out, "  xor %%rdx, %%rdx\n"); //note dividend is combo of EDX:EAX. If we don't 0 out EDX we could get an integer overflow exception because RAX won't be big enough to store the result of the DIV
            fprintf(ctx->out, "  idiv %%rcx\n"); // quotient stored in rax, remainder in rdx
            if (n->binop.op == eToken::mod)
                fprintf(ctx->out, "  mov %%rdx, %%rax\n");
        } break;
        case '<':
        {
            gen_asm_node(ctx, n->binop.left);
            fprintf(ctx->out, "  push %%rax\n");
            gen_asm_node(ctx, n->binop.right);
            fprintf(ctx->out, "  pop %%rcx\n");
            fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
            fprintf(ctx->out, "  mov $0, %%rax\n");
            fprintf(ctx->out, "  setl %%al\n");
        } break;
        case '>':
        {
            gen_asm_node(ctx, n->binop.left);
            fprintf(ctx->out, "  push %%rax\n");
            gen_asm_node(ctx, n->binop.right);
            fprintf(ctx->out, "  pop %%rcx\n");
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
            fprintf(ctx->out, "  push %%rax\n");
            gen_asm_node(ctx, n->binop.right);
            fprintf(ctx->out, "  pop %%rcx\n");
            fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
            fprintf(ctx->out, "  mov $0, %%rax\n");
            fprintf(ctx->out, "  sete %%al\n");
        } break;
        case eToken::logical_not_equal:
        {
            gen_asm_node(ctx, n->binop.left);
            fprintf(ctx->out, "  push %%rax\n");
            gen_asm_node(ctx, n->binop.right);
            fprintf(ctx->out, "  pop %%rcx\n");
            fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
            fprintf(ctx->out, "  mov $0, %%rax\n");
            fprintf(ctx->out, "  setne %%al\n");
        } break;
        case eToken::less_than_or_equal:
        {
            gen_asm_node(ctx, n->binop.left);
            fprintf(ctx->out, "  push %%rax\n");
            gen_asm_node(ctx, n->binop.right);
            fprintf(ctx->out, "  pop %%rcx\n");
            fprintf(ctx->out, "  cmp %%rax, %%rcx\n");
            fprintf(ctx->out, "  mov $0, %%rax\n");
            fprintf(ctx->out, "  setle %%al\n");
        } break;
        case eToken::greater_than_or_equal:
        {
            gen_asm_node(ctx, n->binop.left);
            fprintf(ctx->out, "  push %%rax\n");
            gen_asm_node(ctx, n->binop.right);
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
    ctx.num_frames = 0;

    /*
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
    ASTNode* main = NULL;

    if (!input.root->type == AST_program)
    {
        debug_break();
        return false;
    }

    for (uint32_t i = 0; i < input.root->program.size; ++i)
    {
        ASTNode* n = input.root->program.nodes[i];
        if (n->type == AST_fdef && n->fdef.name.nts == strings_insert_nts("main").nts)
        {
            main = input.root->program.nodes[i];
            break;
        }
    }

    if (!main)
    {
        debug_break();
        return false;
    }

    // expose all function defs as globals
    //for (uint32_t i = 0; i < input.root->program.size; ++i)
    //{
    //    if (input.root->program.nodes[i]->type == AST_fdef)
    //    {
            fprintf(ctx.out, "  .globl %s\n", main->fdef.name.nts);
    //    }
    //}

    for (uint32_t i = 0; i < input.root->program.size; ++i)
    {
        ASTNode* n = input.root->program.nodes[i];
        if (n->type == AST_fdef)
        {
            
            if (!gen_asm_node(&ctx, n))
                return false;
        }
    }
    
    
    return true;
}
