#pragma once
#include "ast.h"
#include "ir.h"

/* Command reference for comparing to clang

  get asm: clang -S file.c
  get ir:  clang -S -emit-llvm file.c

Either .s (asm) or .ll (ir) can be fed back into clang to generate .exe:
  clang file.s -> a.exe
  clang file.ll -> a.exe
 
Get program return value/errorlevel/exit status:
  cmd:  echo %errorlevel%
  bash: echo $?
*/

struct AsmInput
{
    const ASTNode* root;
};

bool gen_asm(FILE* file, const AsmInput& input);
bool gen_asm_from_ir(FILE* out, const IR* ir, size_t ir_size);