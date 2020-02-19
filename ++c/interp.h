#pragma once
#include "ast.h"

// a simple interpreter that will take an AST and either fail to execute or return a final result

bool interp_return_value(ASTNode* root, int64_t* out_result);
bool interp_ir(const struct IR* ir, size_t ir_size, int8_t* out_result); // NOTE: linux only supports a return value up to 128