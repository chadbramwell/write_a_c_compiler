#pragma once
#include <inttypes.h>
#include <stdio.h> // FILE

// After writing lex -> ast -> asm, I realized that the layout of function calls in for ast directly matches the data structure of the ast itself.
// Which leads me to believe that the ast data structure isn't necessary at all. The structure of the function calls could remain to validate
// the code but what the final data structure could be just an IR which would make both optimizations and translation to assembly simpler.
// Open Thoughts/Questions:
// * What should the IR look like for main? (study Clang's IR and generate something similar? Attempt to create it whole-cloth?)
// * HOLDOVER FROM AST: I should look into improving my error-handling. It's strangely overloaded to handle both real parsing errors and failure
//   to begin parsing errors. (i.e. I might call parse_function() blindly and if it fails to find a return type then it fails and I attempt something
//   else, but if we are in the middle of function parsing and something else fails unexpectedly, like missing parens, then it becomes a "true"
//   semantic error.

enum eIR { // Intermediate Representation
    IR_UNKNOWN,
    IR_RETURN,
    IR_RETURN_VALUE,
    IR_GLOBAL_FUNC,
    IR_CONSTANT,
    IR_UNARY_OP,
    IR_BINARY_OP,
};
enum eVT { // Value Type
    VT_UNKNOWN,
    VT_void,
    VT_uint64,
};

struct IR
{
    eIR type;
    union {
        struct { // IR_RETURN_VALUE
            uint64_t rid; // register id
        } retval;
        struct { // IR_GLOBAL_FUNC
            eVT return_type;
            const char* name;
            eVT* params;
        } func;
        struct { // IR_CONSTANT
            uint64_t value;
            uint64_t rid; // register id
        } constant;
        struct { // IR_UNARY_OP
            uint8_t op;
            uint64_t rid_from;
            uint64_t rid_to;
        } un;
        struct {  // IR_BINARY_OP
            uint8_t op;
            uint64_t rid_left;
            uint64_t rid_right;
            uint64_t rid_out;
        } bin;
    };
};

bool ir(const struct Token* tokens, size_t num_tokens, IR** out, size_t* out_size);
bool ir_func_interior(const struct Token* tokens, size_t num_tokens, IR** out, size_t* out_size);
void dump_ir(FILE* out, const IR* ir, size_t ir_size);