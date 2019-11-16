#include "lex.h"
#include "debug.h"
#include "strings.h"
#include <string.h>//memcmp
#include <stdlib.h>//malloc

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

void push_1c(LexOutput* out, eToken type)
{
    assert(out->tokens_size != LexOutput::MAX_TOKENS); // out of memory
    Token* token = &out->tokens[out->tokens_size++];
    token->type = type;
}
void push_2c(LexOutput* out, eToken type)
{
    assert(out->tokens_size != LexOutput::MAX_TOKENS); // out of memory
    Token* token = &out->tokens[out->tokens_size++];
    token->type = type;
}
void push_num(LexOutput* out, uint64_t n)
{
    assert(out->tokens_size != LexOutput::MAX_TOKENS); // out of memory
    Token* token = &out->tokens[out->tokens_size++];
    token->type = eToken::constant_number;
    token->number = n;
}
Token* push_id(LexOutput* out, const char* start, const char* end)
{
    assert(out->tokens_size != LexOutput::MAX_TOKENS); // out of memory
    Token* token = &out->tokens[out->tokens_size++];
    token->type = eToken::identifier;
    token->identifier = strings_insert(start, end);
    return token;
}
void push_line_comment(LexOutput* out, char** io_stream, const char* const end_stream)
{
    char* stream = *io_stream;
    stream += 2;

    while (stream != end_stream)
    {
        while (stream != end_stream && *stream != '\n')
            ++stream;

        if (stream != end_stream)
            ++stream; // consume '\n'

        // backslash at end of line means we are still "logically" on the same line according to the standard
        if (stream[-2] == '\\')
            continue;
        if (stream[-3] == '\\' && stream[-2] == '\r')
            continue;

        break;
    }

    assert(out->tokens_size != LexOutput::MAX_TOKENS); // out of memory
    Token* token = &out->tokens[out->tokens_size++];
    token->type = eToken::comment;

    printf("REMOVING COMMENT FROM TOKEN STREAM. TODO: Fixup AST to handle comments?\n");
    --out->tokens_size;

    *io_stream = stream;
}
bool push_multiline_comment(LexOutput* out, char** io_stream, const char* const end_stream)
{
    char* stream = *io_stream;
    stream += 2;

    bool found_end = false;
    while (stream + 1 < end_stream)
    {
        if (stream[0] == '*' && stream[1] == '/')
        {
            found_end = true;
            break;
        }
        ++stream;
    }

    if (!found_end)
        return false;
    
    stream += 2; // skip past */

    assert(out->tokens_size != LexOutput::MAX_TOKENS); // out of memory
    Token* token = &out->tokens[out->tokens_size++];
    token->type = eToken::comment;

    printf("REMOVING COMMENT FROM TOKEN STREAM. TODO: Fixup AST to handle comments?\n");
    --out->tokens_size;

    *io_stream = stream;
    return true;
}
bool handle_directive(char** io_stream, const char* end_stream, LexOutput* output)
{
    io_stream; end_stream; output;
    return false;
}

static str kStrVoid;
static str kStrInt;
static str kStrReturn;
static str kStrIf;
static str kStrElse;
static str kStrFor;
static str kStrWhile;
static str kStrDo;
static str kStrBreak;
static str kStrContinue;

void init_str_keywords()
{
    if (kStrVoid.nts)
        return; // Assume all other strings are also initilized

    kStrVoid = strings_insert_nts("void");
    kStrInt = strings_insert_nts("int");
    kStrReturn = strings_insert_nts("return");
    kStrIf = strings_insert_nts("if");
    kStrElse = strings_insert_nts("else");
    kStrFor = strings_insert_nts("for");
    kStrWhile = strings_insert_nts("while");
    kStrDo = strings_insert_nts("do");
    kStrBreak = strings_insert_nts("break");
    kStrContinue = strings_insert_nts("continue");
}

eToken try_resolve_keyword(str identifier)
{
    // cool warning from MSVC from previous code:
    // for "io_token->type == eToken::keyword_int;" when I intended an assignment
    // 1>c:\users\chad\desktop\practice\write_a_c_compiler\++c\lex.cpp(58): warning C4553: '==': result of expression not used; did you intend '='?

    if (identifier.nts == kStrVoid.nts) return eToken::keyword_void;
    else if (identifier.nts == kStrInt.nts) return eToken::keyword_int;
    else if (identifier.nts == kStrReturn.nts) return eToken::keyword_return;
    else if (identifier.nts == kStrIf.nts) return eToken::keyword_if;
    else if (identifier.nts == kStrElse.nts) return eToken::keyword_else;
    else if (identifier.nts == kStrFor.nts) return eToken::keyword_for;
    else if (identifier.nts == kStrWhile.nts) return eToken::keyword_while;
    else if (identifier.nts == kStrDo.nts) return eToken::keyword_do;
    else if (identifier.nts == kStrBreak.nts) return eToken::keyword_break;
    else if (identifier.nts == kStrContinue.nts) return eToken::keyword_continue;

    return eToken::identifier;
}

LexInput init_lex(const char* filename, const char* filedata, uint64_t filelen)
{
    return LexInput{ filename, filedata, filelen };
}

bool lex(const LexInput* input, LexOutput* output)
{
    init_str_keywords();

    // FIRST PASS: Copy and perform early Translation Phase steps (5.1.1.2 of http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf).
    char* stream = (char*)malloc((size_t)input->length);
    char* end_stream = stream + input->length;
    {
        // TODO: pass
        memcpy(stream, input->stream, (size_t)input->length);
    }

    while (stream < end_stream)
    {
        // skip whitespace
        if(iswhitespace(*stream))
        {
            ++stream;
            continue;
        }

        // skip \newline (logically concatenates the line with the next line)
        if (stream[0] == '\\')
        {
            if (stream + 1 == end_stream)
            {
                output->failure_location = stream;
                output->failure_reason = "[lex] line concatenation with ending \\ is not allowed at end of file";
                debug_break();
                return false;
            }

            if (stream[1] == '\r' || stream[1] == '\n')
            {
                stream += 2;
                continue;
            }
        }

        // handle two-char syntax: &&, ||, ==, !=, <=, >=
        if (stream + 1 < end_stream)
        {
            if (stream[0] == '&' && stream[1] == '&')
            {
                push_2c(output, eToken::logical_and);
                stream += 2;
                continue;
            }
            if (stream[0] == '|' && stream[1] == '|')
            {
                push_2c(output, eToken::logical_or);
                stream += 2;
                continue;
            }
            if (stream[0] == '=' && stream[1] == '=')
            {
                push_2c(output, eToken::logical_equal);
                stream += 2;
                continue;
            }
            if (stream[0] == '!' && stream[1] == '=')
            {
                push_2c(output, eToken::logical_not_equal);
                stream += 2;
                continue;
            }
            if (stream[0] == '<' && stream[1] == '=')
            {
                push_2c(output, eToken::less_than_or_equal);
                stream += 2;
                continue;
            }
            if (stream[0] == '>' && stream[1] == '=')
            {
                push_2c(output, eToken::greater_than_or_equal);
                stream += 2;
                continue;
            }
            if (stream[0] == '/' && stream[1] == '/')
            {
                push_line_comment(output, &stream, end_stream);
                continue;
            }
            if (stream[0] == '/' && stream[1] == '*')
            {
                if (push_multiline_comment(output, &stream, end_stream))
                    continue;
                output->failure_location = stream;
                output->failure_reason = "[lex] failed to find end of multi-line comment";
                debug_break();
                return false;
            }
        }

        // handle one-char syntax: (){};
        switch (*stream)
        {
        case '!':
            push_1c(output, eToken::logical_not);
            ++stream;
            continue;
        case '#':
            if (!handle_directive(&stream, end_stream, output)) // unlike other calls here, this one actually moves stream
            {
                debug_break();
                return false;
            }
            continue;
        case '%':
            push_1c(output, eToken::mod);
            ++stream;
            continue;
        case '&':
            push_1c(output, eToken::bitwise_and);
            ++stream;
            continue;
        case '(':
            push_1c(output, eToken::open_parens);
            ++stream;
            continue;
        case ')':
            push_1c(output, eToken::closed_parens);
            ++stream;
            continue;
        case '*':
            push_1c(output, eToken::star);
            ++stream;
            continue;
        case '+':
            push_1c(output, eToken::plus);
            ++stream;
            continue;
        case ',':
            push_1c(output, eToken::comma);
            ++stream;
            continue;
        case '-':
            push_1c(output, eToken::dash);
            ++stream;
            continue;
        case '/':
            push_1c(output, eToken::forward_slash);
            ++stream;
            continue;
        case ':':
            push_1c(output, eToken::colon);
            ++stream;
            continue;
        case ';':
            push_1c(output, eToken::semicolon);
            ++stream;
            continue;
        case '<':
            push_1c(output, eToken::less_than);
            ++stream;
            continue;
        case '=':
            push_1c(output, eToken::assignment);
            ++stream;
            continue;
        case '>':
            push_1c(output, eToken::greater_than);
            ++stream;
            continue;
        case '?':
            push_1c(output, eToken::question_mark);
            ++stream;
            continue;
        case '{':
            push_1c(output, eToken::open_curly);
            ++stream;
            continue;
        case '|':
            push_1c(output, eToken::logical_or);
            ++stream;
            continue;
        case '}':
            push_1c(output, eToken::closed_curly);
            ++stream;
            continue;
        case '~':
            push_1c(output, eToken::bitwise_not);
            ++stream;
            continue;
        }

        if (isnumber(*stream))
        {
            uint64_t number = uint64_t(*stream - '0');
            ++stream;

            while (stream < end_stream && isnumber(*stream))
            {
                number *= 10;
                number += *stream - '0';
                ++stream;
            }

            push_num(output, number);
            continue;
        }
        
        if(is_letter_or_underscore(*stream))
        {
            char id_temp[256];
            char* id_end = id_temp;

            *id_end++ = *stream++;
            while (stream < end_stream)
            {
                if (is_letter_or_underscore(*stream) || isnumber(*stream))
                {
                    *id_end++ = *stream++;
                    if (id_end == id_temp + 256)
                    {
                        output->failure_location = stream;
                        output->failure_reason = "[lex] max identifier size set to 256, ran out of space.";
                        return false;
                    }
                    continue;
                }

                if (stream[0] == '\\'
                    && stream + 3 < end_stream
                    && stream[1] == '\r'
                    && stream[2] == '\n')
                {
                    stream += 3;
                    continue;
                }

                if (stream[0] == '\\'
                    && stream + 1 < end_stream
                    && stream[1] == '\n')
                {
                    stream += 2;
                    continue;
                }
                    
                if(stream[0] == '\\')
                {
                    output->failure_location = stream;
                    output->failure_reason = "[lex] invalid character after \\, expected \\r and/or \\n";
                    return false;
                }

                break;
            }

            Token* token = push_id(output, id_temp, id_end);

            token->type = try_resolve_keyword(token->identifier);
            continue;
        }

        if (*stream == '\'')
        {
            const char* token_start = stream++;
            uint64_t value = 0;
            while (stream != end_stream && *stream != '\'' && (stream - token_start) <= 8)
            {
                unsigned char orvalue = *stream;

                // https://en.cppreference.com/w/cpp/language/escape
                if (*stream == '\\')
                {
                    ++stream;
                    assert(stream < end_stream);
                    switch (*stream)
                    {
                    case '\'': orvalue = 0x27; break; // single quote
                    case '"': orvalue = 0x22; break; // double quote
                    case '?': orvalue = 0x3f; break; // question mark
                    case '\\': orvalue = 0x5c; break; // backslash
                    case 'a': orvalue = 0x07; break; // audible bell
                    case 'b': orvalue = 0x08; break; // backspace
                    case 'f': orvalue = 0x0c; break; // form feed - new page
                    case 'n': orvalue = 0x0a; break; // line feed - new line
                    case 'r': orvalue = 0x0d; break; // carriage return
                    case 't': orvalue = 0x09; break; // horizontal tab
                    case 'v': orvalue = 0x0b; break; // vertical tab
                    default:
                        output->failure_location = stream;
                        output->failure_reason = "[lex] invalid or currently unhandled escape type in single quotes.";
                        return false;
                    }
                }

                value <<= 8;
                value |= orvalue;
                ++stream;
            }
            if (*stream != '\'')
            {
                output->failure_location = stream;
                output->failure_reason = "[lex] missing end of single quote. max length is 8 chars.";
                return false;
            }
            ++stream;
            push_num(output, value);
            continue;
        }

        output->failure_location = stream;
        output->failure_reason = "[lex] unsupported data in input";
        debug_break();
        return false;
    }

    return true;
}

void dump_lex(FILE* file, const LexOutput* lex)
{
    const Token* const token_end = lex->tokens + lex->tokens_size;
    for(const Token* iter = lex->tokens; iter != token_end; ++iter)
    {
        const Token token = *iter;
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
        case eToken::keyword_void: fprintf(file, "void "); continue;
        case eToken::keyword_int: fprintf(file, "int "); continue;
        case eToken::keyword_return: fprintf(file, "return "); continue;
        case eToken::keyword_if: fprintf(file, "if "); continue;
        case eToken::keyword_else: fprintf(file, "else "); continue;
        case eToken::keyword_for: fprintf(file, "for"); continue;
        case eToken::keyword_while: fprintf(file, "while"); continue;
        case eToken::keyword_do: fprintf(file, "do"); continue;
        case eToken::keyword_break: fprintf(file, "break"); continue;
        case eToken::keyword_continue: fprintf(file, "continue"); continue;
        case eToken::comment: fprintf(file, "// or /**/"); continue;
        //case eToken::include_path: fprintf(file, "#include \"%.*s\"", (token.path.end - token.path.start), token.path.start); continue;
        }
        
        debug_break();
        break;// detected token we don't know how to handle
    }
}
