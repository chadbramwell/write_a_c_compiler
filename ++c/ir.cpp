#include "ir.h"
#include "lex.h"
#include "debug.h"
#include <stdlib.h>

enum eFailureReason {
    FR_OKAY,
    FR_OUT_OF_TOKENS,

    FR_SEMANTIC_ERROR_EXPECTED_VALUE_TYPE,
    FR_SEMANTIC_ERROR_EXPECTED_IDENTIFIER,
    FR_SEMANTIC_ERROR_FUNC_MISSING_OPEN_PARENS,
    FR_SEMANTIC_ERROR_FUNC_MISSING_CLOSED_PARENS,
    FR_SEMANTIC_ERROR_NOT_ALL_CONTROL_PATHS_RETURN_VALUE,
    FR_SEMANTIC_ERROR_WRONG_RETURN_TYPE,
    FR_SEMANTIC_ERROR_EXPECTED_SEMICOLON,
    FR_SEMANTIC_ERROR_EXPECTED_EXPRESSION,
    FR_SEMANTIC_ERROR_UNARY_OP_MISSING_TARGET,
    FR_SEMANTIC_ERROR_ONLY_MAIN_IS_ALLOWED_TO_HAVE_MISSING_RETURN,
    FR_SEMANTIC_ERROR_MAIN_WITHOUT_INT_OR_VOID_RETURN_TYPE,

    FR_COMPILER_ERROR_VALUE_TYPE_NOT_HANDLED,
    FR_COMPILER_ERROR_MORE_TOKENS_TO_CONSUME,
    FR_COMPILER_ERROR_MISSING_RETURN_VALUE_IR,

    FR_TODO_OTHER_GLOBAL_STUFF_LIKE_INCLUDE_AND_PRAGMA,
    FR_TODO_GLOBAL_VAR,
    FR_TODO_FUNC_PARAMS,
    FR_TODO_FUNC_DEF,
    FR_TODO_NON_RETURN_STATEMENTS,
    FR_TODO_RETURN_NON_CONSTANT,
    FR_TODO_LOOP_CONSTRUCTS,
    FR_TODO_HANDLE_DIFFERING_RETURN_TYPES,
};

struct TokenStream
{
    const Token* next;
    const Token* end;
};

#define CHECK_OUT_OF_TOKENS if(tokens.next == tokens.end) { debug_break(); return FR_OUT_OF_TOKENS; }
#define RETURN_ERROR(x) {debug_break(); return x;}
#define RETURN_TODO(x) {debug_break(); return x;}

struct ir_context
{
    const char* failure;
    eToken func_return_type; // used to verify return value of funcs
    //ASTNodeArray var_decl_stack; // fixup references
    IR* ir; // used realloc_ir
    size_t irsz;
    uint64_t next_rid;
};

static size_t emplace_back_ir(ir_context* ctx)
{
    size_t offset = ctx->irsz;
    ++ctx->irsz;
    ctx->ir = (IR*)realloc(ctx->ir, ctx->irsz * sizeof(IR));
    return offset;
}

static IR* last_ir(ir_context* ctx)
{
    if (ctx->ir == NULL || ctx->irsz == 0) {
        debug_break();
        return NULL;
    }
    return ctx->ir + ctx->irsz - 1;
}

eVT to_value_type(eToken t)
{
    switch (t) {
    case eToken::keyword_void: return VT_void;
    case eToken::keyword_int: return VT_uint64;
    }
    return VT_UNKNOWN;
}

static eFailureReason global_var_or_func(eVT vt, TokenStream* io_tokens, ir_context* ctx);
static eFailureReason func_interior(TokenStream* io_tokens, ir_context* ctx);
static eFailureReason transform_translation_unit(TokenStream* io_tokens, ir_context* ctx)
{
    TokenStream tokens = *io_tokens;
    
    eFailureReason fr = FR_OKAY;
    while (tokens.next != tokens.end)
    {
        eVT vt = to_value_type(tokens.next->type);
        if (vt != VT_UNKNOWN)
        {
            ++tokens.next;
            fr = global_var_or_func(vt, &tokens, ctx);
            if (fr == FR_OKAY)
                continue;
            RETURN_ERROR(FR_COMPILER_ERROR_VALUE_TYPE_NOT_HANDLED);
        }
        RETURN_TODO(FR_TODO_OTHER_GLOBAL_STUFF_LIKE_INCLUDE_AND_PRAGMA);
    }
    
    if (fr != FR_OKAY) return fr;
    if (tokens.next != tokens.end) RETURN_ERROR(FR_COMPILER_ERROR_MORE_TOKENS_TO_CONSUME);
    return FR_OKAY;
}

static eFailureReason global_var_or_func(eVT vt, TokenStream* io_tokens, ir_context* ctx)
{
    TokenStream tokens = *io_tokens;
    CHECK_OUT_OF_TOKENS;
    if (tokens.next->type != eToken::identifier) RETURN_ERROR(FR_SEMANTIC_ERROR_EXPECTED_IDENTIFIER);
    const Token* id = tokens.next;
    ++tokens.next; CHECK_OUT_OF_TOKENS;
    // global var
    if (tokens.next->type == eToken::semicolon) RETURN_TODO(FR_TODO_GLOBAL_VAR);
    if (tokens.next->type == eToken::assignment) RETURN_TODO(FR_TODO_GLOBAL_VAR);
    // global func def or impl
    if (tokens.next->type != eToken::open_parens) RETURN_ERROR(FR_SEMANTIC_ERROR_FUNC_MISSING_OPEN_PARENS);
    ++tokens.next; CHECK_OUT_OF_TOKENS;
    if (tokens.next->type != eToken::closed_parens) RETURN_TODO(FR_TODO_FUNC_PARAMS);
    ++tokens.next; CHECK_OUT_OF_TOKENS;
    if (tokens.next->type != eToken::open_curly) RETURN_TODO(FR_TODO_FUNC_DEF);
    ++tokens.next; CHECK_OUT_OF_TOKENS;

    {
        size_t i = emplace_back_ir(ctx);
        IR* f = ctx->ir + i;
        f->type = IR_GLOBAL_FUNC;
        f->func.return_type = vt;
        f->func.name = id->identifier.nts;
        f->func.params = NULL;
    }
    
    eFailureReason fr = func_interior(&tokens, ctx);
    if (fr != FR_OKAY) return fr;

    CHECK_OUT_OF_TOKENS;
    if (tokens.next->type != eToken::closed_curly) RETURN_ERROR(FR_SEMANTIC_ERROR_FUNC_MISSING_CLOSED_PARENS);
    ++tokens.next;

    const eIR last_type = last_ir(ctx)->type;
    if (last_type != IR_RETURN && last_type != IR_RETURN_VALUE)
    {
        if (is_str_main(id->identifier.nts)) {
            if (vt == VT_void) {
                size_t i = emplace_back_ir(ctx);
                IR* r = ctx->ir + i;
                r->type = IR_RETURN;
            } else if (vt == VT_uint64) {
                const uint64_t zero_rid = ++ctx->next_rid;

                size_t i = emplace_back_ir(ctx);
                IR* r = ctx->ir + i;
                r->type = IR_CONSTANT;
                r->constant.value = 0;
                r->constant.rid = zero_rid;

                i = emplace_back_ir(ctx);
                r = ctx->ir + i;
                r->type = IR_RETURN_VALUE;
                r->retval.rid = zero_rid;
            } else {
                RETURN_ERROR(FR_SEMANTIC_ERROR_MAIN_WITHOUT_INT_OR_VOID_RETURN_TYPE);
            }
        } else if (vt != VT_void) {
            RETURN_ERROR(FR_SEMANTIC_ERROR_NOT_ALL_CONTROL_PATHS_RETURN_VALUE);
        } else {
            size_t i = emplace_back_ir(ctx);
            IR* r = ctx->ir + i;
            r->type = IR_RETURN;
        }
    }
    
    *io_tokens = tokens;
    return FR_OKAY;
}

static eFailureReason func_interior(TokenStream* io_tokens, ir_context* ctx)
{
    TokenStream tokens = *io_tokens;
    CHECK_OUT_OF_TOKENS;

    while (tokens.next != tokens.end && tokens.next->type != eToken::closed_curly)
    {
        // find expression and write to ir in reverse
        {
            // find end of expression
            const Token* const expr_start = tokens.next;
            
            // handle cases that can only happen at start of expression
            switch (tokens.next->type) {
                case eToken::keyword_return: 
                    ++tokens.next;
            }

            // handle the rest
            while (tokens.next != tokens.end) {
                switch (tokens.next->type) {
                case eToken::constant_number: // fall-through
                case '!': case '-': case '~': // fall-through
                    ++tokens.next;
                    continue;
                }
                break;
            }
            CHECK_OUT_OF_TOKENS;

            // check that we found something
            if (tokens.next - expr_start == 0) {
                RETURN_ERROR(FR_SEMANTIC_ERROR_EXPECTED_EXPRESSION);
            }

            // if it's just a return, handle it
            if (tokens.next - expr_start == 1
                && expr_start->type == eToken::keyword_return) {
                size_t i = emplace_back_ir(ctx);
                IR* r = ctx->ir + i;
                r->type = IR_RETURN;
                continue;
            }

            // write expression in reverse
            uint64_t last_rid = 0;
            for (const Token* expr_i = tokens.next - 1; expr_i >= expr_start; --expr_i)
            {
                size_t i = emplace_back_ir(ctx);
                IR* r = ctx->ir + i;
                switch (expr_i->type) {

                    case eToken::keyword_return: {
                        if (last_rid == 0) {
                            RETURN_ERROR(FR_COMPILER_ERROR_MISSING_RETURN_VALUE_IR);
                        }
                        r->type = IR_RETURN_VALUE;
                        r->retval.rid = last_rid;
                    } break;

                    case eToken::constant_number: {
                        r->type = IR_CONSTANT;
                        r->constant.value = expr_i->number;
                        r->constant.rid = ++ctx->next_rid;
                        last_rid = r->constant.rid;
                    } break;

                    case '!': case '-': case '~': {
                        if (last_rid == 0) {
                            RETURN_ERROR(FR_SEMANTIC_ERROR_UNARY_OP_MISSING_TARGET);
                        }
                        r->type = IR_UNARY_OP;
                        r->un.op = expr_i->type;
                        r->un.rid_from = last_rid;
                        r->un.rid_to = ++ctx->next_rid;
                        last_rid = r->un.rid_to;
                    } break;
                }
            }
        }

        // after expression there should be a semicolon
        if (tokens.next->type != eToken::semicolon) RETURN_ERROR(FR_SEMANTIC_ERROR_EXPECTED_SEMICOLON);
        ++tokens.next;
    }
    *io_tokens = tokens;
    return eFailureReason::FR_OKAY;
}

bool ir(const Token* tokens, size_t num_tokens, IR** out, size_t* out_size)
{
    TokenStream io_tokens;
    io_tokens.next = tokens;
    io_tokens.end = tokens + num_tokens;

    ir_context ctx = {};

    eFailureReason fr = transform_translation_unit(&io_tokens, &ctx);

    if (fr != FR_OKAY)
    {
        debug_break();
        return false;
    }

    *out = ctx.ir;
    *out_size = ctx.irsz;

    return true;
}

bool ir_func_interior(const struct Token* tokens, size_t num_tokens, IR** out, size_t* out_size)
{
    TokenStream io_tokens;
    io_tokens.next = tokens;
    io_tokens.end = tokens + num_tokens;

    ir_context ctx = {};

    eFailureReason fr = func_interior(&io_tokens, &ctx);

    if (fr != FR_OKAY)
    {
        debug_break();
        return false;
    }

    *out = ctx.ir;
    *out_size = ctx.irsz;

    return true;
}

static const char* binop_to_string(uint8_t op) {
    switch (op) {
    case '%': return "%"; // mod
    case '&': return "&"; // bitwise and
    case '*': return "*"; // mul
    case '+': return "+"; // add
    case '-': return "-"; // sub
    case '/': return "/"; // div
    case '<': return "<"; // less-than
    case '>': return ">"; // greater-than
    case '|': return "|"; // bitwise or
    case eToken::logical_and: return "&&";
    case eToken::logical_or: return "||";
    case eToken::logical_equal: return "==";
    case eToken::logical_not_equal: return "!=";
    case eToken::less_than_or_equal: return "<=";
    case eToken::greater_than_or_equal: return ">=";
    }
    debug_break();
    return "<unknown op> binop_to_string failed.";
}

void dump_ir(FILE* out, const IR* ir, size_t ir_size) {
    const IR* const ir_end = ir + ir_size;
    int ir_index = 0;
    while (ir != ir_end) {

        fprintf(out, "[%3d] ", ir_index++);

        switch (ir->type) {
        case IR_UNKNOWN: fprintf(out, "IR_UNKNOWN"); debug_break();  break;
        case IR_RETURN: fprintf(out, "IR_RETURN"); break;
        case IR_RETURN_VALUE: fprintf(out, "IR_RETURN_VALUE: r%" PRIu64, ir->retval.rid); break;
        case IR_GLOBAL_FUNC: fprintf(out, "IR_GLOBAL_FUNC(%s)", ir->func.name); break;
        case IR_CONSTANT: 
            fprintf(out, "IR_CONSTANT: $%" PRIu64 " -> r%" PRIu64, 
                ir->constant.value, 
                ir->constant.rid);
            break;
        case IR_UNARY_OP: 
            fprintf(out, "IR_UNARY_OP: %cr%" PRIu64 " -> r%" PRIu64,
                ir->un.op,
                ir->un.rid_from,
                ir->un.rid_to);
            break;
        case IR_BINARY_OP: 
            fprintf(out, "IR_BINARY_OP: r%" PRIu64 " %s r%" PRIu64 " -> r%" PRIu64, 
                ir->bin.rid_left,
                binop_to_string(ir->bin.op),
                ir->bin.rid_right,
                ir->bin.rid_out); 
            break;
        default: debug_break(); fprintf(out, "??? TODO ???"); break;
        }

        ++ir;
        if (ir != ir_end)
            fprintf(out, "\n");
    }
}
