#pragma once
#include "strings.h"
#include <inttypes.h>
#include <stdio.h>

// Below is an attempt at a simple lexer. It's not meant to be optimal.

enum eToken : uint8_t
{
    UNKNOWN = 0,
    identifier = 1,
    constant_number = 2,
    string = 3,

    logical_not     = '!',//ASCII 33
    operators_start = logical_not,
    // " # $
    mod             = '%',//37
    bitwise_and     = '&',//38
    // '
    open_parens     = '(',//40
    closed_parens   = ')',//41
    star            = '*',//42 - could be multiply, pointer decl, pointer deref
    plus            = '+',//43 - could be sign decl or binary add
    comma           = ',',//44
    dash            = '-',//45 - could be sign decl or binary sub
    // .
    forward_slash   = '/',//47
    // 0-9
    colon           = ':',//58
    semicolon       = ';',//59
    less_than       = '<',//60
    assignment      = '=',//61
    greater_than    = '>',//62
    question_mark   = '?',//63
    // @ A-Z [ \ ] ^ _ ` a-z
    open_curly      = '{',//123
    bitwise_or      = '|',//124
    closed_curly    = '}',//125
    bitwise_not     = '~',//126

    logical_and = 127,      // &&
    logical_or,             // ||
    logical_equal,          // ==
    logical_not_equal,      // !=
    less_than_or_equal,     // <=
    greater_than_or_equal,  // >=
    operators_end = greater_than_or_equal,

    keyword_void, keywords_first = keyword_void,
    keyword_int,
    keyword_return,
    keyword_if,
    keyword_else,
    keyword_for,
    keyword_while,
    keyword_do,
    keyword_break,
    keyword_continue, keywords_last = keyword_continue,

    comment,
    //include_path,
};

struct str_slice
{
    const char* start;
    const char* end;
};

struct Token
{
    eToken type;
    str_slice location;

    union {
        str identifier; // only valid if type == eToken::identifier or one of the keywords
        uint64_t number; // only valid if type == eToken::constant_number
        str_slice str; // only valid if type == eToken::string
    };
};

struct LexInput
{
    const char* filename;
    const char* stream;
    uint64_t length;
};

struct LexOutput
{
    Token* tokens;
    uint64_t num_tokens;

    char* strings;
    char* strings_end;

    const char* failure_location;
    const char* failure_reason;
};

LexInput init_lex(const char* filename, const char* filedata, uint64_t filelen);
bool lex(const LexInput* input, LexOutput* output);
void dump_lex(FILE* file, const LexOutput* lex);
