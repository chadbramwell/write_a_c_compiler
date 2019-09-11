# [PRACTICE] Write a C Compiler and then try changing the language for the better.
See "++c" folder for my compiler, I'm
following: [Nora Sandler's Writing a C Compiler](https://norasandler.com/2017/11/29/Write-a-Compiler.html)

Stages to my compiler: *lex* -> *ast* -> *gen*

*gen* generates assembly (.s file) and clang converts that to binary. Eventually I'll add *bin* so I can skip clang directly.

Additional items written: *timer* (for perf timing), *debug* (for compile-time breakpoint & asserts), *dir* (for getting file names in a directory), *strings* (for compact storage of identifiers and pointer-comparison instead of full string-comparison), *interp* (an interpreter for my AST, to be used in real-time)

**Never stop learning!** 

-- Chad (@chad_bramwell)

## Random Thoughts:
* Lexing is compression.
* AST is pattern matching.
* I liked these lines from [Stage 6](https://norasandler.com/2018/02/25/Write-a-Compiler-6.html):
  * "an expression has a value, but a statement doesn’t"
  * "a statement can contain other statements, but an expression can’t contain statements"

## Timeline 
**Note: 'day' in descriptions below is more like a few hours of work**

* 8/5/2019 - **STAGE 1 TESTS PASS!** 
    * Took me ~3 days to get here: 1day for lex, 1day for ast, 1day for gen
* 8/7/2019 - **STAGE 2 TESTS PASS!**
    * Took me a few hours to go from STAGE 1 to STAGE 2.
* 8/10/2019 - STAGE 3 _INPROGRESS_
    * Lexing +/* took just a few minutes
    * Spent the rest of a few hours writing errors for AST stage
    * Spent a few minutes combining all virtual ASTNodes into a single node in prep for binary_ops (deleted a bunch of code!)
* 8/15/2019 - **STAGE 3 TESTS PASS!**
    * Life, newborn, etc... I think to go from 8/10 to here was only about a day of work.
    * So 2 days int total for STAGE 3.
    * Binary Op stuff as part of the AST was the most confusing. And I'm disheartened to say that I simply followed the ruleset instead of trying to really understand it.
* 8/19/2019 - STAGE 4 _INPROGRESS_
    * I've spent 2? days adding the ability to quickly test all files in a directory so I don't have to rely on the test_compiler.sh script. This provides me with the ability to immediately break on an error. Took so long due to hemming and hawing on "best" way to do it which later turned into "just do one of them because or none of them because you aren't working on the compiler which is what you really want to do."
* 8/26/2019 - **STAGE 4 TESTS PASS!**
   * I think this probably took another 2 days? I undid all my OOP work. I thought it'd help but it just made everything way more confusing and annoying. Now all data that I need is smashed into a single struct: ASTNode
   * Added more testing stuff like the ability to test full directories for lex and such.
* 8/29/2019 - STAGE 5 _INPROGRESS_
  * Lex didn't require any extra work for '=' (aka assignment operator).
  * AST complete but untested.
* 9/2/2019 - **STAGE 5 TESTS PASS!**
  * Added "strings" - a chunk of memory to store all identifiers and enable instant comparison of strings by comparing pointers. This doubled the speed of my compiler and all I did was replace my usage of std::string.
  * It's a bit janky and hacky and there's plenty of room for optimizations of the output assembly... but it works.
  * I'll say this took me ~3 days in total for Stage 5.
* 9/6/2019 - **STAGE 6 TESTS PASS!**
  * Took maybe 2 days?
  * if/else and ?: were quite similar
  * I think these were easier because the cmp/jmp assembly stuff had already been done in the past with binary operations.
  * Added -test option to test everything. TODO: actually test clang's compilation of prog and its return value to our prog's return value.
* 9/7/2019 - **STAGE 7 TESTS PASS!**
  * Took less than a day, relatively easy addition.
  * Note that my internal stack is limited (max of 256 vars on a stack frame, max of 256 stack frames) but that's fine for now.
  * Strangely, I ignored almost everything in https://norasandler.com/2018/03/14/Write-a-Compiler-7.html -> I think it's the first time an algorithm was described in depth and honestly the algo requirements seemed really simple to me. I also totally ignored the stuff on "deallocating variables on the stack" because there's no point. It's a waste of assembly and more complicated than it needs to be. Note that my feeling here is in stark contrast to the simplicity that every AST operation puts its value into %rax.
* 9/1/2019 - I wrote an interpreter for fun.
  * Spent 1 day just getting raw input from user to pipe to interpreter (this time included a rabbit hole for making a imgui app to show everything).
  * Spent 1 day making the interpreter support everything up to stage 7.
  * Added unary|number reduction 'cause I got tired of seeing it in my AST output. It's marked as an "optimization" which is in quotes because it's more a better version of Nora's example compiler then it is an actual optimization.

## Perf Status
```
Tests took 28893.22ms
Perf Results (average milliseconds)
        read file: 0.12ms
        lex: 0.10ms
        ast: 0.16ms
        gen: 1.88ms <--- Big WTF. Why do I take 2ms to generate ~100bytes of asm??
        clang: 122.65ms <--- system() to generate exe
        compare: 221.50ms <--- 3x system() calls. (clang .c, a.exe, my.exe)
        interp: 1.49ms <--- WOW. hmm... no clue why so slow, I should investigate.
```

## TODO (other than Stages)
* Eliminate clang depedency (by generating binary directly)
* LEARN: compare CL/Clang/GCC optimized assembly to my generated assembly
* Update Simplify to write out original source with modifications (currently writes out using ASTNodes which elimates all user formatting)
* ~~stringslab so I don't have to copy std::string around everywhere and so I can get back to using memset(0)~~ **see "strings.h"**
* Try new simplify rule: Variable elimination (ex: "int main(){int a = 2;return a;}" should become "int main(){return 2;}"
* Add warning for uninitialized variable
* Add equivalent warnings to what Clang produces:
```
../stage_4/valid/and_true.c:2:14: warning: use of logical '&&' with constant operand [-Wconstant-logical-operand]
    return 1 && -1;
             ^  ~~
../stage_4/valid/and_true.c:2:14: note: use '&' for a bitwise operation
    return 1 && -1;
             ^~
             &
../stage_4/valid/and_true.c:2:14: note: remove constant to silence this warning
    return 1 && -1;
            ~^~~~~
1 warning generated.
../stage_4/valid/precedence.c:2:19: warning: use of logical '&&' with constant operand [-Wconstant-logical-operand]
    return 1 || 0 && 2;
                  ^  ~
../stage_4/valid/precedence.c:2:19: note: use '&' for a bitwise operation
    return 1 || 0 && 2;
                  ^~
                  &
../stage_4/valid/precedence.c:2:19: note: remove constant to silence this warning
    return 1 || 0 && 2;
                 ~^~~~
1 warning generated.
../stage_5/valid/unused_exp.c:2:7: warning: expression result unused [-Wunused-value]
    2 + 2;
    ~ ^ ~
1 warning generated.
../stage_6/valid/statement/if_nested_3.c:6:9: warning: add explicit braces to avoid dangling else [-Wdangling-else]
        else
        ^
1 warning generated.
../stage_6/valid/statement/if_nested_4.c:6:9: warning: add explicit braces to avoid dangling else [-Wdangling-else]
        else
        ^
1 warning generated.
```

## Useful links:
* http://www.wilfred.me.uk/blog/2014/08/27/baby-steps-to-a-c-compiler/
* http://www.quut.com/c/ANSI-C-grammar-l-1999.html (lexing rules for C99)
* http://www.quut.com/c/ANSI-C-grammar-y-1999.html (hyperlinked grammar rules for C99)
* https://gist.github.com/codebrainz/2933703 (combined version of lex and yak files incase quut goes down). 
* https://github.com/Wilfred/babyc (most of the links above came from here)

## If I were to write a tutorial:
* Encourage usage of FILE* due to its versitility writing to stdout or an actual file.
* Focusing on a minimal program "int main(){return 2;}" is fantastic!
* Let the user know upfront that the way to test is to echo the return code of the program, not to look for a stdout print.
* Tutorial should also include binary generation. Skip the assembler entirely! Not sure the level of complexity there but an executable is just a file format that the OS knows to load into executable memory. It's just data.

## Open questions I have:
* I'm confident I can make the lexer work without memory allocations - use stack memory, do chunk-based iteration, strings are look-ups into source, all the same as my json parser. Caveat: Minimum required stack memory will be set to max allowable identifier size and I don't know if C places a limit on that. So here's my question, can the same be done for parsing and assembly generation? Could I pipeline the whole process without requiring memory allocation? Probably not, but it's a fun question to think about.
* Lexer could produce an identifier "int" or a token defining the keyword "int." It doesn't know which is which from context. C declares "int" is a reserved keyword. I don't see why this *MUST* but the case yet. I suspect it's not and it gets thrown onto the heap of "well we made specific decisions to make it easier for a compiler writer"

## See below for the original README.md which is a fork of https://github.com/nlsandler/write_a_c_compiler


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
