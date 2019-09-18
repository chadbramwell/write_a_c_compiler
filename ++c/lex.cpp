#include "lex.h"
#include "debug.h"
#include "strings.h"

bool is_letter_or_underscore(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool isnumber(char c)
{
    return c >= '0' && c <= '9';
}
bool iswhitespace(char c)
{
    return (c >= '\t' && c <= '\r') || c == ' ';
}

void push_1c(LexOutput* out, eToken type, const char* start)
{
    assert(out->tokens_size != LexOutput::MAX_TOKENS); // out of memory
    Token* token = &out->tokens[out->tokens_size++];
    token->type = type;
    token->start = start;
    token->end = start + 1;
}
void push_2c(LexOutput* out, eToken type, const char* start)
{
    assert(out->tokens_size != LexOutput::MAX_TOKENS); // out of memory
    Token* token = &out->tokens[out->tokens_size++];
    token->type = type;
    token->start = start;
    token->end = start + 2;
}
void push_num(LexOutput* out, uint64_t n, const char* start, const char* end)
{
    assert(out->tokens_size != LexOutput::MAX_TOKENS); // out of memory
    Token* token = &out->tokens[out->tokens_size++];
    token->type = eToken::constant_number;
    token->start = start;
    token->end = end;
    token->number = n;
}
Token* push_id(LexOutput* out, const char* start, const char* end)
{
    assert(out->tokens_size != LexOutput::MAX_TOKENS); // out of memory
    Token* token = &out->tokens[out->tokens_size++];
    token->type = eToken::identifier;
    token->start = start;
    token->end = end;
    token->identifier = strings_insert(start, end);
    return token;
}

bool lex(const LexInput& input, LexOutput& output)
{
    struct keyword
    {
        const char* nts;
        eToken token_id;
    };
#define KW(id) {strings_insert_nts(#id).nts, eToken::keyword_##id}
    const keyword keywords[] = {
        KW(int),
        KW(return),
        KW(if),
        KW(else),
        KW(for),
        KW(while),
        KW(do),
        KW(break),
        KW(continue)
    };
#undef KW

    // store length of output buffer and wipe it's length for return value
    // code below will write to and increment end_output
    const char* stream = input.stream;
    const char* end_stream = input.stream + input.length;

    while (stream < end_stream)
    {
        // skip whitespace
        if(iswhitespace(*stream))
        {
            ++stream;
            continue;
        }

        // handle two-char syntax: &&, ||, ==, !=, <=, >=
        if (stream + 1 < end_stream)
        {
            if (stream[0] == '&' && stream[1] == '&')
            {
                push_2c(&output, eToken::logical_and, stream);
                stream += 2;
                continue;
            }
            if (stream[0] == '|' && stream[1] == '|')
            {
                push_2c(&output, eToken::logical_or, stream);
                stream += 2;
                continue;
            }
            if (stream[0] == '=' && stream[1] == '=')
            {
                push_2c(&output, eToken::logical_equal, stream);
                stream += 2;
                continue;
            }
            if (stream[0] == '!' && stream[1] == '=')
            {
                push_2c(&output, eToken::logical_not_equal, stream);
                stream += 2;
                continue;
            }
            if (stream[0] == '<' && stream[1] == '=')
            {
                push_2c(&output, eToken::less_than_or_equal, stream);
                stream += 2;
                continue;
            }
            if (stream[0] == '>' && stream[1] == '=')
            {
                push_2c(&output, eToken::greater_than_or_equal, stream);
                stream += 2;
                continue;
            }
        }

        // handle one-char syntax: (){};
        switch (*stream)
        {
        case '!':
            push_1c(&output, eToken::logical_not, stream);
            ++stream;
            continue;
        case '%':
            push_1c(&output, eToken::mod, stream);
            ++stream;
            continue;
        case '&':
            push_1c(&output, eToken::bitwise_and, stream);
            ++stream;
            continue;
        case '(':
            push_1c(&output, eToken::open_parens, stream);
            ++stream;
            continue;
        case ')':
            push_1c(&output, eToken::closed_parens, stream);
            ++stream;
            continue;
        case '*':
            push_1c(&output, eToken::star, stream);
            ++stream;
            continue;
        case '+':
            push_1c(&output, eToken::plus, stream);
            ++stream;
            continue;
        case ',':
            push_1c(&output, eToken::comma, stream);
            ++stream;
            continue;
        case '-':
            push_1c(&output, eToken::dash, stream);
            ++stream;
            continue;
        case '/':
            push_1c(&output, eToken::forward_slash, stream);
            ++stream;
            continue;
        case ':':
            push_1c(&output, eToken::colon, stream);
            ++stream;
            continue;
        case ';':
            push_1c(&output, eToken::semicolon, stream);
            ++stream;
            continue;
        case '<':
            push_1c(&output, eToken::less_than, stream);
            ++stream;
            continue;
        case '=':
            push_1c(&output, eToken::assignment, stream);
            ++stream;
            continue;
        case '>':
            push_1c(&output, eToken::greater_than, stream);
            ++stream;
            continue;
        case '?':
            push_1c(&output, eToken::question_mark, stream);
            ++stream;
            continue;
        case '{':
            push_1c(&output, eToken::open_curly, stream);
            ++stream;
            continue;
        case '|':
            push_1c(&output, eToken::logical_or, stream);
            ++stream;
            continue;
        case '}':
            push_1c(&output, eToken::closed_curly, stream);
            ++stream;
            continue;
        case '~':
            push_1c(&output, eToken::bitwise_not, stream);
            ++stream;
            continue;
        }

        if (isnumber(*stream))
        {
            const char* num_start = stream;
            uint64_t number = uint64_t(*stream - '0');
            ++stream;

            while (stream < end_stream && isnumber(*stream))
            {
                number *= 10;
                number += *stream - '0';
                ++stream;
            }

            push_num(&output, number, num_start, stream);
            continue;
        }
        
        if(is_letter_or_underscore(*stream))
        {
            const char* token_end = stream + 1;
            while (token_end < end_stream && (is_letter_or_underscore(*token_end) || isnumber(*token_end)))
            {
                ++token_end;
            }

            Token* token = push_id(&output, stream, token_end);
            stream = token_end;

            for (int i = 0; i < _countof(keywords); ++i)
            {
                if (token->identifier.nts == keywords[i].nts)
                {
                    token->type = keywords[i].token_id;
                    break;
                }
            }

            continue;
        }

        output.failure_location = stream;
        output.failure_reason = "[lex] unsupported data in input";
        debug_break();
        return false;
    }

    return true;
}

void dump_lex(FILE* file, const LexOutput& lex)
{
    for(const Token* iter = lex.tokens; 
        iter != (lex.tokens + lex.tokens_size);
        ++iter)
    {
        const Token& token = *iter;
        switch (token.type)
        {
        case eToken::identifier:
            fwrite(token.identifier.nts, 1, token.identifier.len, file);
            continue;
        case eToken::constant_number:
            fprintf(file, "%" PRIu64, token.number);
            continue;
        case '!': fputc(token.type, file); continue;
        case '%': fputc(token.type, file); continue;
        case '&': fputc(token.type, file); continue;
        case '(': fputc(token.type, file); continue;
        case ')': fputc(token.type, file); continue;
        case '*': fputc(token.type, file); continue;
        case '+': fputc(token.type, file); continue;
        case ',': fputc(token.type, file); continue;
        case '-': fputc(token.type, file); continue;
        case '/': fputc(token.type, file); continue;
        case ':': fputc(token.type, file); continue;
        case ';': fputc(token.type, file); continue;
        case '<': fputc(token.type, file); continue;
        case '=': fputc(token.type, file); continue;
        case '>': fputc(token.type, file); continue;
        case '?': fputc(token.type, file); continue;
        case '{': fputc(token.type, file); continue;
        case '}': fputc(token.type, file); continue;
        case '~': fputc(token.type, file); continue;
        case eToken::logical_and: fprintf(file, "&&"); continue;
        case eToken::logical_or: fprintf(file, "||"); continue;
        case eToken::logical_equal: fprintf(file, "=="); continue;
        case eToken::logical_not_equal: fprintf(file, "!="); continue;
        case eToken::less_than_or_equal: fprintf(file, "<="); continue;
        case eToken::greater_than_or_equal: fprintf(file, ">="); continue;
        case eToken::keyword_int: fprintf(file, "int "); continue;
        case eToken::keyword_return: fprintf(file, "return "); continue;
        case eToken::keyword_if: fprintf(file, "if "); continue;
        case eToken::keyword_else: fprintf(file, "else "); continue;
        case eToken::keyword_for: fprintf(file, "for"); continue;
        case eToken::keyword_while: fprintf(file, "while"); continue;
        case eToken::keyword_do: fprintf(file, "do"); continue;
        case eToken::keyword_break: fprintf(file, "break"); continue;
        case eToken::keyword_continue: fprintf(file, "continue"); continue;
        }
        
        debug_break();
        break;// detected token we don't know how to handle
    }
}