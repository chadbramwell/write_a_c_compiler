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
void push_line_comment(LexOutput* out, const char** io_stream, const char* const end_stream)
{
    const char* stream = *io_stream;
    const char* const token_start = stream;
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
    token->start = token_start;
    token->end = stream;

    printf("REMOVING COMMENT FROM TOKEN STREAM. TODO: Fixup AST to handle comments?\n");
    --out->tokens_size;

    *io_stream = stream;
}
bool push_multiline_comment(LexOutput* out, const char** io_stream, const char* const end_stream)
{
    const char* stream = *io_stream;
    const char* const token_start = stream;
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
    token->start = token_start;
    token->end = stream;

    printf("REMOVING COMMENT FROM TOKEN STREAM. TODO: Fixup AST to handle comments?\n");
    --out->tokens_size;

    *io_stream = stream;
    return true;
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

bool lex(const LexInput* input, LexOutput* output)
{
    init_str_keywords();

    // store length of output buffer and wipe it's length for return value
    // code below will write to and increment end_output
    const char* stream = input->stream;
    const char* end_stream = input->stream + input->length;

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
            push_1c(output, eToken::logical_not, stream);
            ++stream;
            continue;
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
            push_num(output, value, token_start, stream);
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
        case eToken::comment: fprintf(file, "%.*s", (token.end - token.start), token.start); continue;
        }
        
        debug_break();
        break;// detected token we don't know how to handle
    }
}

void get_debug_data_from_file_offset(const LexInput* lex, const char* error_location, const char** o_line_start, const char** o_line_end, uint64_t* o_line_num)
{
    const char* const file_start = lex->stream;
    const char* const file_end = lex->stream + lex->length;

    // line_num & line_start
    uint64_t line_num = 0;
    const char* line_start = file_start;
    const char* line_end = file_end;
    while (line_start < error_location)
    {
        ++line_num;
        const char* new_line_start = line_start;
        while (new_line_start < file_end && *new_line_start != '\n')
            ++new_line_start;
        if (new_line_start < file_end) // we must be at '\n', skip it
            ++new_line_start;
        if (new_line_start < error_location)
        {
            line_start = new_line_start;
            continue;
        }

        line_end = new_line_start - 2;
        break;
    }

    *o_line_start = line_start;
    *o_line_end = line_end;
    *o_line_num = line_num;
}

void draw_error_caret_at(FILE* out, const LexInput* lex, const char* error_location, const char* error_reason)
{
    const char* line_start;
    const char* line_end;
    uint64_t line_num;
    get_debug_data_from_file_offset(lex, error_location, &line_start, &line_end, &line_num);

    uint64_t char_offset = uint64_t(error_location - line_start);
    
    fprintf(out, "%s:%" PRIu64 ":%" PRIu64 ": error: %s\n",
        lex->filename,
        line_num,
        char_offset,
        error_reason);
    fprintf(out, "%.*s\n",
        int(line_end - line_start),
        line_start);

    while (char_offset)
    {
        fputc(' ', out);
        --char_offset;
    }

    fputc('^', out);
    fputc('\n', out);
}
