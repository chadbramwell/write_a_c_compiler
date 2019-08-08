# I'm trying my hand at making a compiler!
See "++c" folder. - Chad Bramwell

Stages to my compiler: *lex* -> *ast* -> *gen*

8/5/2019 - STAGE 1 TESTS PASS! (took me ~3 days to get here: 1day for lex, 1day for ast, 1day for gen, note that "day" is generous here. More like a few hours each night.)
8/7/2019 - STAGE 2 TESTS PASS! (took me a few hours today to go from STAGE 1 to STAGE 2)

Useful links:
http://www.wilfred.me.uk/blog/2014/08/27/baby-steps-to-a-c-compiler/
http://www.quut.com/c/ANSI-C-grammar-l-1999.html (lexing rules for C99)
http://www.quut.com/c/ANSI-C-grammar-y-1999.html (hyperlinked grammar rules for C99)
https://gist.github.com/codebrainz/2933703 (combined version of lex and yak files incase quut goes down). https://github.com/Wilfred/babyc (most of the links above came from here)

If I were to write a tutorial:
* keep it short and simple like http://www.wilfred.me.uk/blog/2014/08/27/baby-steps-to-a-c-compiler/ **BUT...**
* ...don't encourage usage of Flex/Bison. I wanted to learn how to bootstrap from scratch. (i.e. minimal/no depdencies)
* Focusing on a minimal program "int main(){return 2;}" is fantastic!
* Stages format from [this tutorial](https://norasandler.com/2017/11/29/Write-a-Compiler.html) appeals to me. Get something simple done, do the next thing. However the focus on "easier" languages like OCaml not so much.
* Tutorial should also include binary generation. Skip the assembler entirely! Not sure the level of complexity there but an executable is just a file format that the OS knows to load into executable memory. It's just data.

Random Thoughts:
* Lexing is compression.
* AST is pattern matching.

Open questions I have:
* I'm confident I can make the lexer work without memory allocations - use stack memory, do chunk-based iteration, strings are look-ups into source, all the same as my json parser. Caveat: Minimum required stack memory will be set to max allowable identifier size and I don't know if C places a limit on that. So here's my question, can the same be done for parsing and assembly generation? Could I pipeline the whole process without requiring memory allocation? Probably not, but it's a fun question to think about.
* Lexer could produce an identifier "int" or a token defining the keyword "int." It doesn't know which is which from context. C declares "int" is a reserved keyword. I don't see why this *MUST* but the case yet. I suspect it's not and it gets thrown onto the heap of "well we made specific decisions to make it easier for a compiler writer"

## See below for original markdown.
---
# Write a C Compiler!

This is a set of C test programs to help you write your own compiler. They were written to accompany [this tutorial](https://norasandler.com/2017/11/29/Write-a-Compiler.html).

## Usage

### test all
```
./test_compiler.sh /path/to/your/compiler
```

### test specific stages
To test stage 1 and stage 3,
```
./test_compiler.sh /path/to/your/compiler 1 3
```
To test from stage 1 to stage 6,
```
./test_compiler.sh /path/to/your/compiler `seq 1 6`
```

In order to use this script, your compiler needs to follow this spec:

1. It can be invoked from the command line, taking only a C source file as an argument, e.g.: `./YOUR_COMPILER /path/to/program.c`

2. When passed `program.c`, it generates executable `program` in the same directory.

3. It doesn’t generate assembly or an executable if parsing fails (this is what the test script checks for invalid test programs).

The script doesn’t check whether your compiler outputs sensible error messages, but you can use the invalid test programs to test that manually.

## Contribute

Additional test cases welcome! You can also file issues here, either about the test suite itself or about the content of the tutorial.
