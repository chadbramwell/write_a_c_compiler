# [PRACTICE] Write a C Compiler and then try changing the language for the better.
See "++c" folder for my compiler, I'm
following: [Nora Sandler's Writing a C Compiler](https://norasandler.com/2017/11/29/Write-a-Compiler.html)

Stages to my compiler: *lex* -> *ast* -> *gen*

*gen* generates assembly (.s file) and clang converts that to binary. Eventually I'll add *bin* so I can skip clang directly.

Additional items written: *timer* (for perf timing), *debug* (for compile-time breakpoint & asserts), *dir* (for getting file names in a directory), *strings* (for compact storage of identifiers and pointer-comparison instead of full string-comparison), *interp* (an interpreter for my AST, to be used in real-time)


C support:
* generates x64 asm
* types: int (8 bytes)
* supported unary op: -
* supported binary op: +,-,/,*,%
* supported terop: ?:
* supported logical ops: <,>,<=,>=,||,&&,!=
* if/else (body must be a single statement)
* loops: for, while, do-while, break, continue, return
* vars and scoping
* functions (max 4 params)

Milestones:
* "Hello, World!" - using putchar(int)
* recursive fibonacci - output is final return value of main

**Never stop learning!** 

-- Chad (@chad_bramwell)

## Random Thoughts:
* Lexing is compression.
* AST is pattern matching.
* I liked these lines from [Stage 6](https://norasandler.com/2018/02/25/Write-a-Compiler-6.html):
  * "an expression has a value, but a statement doesn’t"
  * "a statement can contain other statements, but an expression can’t contain statements"

## x64 Windows Cheat Sheet
Because it's important for function calls and is important info when setting up local variables.
And not explained in Nora Sandler's 'Write a Compiler' series.
Reference: [Calling Convention](https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention), [Register Volitility](https://docs.microsoft.com/en-us/cpp/build/x64-software-conventions?#register-usage)
* Calling convention is NOT cdecl, even for standard library calls.
* Calling convention is fastcall.
* Integer arguments are passed through **rcx**, **rdx**, **r8**, **r9** (in that order)
* If a funcation takes more than 4 arguments than it "spills" onto the stack.
* "call" implicitly pushes return address onto the stack and then jumps
* "ret" implicitly pops the return address off the stack and jumps to it. It's important that you've wiped out any allocations on the stack prior to "ret"
* stack situations:
  * start of main (no need to save rbp/rsp but do need to make room for 32 byte "shadow stack" on windows)
  * end of main (remove "shadow stack" and all local vars)
  * early return from main (same as above, perhaps jump to a cleanup section?)
  * start of func (save rbp/rsp, 32 byte "shadow "stack")
  * end of func (restore stack then restore rbp/rsp)
  * return early from func (same as above, perhaps jump to a cleanup section?)



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
* 9/11/2019 - I wrote an interpreter for fun. And added perf stats to target optimizations.
  * Spent 1 day just getting raw input from user to pipe to interpreter (this time included a rabbit hole for making a imgui app to show everything).
  * Spent 1 day making the interpreter support everything up to stage 7.
  * Added unary|number reduction 'cause I got tired of seeing it in my AST output. It's marked as an "optimization" which is in quotes because it's more a better version of Nora's example compiler then it is an actual optimization.
  * Spent 1 day adding perf stats and optimizing lex (mostly by eliminating C++ stuff - hidden ctor/dtors and memory allocations from std::vector)
* 9/13/2019 (just after midnight so sort of 9/12?) - STAGE 8 _INPROGRESS_
  * Spent 1 day.
  * Added support for mod (%). Interp works for %, for, while, and do-while loops. Gen just about done.
  * Stage 8 is the first stage where I've deviated from grammer tree == ast function graph. Well, I've never liked this approach anyways so... :P
* 9/13/2019 - **STAGE 8 TESTS PASS!**
  * Took about a "day."
  * We support for, while, and do-while looping! (as well as break/continue/return inside them!)
  * I'm really starting to hate my code. It might be about time for a refactor.
  * Things that have bit me that need to change:
    * assumed order of parameters from AST to GEN/INTERP. This mostly came into play when writing the code for a new section by copying and pasting another section. (ex: while vs do-while. swapped condition and body but forgot to swap indicies into array)
    * ASM generated is really verbose and wasteful. I should take a hard look at redoing this generation. The output of the simplified "use rax for everything" has been frustrating to read the asm when I have bugs.
    * AST Nodes are simply new'ed and never tracked/freed. This could be faster and easier to reclaim memory if I wrote a slab allocator. I also setup nodes on the stack prior to copying to a new'ed chunk of memory with the (most likely misguided) thinking that stack + copying would be faster than allocating and writing to.
* 9/19/2019 - STAGE 9 _INPROGRESS_
  * Lex of comma done but need to get params/args and such working in AST.
  * Took a tangent to rewrite my ASTNode data structure. Now it's a union of all my types and we no longer depend on std::vector.
* 10/4/2019 - STAGE 9 _INPROGRESS_
  * Boy oh boy has this stage been a PITA. Ways to improve it:
    * Make a callout to Window's [calling convention](https://www.agner.org/optimize/calling_conventions.pdf) (hint: it's NOT cdecl)
    * I ignored Stage7's statement about "Deallocating Variables" and honestly, it made no difference until this stage. It would've been nice to reiterate the importance of deallocating variables on STAGE 9 because it won't be properly tested until this stage.
  * I'm probably at least 4 "days" into this... Spent most of time my having Clang generate assembly and comparing to what I've generated to try and understand where I've gone wrong.
  * Building my test suite into my compiler and adding command-line options to quickly test a single file or a particular stage has been invaluable in saving time.
  * It has been really useful to have Visual Studio debug my binaries (I drop "int 3" into the asm at the start of main so I can quickly debug it). I do dump all my asm and even clang's asm for comparison but it's a lot of text to absorb and parse, pulling it up in a debugger and stepping through values helps me track down my issue faster.
  * Learnings: stage_9/valid/fib.c has UB (no return at end of func). That broke my compiler because I don't have a way currently to output warnings. Currently this behavior is allowed so if that case happens, it's going to be a hard time debugging it.
  * My code was going crazy complicated trying to repair the stack in all the permutations of main/not-main/return-at-end/return-early. Now every function dumps a label at end for cleanup and all return does is jump to that label after putting its value into rax.
  * Got a new problem. [Deallocating local variables](https://norasandler.com/2018/03/14/Write-a-Compiler-7.html#deallocating-variables)... ugh. Now I got to write a scoping mechanism for vars...
  * I guess I have lots more reading to do. Windows calling conventions are nuts. https://www.gamasutra.com/view/news/178446/Indepth_Windows_x64_ABI_Stack_frames.php
  * Perhaps the advice to to sub/add rsp for every local var is not the right approach for windows. The article above implies that rsp should only change with _alloca.
  * What I don't know: When do I need to store rbp and push it? So far the simple programs I've come up with and thrown at clang don't do that at all.
* 10/9/2019 - STAGE 9 _INPROGRESS_
  * Yep. Still working on this. My last issue was because we set local vars as offsets from rbp but main has rbp set to 0 and accessing an offset from 0 is a violation. There's definately a gap in my knowledge here about why rbp is 0 on start and why Nora has suggested offsets from ebp. Maybe it's an x64 difference? From what I can tell, clang does everything as offsets from rsp... Looks like one fixed stack allocation for all necessary local variables at start of func and a single cleanup at end. I believe this includes space for all local variables as if every single local var was hoisted out of its scope to the function scope.
  * I fixed the rbp issue by always copying rsp to rbp at the beginning of a function (including main)
  * I have some new issue that I don't know the answer to yet. Looks like a stack overflow with fib.c
* 10/15/2019 - **STAGE 9 TESTS PASS!**
  * It's been a while. I'm on paternity leave and constantly exhausted. It's been really hard to find time and mental energy to work on this. But today I finally got in a few hours!
  * Reworked a bunch of stuff. AST_var now has a pointer to the AST_var that declared the variable (or itself). This was helpful in the asm gen phase so I could lookup the stack offset of every *potential* variable used in a function. Stack preallocates all *potential* variables on the stack. Inspiration for this came from studying Clang's output.
  * Added support for 4 param function calls. With x64 that means rcx, rdx, r8, r9. Supporting more than 4 requires "spilling into the stack" and I don't care to learn that and do it... yet.
  * Nora Sandler's tutorial has been awesome and I plan on finishing up Stage 10. HOWEVER, I'm pretty frustrated at overly-simplistic usage of rax and push/pop. Today is the day that I've abandoned that approach to move towards what I saw from Clang's: ignore rbp, preallocate all stack-space needed at start of func, variables are offsets from rsp. Fallout from this was that my binary operations, which required push/pop, didn't work well with this approach since they modify rsp. So now every binary op allocates storage for a temporary on the stack. While this made writing the asm generation take longer, in the end it is producing less assembly. The primary driver for this approach was the frustration with seeing this overly-verbose assembly when trying to debug issues.
  * I'm now wondering if this 3-step compiler where AST goes straight to ASM is the right approach. The frustration during ASM is having to manage the stack. I wonder if a better approach would be to write an IR that assumes infinite registers. My feeling is that it'd make passes (simplification/optimization) on the IR easier and that it'd make the ASM generation a lot more straight-forward.
  * Maybe I got something wrong with all this ASM generation. I did choose to go for x64 calling convention and that's where all my above issues seem to stem from.
* 10/22/2019 - **STAGE 10 TESTS PASS!**
  * That means we support global variables. And all stages complete that have been written by Nora Sandler. (mixed emotions here)
  * Took me a day.
  * Re-wrote how all variables are generated. They now generate a string as offset from rsp (ex: 32(%rsp)) or as an offset from rip for global vars (ex: my_global(%rip)).
* 10/25/2019 - **STAGE 10+ START** Going it on my own from here. 
  * (3 hours) Functions now support a return type of void.
* 10/28/2019
  * (two days?) rewrote some directory iteration code. cleanup_artifacts function
  * (1 hour) Added support for single-quote values. They are interpreted as uint64_t. i.e. 'A' gives back a value of 65. Now we can "putchar('H');" instead of "putchar(72);" like we did in stage_9's hello_world.c. NOTE: I'm on my own here, no clue if this is correct behavior. I'm assuming I'll add proper error-checking if a user attempts to stuff values into non-int types (once we support them).
* 10/29/2019
  * (1 hour) Adding support for // and /**/ comments. Currently I wipe them out of the token stream so AST doesn't have to skip over them. More ammo for my thinking that I need to produce an IR and do multiple passes on that IR instead of a single AST pass that tries to handle every case...

## Performance Status
A lot of work to be done optimizing. Some info: 
* [ast] allocates willy-nilly. 
* [gen]/[interp] do naive searches on stack frame. 
* [clang] does a system call to clang (not sure if slowness if from system() or clang itself)
* [compare] similar boat to [clang] but does 3 system() calls. (clang .c, a.exe, my.exe)

Numbers below in DEBUG build.
with STAGE 7 (9/11/2019):
```
Tests took 49773.24ms
Perf Results  [low,    high,   avg   ]
  read_file:  [0.06ms, 0.40ms, 0.11ms]
  lex:        [0.00ms, 0.03ms, 0.01ms]
  ast:        [0.02ms, 0.30ms, 0.09ms]
  gen_asm:    [0.05ms, 0.34ms, 0.08ms]
  gen_exe:    [121.03ms, 191.02ms, 134.48ms]
  run_exe:    [49.79ms, 84.86ms, 55.44ms]
  grnd_truth: [189.87ms, 447.82ms, 210.55ms]
  interp:     [0.00ms, 0.02ms, 0.01ms]
```
with STAGE 8 (9/13/2019):
```
Tests took 55717.29ms
Perf Results  [low,    high,   avg   ]
  read_file:  [0.06ms, 0.28ms, 0.10ms]
  lex:        [0.00ms, 0.02ms, 0.01ms]
  ast:        [0.02ms, 0.34ms, 0.11ms]
  gen_asm:    [0.05ms, 0.38ms, 0.10ms]
  gen_exe:    [113.70ms, 141.73ms, 121.99ms]
  run_exe:    [46.35ms, 78.04ms, 51.27ms]
  grnd_truth: [182.44ms, 804.20ms, 201.87ms]
  interp:     [0.00ms, 0.74ms, 0.03ms]
```
with STAGE 8 re-written with union of types instead of std::vector<> (9/19/2019):
```
Tests took 56345.18ms
Perf Results  [low,    high,   avg   ]
  read_file:  [0.06ms, 0.35ms, 0.11ms]
  lex:        [0.00ms, 0.06ms, 0.01ms]
  ast:        [0.01ms, 0.07ms, 0.01ms]
  gen_asm:    [0.05ms, 0.37ms, 0.10ms]
  gen_exe:    [111.79ms, 152.30ms, 125.46ms]
  run_exe:    [46.75ms, 66.98ms, 52.80ms]
  grnd_truth: [180.34ms, 770.66ms, 202.42ms]
  interp:     [0.00ms, 0.10ms, 0.01ms]
```
with STAGE 9 functions! (10/15/2019). There's a noticeable climb in ast. This is probably due to the extra pass I added to ast to link all variables back to their declarations:
```
Tests took 61768.23ms
Perf Results  [low,    high,   avg   ]
  read_file:  [0.07ms, 0.50ms, 0.12ms]
  lex:        [0.00ms, 0.11ms, 0.01ms]
  ast:        [0.01ms, 0.38ms, 0.02ms]
  gen_asm:    [0.03ms, 0.25ms, 0.08ms]
  gen_exe:    [112.36ms, 148.25ms, 120.56ms]
  run_exe:    [44.03ms, 60.20ms, 49.32ms]
  grnd_truth: [177.34ms, 255.13ms, 190.90ms]
  interp:     [0.00ms, 0.53ms, 0.01ms]
```
STAGE 10 complete! (10/22/2019) hooo boy that spike up in gen_asm. I'm guessing it's because I now calloc gen_ctx (with the shift to all static arrays, it was too big to keep on the stack).
```
Tests took 68609.15ms
Perf Results  [low,    high,   avg   ]
  read_file:  [0.08ms, 0.99ms, 0.14ms]
  lex:        [0.00ms, 0.06ms, 0.01ms]
  ast:        [0.01ms, 0.27ms, 0.02ms]
  gen_asm:    [1.09ms, 1.75ms, 1.26ms]
  gen_exe:    [114.47ms, 148.93ms, 124.48ms]
  run_exe:    [47.65ms, 59.73ms, 51.09ms]
  grnd_truth: [183.37ms, 712.46ms, 199.28ms]
  interp:     [0.00ms, 12.77ms, 0.14ms]
```
STAGE 10 optimization (put everything in gen_ctx back on the stack). Much better. :)
```
Tests took 68710.57ms
Perf Results  [low,    high,   avg   ]
  read_file:  [0.08ms, 0.45ms, 0.13ms]
  lex:        [0.00ms, 0.07ms, 0.01ms]
  ast:        [0.01ms, 0.35ms, 0.02ms]
  gen_asm:    [0.03ms, 0.39ms, 0.07ms]
  gen_exe:    [111.01ms, 157.71ms, 125.12ms]
  run_exe:    [45.72ms, 60.20ms, 50.85ms]
  grnd_truth: [182.53ms, 787.49ms, 200.46ms]
  interp:     [0.00ms, 8.36ms, 0.08ms]
```
STAGE 10+ refactors. Lex is probably high because I don't preallocate keywords for strings test, I'll try that next. I'm surprised ast went down. asm change has got to be noise.
```
Tests took 70748.13ms
Perf Results  [low,    high,   avg   ]
  read_file:  [0.08ms, 1.80ms, 0.16ms]
  lex:        [0.00ms, 0.22ms, 0.02ms]
  ast:        [0.01ms, 0.18ms, 0.02ms]
  gen_asm:    [0.03ms, 0.34ms, 0.07ms]
  gen_exe:    [117.62ms, 175.77ms, 128.77ms]
  run_exe:    [48.46ms, 67.46ms, 53.77ms]
  grnd_truth: [187.70ms, 407.30ms, 205.75ms]
  interp:     [0.00ms, 9.13ms, 0.10ms]
```
STAGE 10+ Dir refactor and support for cleaning up .ilk and .pdb. New perf display. This makes it apparent to me that I just need to dump my own exe 'cause that's multiple orders of magnitude differnce between generating asm and getting clang to convert that asm into a binary.
```
Tests took 77466.65ms
Perf Results  [samples,      total,        avg,        low,       high]
  read_file:  [    416,    65.37ms,     0.16ms,     0.08ms,     3.32ms]
  lex:        [    416,     3.54ms,     0.01ms,     0.00ms,     0.08ms]
  ast:        [    238,     5.45ms,     0.02ms,     0.01ms,     0.13ms]
  gen_asm:    [    119,     8.56ms,     0.07ms,     0.03ms,     0.36ms]
  gen_exe:    [    119, 16646.88ms,   139.89ms,   115.58ms,   168.23ms]
  run_exe:    [    119,  6838.62ms,    57.47ms,    47.71ms,    79.56ms]
  grnd_truth: [    238, 53288.00ms,   223.90ms,   183.48ms,   560.56ms]
  interp:     [    119,    12.12ms,     0.10ms,     0.00ms,     9.93ms]
  cleanup:    [     25,     2.22ms,     0.09ms,     0.05ms,     0.17ms]
Unaccounted for: 595.89ms
```

## TODO (other than Stages)
* Eliminate clang depedency (by generating binary directly)
* LEARN: compare CL/Clang/GCC optimized assembly to my generated assembly
* Update Simplify to write out original source with modifications (currently writes out using ASTNodes which elimates all user formatting)
* ~~stringslab so I don't have to copy std::string around everywhere and so I can get back to using memset(0)~~ **see "strings.h"**
* Paged Allocator for ASTNodes.
* Try new simplify rule: Variable elimination (ex: "int main(){int a = 2;return a;}" should become "int main(){return 2;}"
* Add warning for uninitialized variable
* Cleanup strings table when running multiple tests. (it never frees and probably has some impact on perf results)
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
* Update Tutorial to be a 4 stage process: Lex -> AST -> IR -> asm/binary/os-specific/arch-specific.
* IR would have "infinite" registers. In theory, this would make it easier to generate efficient binary even without optimizing IR. (I've been frustrated with the overly verbose output from Nora's eax usage for all operations and the extraneous push/pop for binary operations. I hope there's a good reason for it but at the moment the output asm is very verbose and difficult to debug/compare with clang). Also with an "infinite" set of registers, I presume it'd be easier for function calls and such where rcx, rdx, r8, and r9 are used first before spilling onto stack.
* IR would need some sort of "return" register.
* With IR, it'd also be easier to do an important step for x64 generation on windows: make room on the stack for everything a function needs up front.
* Note that Clang doesn't use rbp (from the small # of asm files I've inspected) instead it does offsets from rsp.

## Open questions I have:
* I'm confident I can make the lexer work without memory allocations - use stack memory, do chunk-based iteration, strings are look-ups into source, all the same as my json parser. Caveat: Minimum required stack memory will be set to max allowable identifier size and I don't know if C places a limit on that. So here's my question, can the same be done for parsing and assembly generation? Could I pipeline the whole process without requiring memory allocation? Probably not, but it's a fun question to think about.
* Lexer could produce an identifier "int" or a token defining the keyword "int." It doesn't know which is which from context. C declares "int" is a reserved keyword. I don't see why this *MUST* but the case yet. I suspect it's not and it gets thrown onto the heap of "well we made specific decisions to make it easier for a compiler writer"
* Could we skip the AST entirely? Certainly there's [examples](https://archive.org/details/dr_dobbs_journal_vol_05_201803/page/n193) of generating assembly while parsing C directly. Is the AST really an important transformation stage? Perhaps going straight to IR and validating that would be better?

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
