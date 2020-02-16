/*
    Theory: When trying to parse the semantic structure of code there's certain structures which are more commonly used than others.
    Hypothesis: We should be able to detect those structures and determine probability of their occurance. Then, using the probabilities, code a more efficient parser.
    Supportive idea 1: Mike Acton and his various talks on understanding your data to write an efficient solution.
    Supportive idea 2: "The strongest modern lossless compressors use probabilistic models" - https://en.wikipedia.org/wiki/Data_compression#Lossless

    If I can codify the breadth of possible code and produce a useful probability related to that codification than I can write both a correct and efficient compiler.

    Attempt1: Focus on "termination" sequences and nesting. Non-goal: verify correctness of code. We are just trying to get useful signal here.
        code: #, i, c, o, ;, {}
        # - elements that begin with # like #define and #pragma (requires newline termination while considering \newline)
        i - short for identifier (includes types, no explicit termination)
        o - short for operator (no explicit termination)
        ; - semicolon (note: *sometimes* termination... think on the difference between for(;;) and statements...)
        {} - scope (closing brace is terminator)

    Q: what are the high-level things we care about? Perhaps orient codification towards that?
    
    Examples:
        * #include <stdio.h> -> # id < constant > (right-arrow termination)
        * int x = 3; -> id id = constant ; (semicolon termination)
        * int main() { ... } -> id id (){ <nest> } (close-curly termination)
        * printf("hi\n"); -> id ( constant ) ; (semicolon termination)
        * return 0; -> id constant ; (semicolon termination)
        global: # id 

*/
#include <dir.h>
#include <file.h>
#include <debug.h>
#include <vector>
#include <string>
#include <cstdio>

bool ignore_directory(const char* path) {
    if (0 == strcmp(".", path)) {
        return true;
    }
    if (0 == strcmp("..", path)) {
        return true;
    }
    return false;
}

bool ends_with(const char* path, const char* what) {
    const char* end_path = path;
    while (*end_path) ++end_path;
    --end_path;

    const char* end_what = what;
    while (*end_what) ++end_what;
    --end_what;

    while (end_path >= path && end_what >= what) {
        if (*end_path != *end_what) {
            return false;
        }
        --end_path;
        --end_what;
    }
    return end_what < what; // we checked all the things
}

bool include_file(const char* path) {
    if (ends_with(path, ".h")) {
        return true;
    }
    if (ends_with(path, ".c")) {
        return true;
    }
    if (ends_with(path, ".cpp")) {
        return true;
    }
    return false;
}

// todo: investigate how Everything does this. presumably looking at NTFS tables? inodes on windows? iterating by name is just silly.
bool find_all_files_recursive(const char* root, std::vector<std::string>* o_files) {

    std::vector<std::string> files;
    std::vector<std::string> directories;
    directories.push_back(root);

    DirectoryIter* diter;

    do
    {
        std::string dir_path = directories.back();
        dir_path += '/'; // hack fix for broken dopen code, directories must end in a slash
        directories.pop_back();
        if (!dopen(&diter, dir_path.c_str())) {
            printf("failed to open directory %s\n", dir_path.c_str());
            debug_break();
            return false;
        }

        do
        {
            if (disdir(diter)) {
                const char* new_dir_name = dfname(diter);
                const char* new_dir_path = dfpath(diter);
                if (ignore_directory(new_dir_name)) {
                    continue;
                }
                directories.push_back(new_dir_path);
                continue;
            }

            const char* new_file_name = dfname(diter);
            const char* new_file_path = dfpath(diter);
            if (include_file(new_file_name)) {
                files.push_back(new_file_path);
            }

        } while (dnext(diter));
        dclose(&diter);

    } while (!directories.empty());

    *o_files = std::move(files);
    return true;
}

bool load_and_compact_files(const std::vector<std::string>& file_paths, std::vector<std::string>* o_file_contents) {

    std::vector<std::string> file_contents;
    size_t temp_buffer_size = 0;
    char* temp_buffer = nullptr;

    for (size_t i = 0; i < file_paths.size(); ++i) {

        size_t file_size = 0;
        if (!file_read_into_stretchy_memory(file_paths[i].c_str(), &file_size, &temp_buffer, &temp_buffer_size)) {
            printf("failed to read %s into memory!\n", file_paths[i].c_str());
            debug_break();
            return false;
        }

/*From the C11 draft: http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
5.1.1.2 Translation phases
The precedence among the syntax rules of translation is specified by the 
following phases. Implementations shall behave as if these separate phases 
occur, even though many are typically folded together in practice. Source 
files, translation units, and translated translation units need not necessarily
be stored as files, nor need there be any one-to-one correspondence between 
these entities and any external representation. The description is conceptual 
only, and does not specify any particular implementation.

1. Physical source file multibyte characters are mapped, in an implementation-
defined manner, to the source character set (introducing new-line characters
for end-of-line indicators) if necessary. Trigraph sequences are replaced by
corresponding single-character internal representations.
*/

// --> IGNORED. Assuming files are ASCII and no Trigraphs.

/*
2. Each instance of a backslash character (\) immediately followed by a new-
line character is deleted, splicing physical source lines to form logical 
source lines. Only the last backslash on any physical source line shall be 
eligible for being part of such a splice. A source file that is not empty 
shall end in a new-line character, which shall not be immediately preceded by a
backslash character before any such splicing takes place. */

// --> Important for #define. Can also impact comments. Low probability in src.
        {
            size_t buff_left = file_size - 2;
            char* reader = (char*)memchr(temp_buffer, '\\', buff_left);
            char* writer = reader;
            while (reader && buff_left > 2) {
                if (reader[1] == '\n') {
                    reader += 1; // normally +2, but below memchr(reader+1,...)
                    buff_left -= 2;
                }
                else if (reader[1] == '\r' && reader[2] == '\n') {
                    reader += 2; // normally +3, but below memchr(reader+1,...)
                    buff_left -= 3;
                }

                if (writer == reader) {
                    reader = (char*)memchr(reader + 1, '\\', buff_left);
                    buff_left = reader ? (reader - writer) : 0;
                    writer = reader;
                    continue;
                }

                char* pivot = reader;
                reader = (char*)memchr(reader + 1, '\\', buff_left);
                char* pivot_end = reader ? reader : temp_buffer + file_size;
                size_t mm_total = (pivot_end - pivot);
                memmove(writer, pivot, mm_total);
                buff_left -= mm_total;
                writer = pivot_end;
            }
            if (!reader) {
                assert(writer[buff_left]);
            }
            assert(reader + 2 == temp_buffer + file_size);
            assert(buff_left <= 2);
            for (size_t i = 0; i < buff_left + 2; ++i) {
                assert(writer[i] != '\\' && "TODO Handle edge case");
            }
            writer += 2;
            file_size = (writer - temp_buffer);
        }

/*
3. The source file is decomposed into preprocessing tokens (White-space 
characters separating tokens are no longer significant. Each preprocessing 
token is converted into a token. The resulting tokens are syntactically and 
semantically analyzed and translated as a translation unit.) and sequences of 
white-space characters (including comments). A source file shall not end in a
partial preprocessing token or in a partial comment. Each comment is replaced 
by one space character. New-line characters are retained. Whether each nonempty
sequence of white-space characters other than new-line is retained or replaced
by one space character is implementation-defined.

4. Preprocessing directives are executed, macro invocations are expanded, and
_Pragma unary operator expressions are executed. If a character sequence that
matches the syntax of a universal character name is produced by token
concatenation (6.10.3.3), the behavior is undefined. A #include preprocessing
directive causes the named header or source file to be processed from phase 1
through phase 4, recursively. All preprocessing directives are then deleted.

5. Each source character set member and escape sequence in character constants 
and string literals is converted to the corresponding member of the execution 
character set; if there is no corresponding member, it is converted to an 
implementation-defined member other than the null (wide) character. An 
implementation need not convert all non-corresponding source characters to the
same execution character.

6. Adjacent string literal tokens are concatenated.

7. White-space characters separating tokens are no longer significant. Each
preprocessing token is converted into a token. The resulting tokens are
syntactically and semantically analyzed and translated as a translation unit.

8. All external object and function references are resolved. Library 
components are linked to satisfy external references to functions and objects 
not defined in the current translation. All such translator output is 
collected into a program image which contains information needed for execution
in its execution environment.
*/

        // And done!
        file_contents.push_back(std::string(temp_buffer, file_size));
    }

    free(temp_buffer);
    
    *o_file_contents = std::move(file_contents);
    return true;
}

int main(int argc, char** argv) {

    if (argc != 2) {
        printf("expected 1 argument: directory to start from\n");
        return 1;
    }

    std::vector<std::string> file_paths;
    bool ok = find_all_files_recursive(argv[1], &file_paths);
    if (!ok) {
        debug_break();
        return 1;
    }

    std::vector<std::string> file_contents;
    ok = load_and_compact_files(file_paths, &file_contents);
    if (!ok) {
        debug_break();
        return 1;
    }

    return 0;
}