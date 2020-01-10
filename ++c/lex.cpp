#include "lex.h"
#include "debug.h"
#include "strings.h"
#include <string.h>//memcmp
#include <stdlib.h>//malloc

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
Token* alloc_token(LexOutput* out)
{
    out->tokens = (Token*)realloc(out->tokens, (out->num_tokens + 1) * sizeof(Token));
    assert(out->tokens);
    return &out->tokens[out->num_tokens++];
}
void push_1c(LexOutput* out, eToken type, const char* start)
{
    Token* token = alloc_token(out);
    token->type = type;
    token->location.start = start;
    token->location.end = start + 1;
}
void push_2c(LexOutput* out, eToken type, const char* start)
{
    Token* token = alloc_token(out);
    token->type = type;
    token->location.start = start;
    token->location.end = start + 2;
}
void push_num(LexOutput* out, uint64_t n, const char* start, const char* end)
{
    Token* token = alloc_token(out);
    token->type = eToken::constant_number;
    token->number = n;
    token->location.start = start;
    token->location.end = end;
}
void push_string(LexOutput* out, const char* start, const char* end)
{
    Token* token = alloc_token(out);
    token->type = eToken::string;
    token->location.start = start;
    token->location.end = end;
    token->str.start = start + 1; // skip starting "
    token->str.end = end - 1; // set end at ending ", end should not be read only one before it
}
void push_id_or_keyword(LexOutput* out, str id, const char* start, const char* end)
{
    Token* token = alloc_token(out);
    token->type = eToken::identifier;
    token->location.start = start;
    token->location.end = end;
    token->identifier = id;

    // cool warning from MSVC from previous code:
    // for "io_token->type == eToken::keyword_int;" when I intended an assignment
    // 1>c:\users\chad\desktop\practice\write_a_c_compiler\++c\lex.cpp(58): warning C4553: '==': result of expression not used; did you intend '='?

    // Try to convert identifier to keyword. Keywords in C are reserved so we can do this at lex-time.
    // TODO: verify this is still valid once we have implemented #define.
    if (id.nts == kStrVoid.nts) token->type = eToken::keyword_void;
    else if (id.nts == kStrInt.nts) token->type = eToken::keyword_int;
    else if (id.nts == kStrReturn.nts) token->type = eToken::keyword_return;
    else if (id.nts == kStrIf.nts) token->type = eToken::keyword_if;
    else if (id.nts == kStrElse.nts) token->type = eToken::keyword_else;
    else if (id.nts == kStrFor.nts) token->type = eToken::keyword_for;
    else if (id.nts == kStrWhile.nts) token->type = eToken::keyword_while;
    else if (id.nts == kStrDo.nts) token->type = eToken::keyword_do;
    else if (id.nts == kStrBreak.nts) token->type = eToken::keyword_break;
    else if (id.nts == kStrContinue.nts) token->type = eToken::keyword_continue;
}
uint64_t push_line_comment(LexOutput* out, const char* const io_stream, const char* const end_stream)
{
    const char* stream = io_stream;
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

    Token* token = alloc_token(out);
    token->type = eToken::comment;
    token->location.start = io_stream;
    token->location.end = stream;

    return stream - io_stream;
}
uint64_t push_multiline_comment(LexOutput* out, const char* const io_stream, const char* const end_stream)
{
    const char* stream = io_stream;
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
        return 0;
    
    stream += 2; // skip past */

    Token* token = alloc_token(out);
    token->type = eToken::comment;
    token->location.start = io_stream;
    token->location.end = stream;

    return stream - io_stream;
}
//uint64_t handle_directive(LexOutput* /*output*/, const char* const io_stream, const char* const end_stream)
//{
//    // HACK: this isn't correct.
//    const char* stream = io_stream;
//    while (*stream != '\n')
//        ++stream;
//    return stream - io_stream;
//}
str_slice convert_string(LexOutput* out, const char* start, const char* end)
{
    // space for string allocated in table out->all_strings
    // does the work to handle backslash-newlines, \n, \t, etc.. in strings
    if (end - start < 512)
    {
        debug_break();
        return str_slice();
    }
    char temp[512];
    char* write = temp;
    const char* read = start;
    while (read < end)
    {
        //if (*read != '\\')
        {
            *write++ = *read++;
            continue;
        }

    }

    if (read < end && *read == '\\')
    {
        debug_break();
        return str_slice();
    }

    uint64_t entry_len = write - temp;
    uint64_t table_len = out->strings_end - out->strings;
    out->strings = (char*)realloc(out->strings, table_len + entry_len);

    memcpy(out->strings_end, temp, entry_len);
    out->strings_end = out->strings + table_len + entry_len; // NOTE: we re-offset from out->strings because realloc could change the base pointer

    str_slice ret;
    ret.start = out->strings + table_len;
    ret.end = ret.start + entry_len;

    return ret;
}

LexInput init_lex(const char* filename, const char* filedata, uint64_t filelen)
{
    return LexInput{ filename, filedata, filelen };
}

bool lex(const LexInput* input, LexOutput* output)
{
    assert(output->tokens == NULL); // caller must init to 0 LexOutput
    init_str_keywords();

    const char* stream = input->stream;
    const char* end_stream = stream + input->length;

    // FIRST PASS: Copy and perform early Translation Phase steps (5.1.1.2 of http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf).
    //char* stream = (char*)malloc((size_t)input->length);
    //char* end_stream = stream + input->length;
    //{
    //    // TODO: pass
    //    memcpy(stream, input->stream, (size_t)input->length);
    //}

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
                push_2c(output, eToken::logical_and, stream);
                stream += 2;
                continue;
            }
            if (stream[0] == '|' && stream[1] == '|')
            {
                push_2c(output, eToken::logical_or, stream);
                stream += 2;
                continue;
            }
            if (stream[0] == '=' && stream[1] == '=')
            {
                push_2c(output, eToken::logical_equal, stream);
                stream += 2;
                continue;
            }
            if (stream[0] == '!' && stream[1] == '=')
            {
                push_2c(output, eToken::logical_not_equal, stream);
                stream += 2;
                continue;
            }
            if (stream[0] == '<' && stream[1] == '=')
            {
                push_2c(output, eToken::less_than_or_equal, stream);
                stream += 2;
                continue;
            }
            if (stream[0] == '>' && stream[1] == '=')
            {
                push_2c(output, eToken::greater_than_or_equal, stream);
                stream += 2;
                continue;
            }
            if (stream[0] == '/' && stream[1] == '/')
            {
                stream += push_line_comment(output, stream, end_stream);
                continue;
            }
            if (stream[0] == '/' && stream[1] == '*')
            {
                uint64_t offset = push_multiline_comment(output, stream, end_stream);
                if (offset == 0)
                {
                    output->failure_location = stream;
                    output->failure_reason = "[lex] failed to find end of multi-line comment";
                    debug_break();
                    return false;
                }
                stream += offset;
                continue;
            }
        }

        // handle one-char syntax: (){};
        switch (*stream)
        {
        case '!':
            push_1c(output, eToken::logical_not, stream);
            ++stream;
            continue;
        //case '#':
        //{
        //    uint64_t offset = handle_directive(output, stream, end_stream);
        //    if (offset == 0)
        //    {
        //        debug_break();
        //        return false;
        //    }
        //    stream += offset;
        //    continue;
        //}
        case '%':
            push_1c(output, eToken::mod, stream);
            ++stream;
            continue;
        case '&':
            push_1c(output, eToken::bitwise_and, stream);
            ++stream;
            continue;
        case '(':
            push_1c(output, eToken::open_parens, stream);
            ++stream;
            continue;
        case ')':
            push_1c(output, eToken::closed_parens, stream);
            ++stream;
            continue;
        case '*':
            push_1c(output, eToken::star, stream);
            ++stream;
            continue;
        case '+':
            push_1c(output, eToken::plus, stream);
            ++stream;
            continue;
        case ',':
            push_1c(output, eToken::comma, stream);
            ++stream;
            continue;
        case '-':
            push_1c(output, eToken::dash, stream);
            ++stream;
            continue;
        case '/':
            push_1c(output, eToken::forward_slash, stream);
            ++stream;
            continue;
        case ':':
            push_1c(output, eToken::colon, stream);
            ++stream;
            continue;
        case ';':
            push_1c(output, eToken::semicolon, stream);
            ++stream;
            continue;
        case '<':
            push_1c(output, eToken::less_than, stream);
            ++stream;
            continue;
        case '=':
            push_1c(output, eToken::assignment, stream);
            ++stream;
            continue;
        case '>':
            push_1c(output, eToken::greater_than, stream);
            ++stream;
            continue;
        case '?':
            push_1c(output, eToken::question_mark, stream);
            ++stream;
            continue;
        case '{':
            push_1c(output, eToken::open_curly, stream);
            ++stream;
            continue;
        case '|':
            push_1c(output, eToken::logical_or, stream);
            ++stream;
            continue;
        case '}':
            push_1c(output, eToken::closed_curly, stream);
            ++stream;
            continue;
        case '~':
            push_1c(output, eToken::bitwise_not, stream);
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

            push_num(output, number, num_start, stream);
            continue;
        }
        
        if(is_letter_or_underscore(*stream))
        {
            const char* start = stream;
            char id_temp[256];
            const char* const ID_MAX = id_temp + 256;
            char* id_iter = id_temp;

            *id_iter++ = *stream++;
            while (stream < end_stream)
            {
                if (is_letter_or_underscore(*stream) || isnumber(*stream))
                {
                    *id_iter++ = *stream++;
                    if (id_iter == ID_MAX)
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

            str id = strings_insert(id_temp, id_iter);
            push_id_or_keyword(output, id, start, stream);
            continue;
        }

        // single-quote strings
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
            push_num(output, value, token_start, stream);
            continue;
        }

        // double-quote strings
        if (*stream == '\"')
        {
            const char* string_start = stream++;
            while (stream != end_stream && *stream != '\"')
            {
                // handle \"
                if (*stream == '\\' && stream + 1 < end_stream && stream[1] == '\"')
                {
                    stream += 2;
                    continue;
                }
                ++stream;
            }
            if (*stream != '\"')
            {
                output->failure_location = stream;
                output->failure_reason = "[lex] missing end of string.";
                return false;
            }
            ++stream;
            push_string(output, string_start, stream);
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
    const Token* const token_end = lex->tokens + lex->num_tokens;
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
