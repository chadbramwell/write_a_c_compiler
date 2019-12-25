#pragma once

// After writing lex -> ast -> asm, I realized that the layout of function calls in for ast directly matches the data structure of the ast itself.
// Which leads me to believe that the ast data structure isn't necessary at all. The structure of the function calls could remain to validate
// the code but what the final data structure could be just an IR which would make both optimizations and translation to assembly simpler.
// Open Thoughts/Questions:
// * What should the IR look like for main? (study Clang's IR and generate something similar? Attempt to create it whole-cloth?)
// * HOLDOVER FROM AST: I should look into improving my error-handling. It's strangely overloaded to handle both real parsing errors and failure
//   to begin parsing errors. (i.e. I might call parse_function() blindly and if it fails to find a return type then it fails and I attempt something
//   else, but if we are in the middle of function parsing and something else fails unexpectedly, like missing parens, then it becomes a "true"
//   semantic error.

enum IR_TYPE {
    IR_UNKNOWN,
    IR_FUNC,
};

struct IR
{
    IR_TYPE t;
};

bool ir(const struct Token* tokens, size_t num_tokens, IR** out, size_t** out_sz);
