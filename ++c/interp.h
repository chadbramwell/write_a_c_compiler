#pragma once
#include "ast.h"

// a simple interpreter that will take an AST and either fail to execute or return a final result

bool interp_return_value(ASTNode* root, int* out_result);
