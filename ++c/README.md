# ++c (todo: change name to zc)
(pronounced zee-cee or zero-c) - a programming-language/thought-experiment on how to simplify C and empower developers.

I like C a lot. It's C89 was reasonably simple but still had rough edges. I'd like to see how far I can push the language by removing things. It's easy to add, hard to remove.

However, there will be new things or more so, better defaults. I'll be modifying/changing/adding as necessary to remove UB (undefined behavior) and IDB (implementation defined behavior). For example, there's a massive number of ramifications through C due to the unknown size of char. char will be defined to 8 bits as is standard on all modern processors. Another area that will need changes are easy gotchas for programmers, the predominant one being to forget to initialize variables.

# language pillars
* Zero undefined behavior
* Zero implementation-defined behavior
* Default is to initialize to zero
* Every engineer shouldn't have to spend a lifetime learning new things about their langauge in order to work effectively

# ++c supported simplify rules
These include anything wrapped in nested parenthesis because those wash out when going from tokens to AST.
* ~~unary(-)[number] -> number (aka token reduction, makes further reductions more obvious)~~
  * This is now done automatically when generating AST
* binary(+)[number,number] -> number (do the addition for the user)

# ++c supported simplify rules I'm looking forward to implementing
* variable hoisting (moving a variable that's only read in a loop outside of the scope of the loop - this is a big reason why debug builds are slower than release builds)
* it'd be difficult but I'd love some system that could [Finish Your Derivations](https://fgiesen.wordpress.com/2010/10/21/finish-your-derivations-please/)

# ++c simplify algorithm
* for full AST, try to pattern match a simplify rule
  * write match found, store reduction location and value
* if no more matches found, done, otherwise repeat

# language challenges
C is a good starting point but it has ~200 undefined behaviors and ~150 implementation-defined behaviors. I have no doubt there are very good reasons for all of that but I don't know what they are. So this will be a path of learning and discovery.

# language hope
* Fewer keywords than C
* Remove allowance of missing return (confusing)
* Remove forward-decl requirement. (Broken, ex: can't forward-decl FILE)

# half-baked ideas
* @ keywords that can be used to add new keywords and reduce conflicts with existing keywords. Ex: @pure
* "pure" keyword that can be attached to functions. A pure function guarantees that it and none of the functions it calls touch any global state.
* Simpler syntax for data types. Instead of char/int/uint64_t it's i8/i32/u64 and beyond: users can define arbitrary bit size of their variables effectively combining two different pieces of C syntax into one. Bit manipulation code will be automatically generated for you.
* I'd love to get rid of header files but a good first step is to eliminate the requirement for forward-declaration. What's the point?
* "Debug builds are too slow" is total garbage. Engineers need to learn how to write better code. Compilers have a very large oppurtunity to actual improve the quality of engineers here. If a compiler could regurgatate the code it compiled but with very obvious and easy optimizations then an engineer could diff their source files against these to learn how to improve. The difference between debug and release builds could finally be focused on optimizations that would be too tedious for engineers to write.
* Compilers should have the freedom to reorder data as needed for optimizations. However, compliers also need to support better (compile-time) reflection systems for serialization.
* idiv calculates the division and remainder at the same time. A sneaky compiler might be able to optimize that if they detect "c=a/b and r=a%b" but I would think a simpler solution would be to repurpose comma "," for multiple return values. 
  * "d,r = a/b;" or 
  * "d = a/b;" or 
  * "_,r = a/b;" etc...
* dynamic array type, or mechanisms or whatever cause "t = (type*)realloc(t, num * sizeof(t));" is bananas
  * type[] t; // type[] is essentially a struct { type* data; uint32_t size; }
  * type* nt = t.alloc(1);
  * type* nts = t.alloc(10); //nts is pointed to start of next 10 elements
  * free(t.data); // users have direct access to underlying data structure
  * t.size = 0; // clear

(reference: http://www.iso-9899.info/wiki/The_Standard)

# C keywords and some thoughts
* ~~auto~~ 
  * Can be ignored by compiler and thus is unhelpful. (see register below) [6.7.2:123](https://web.archive.org/web/20181230041359if_/http://www.open-std.org/jtc1/sc22/wg14/www/abq/c17_updated_proposed_fdis.pdf) "The implementation may treat any **register** declaration simply as an **auto** declaration."
* break
* case
* char
  * char can be signed or unsigned depending on compiler settings (*facepalm*)
  * undefined size, should be defined to 8bits
* const
* continue
* default
* do
* double
* else
* enum
* ~~extern~~
  * needs more thought about API description through external linkage mechanisms
* float
* for
* goto
* if
* ~~inline~~ 
  * doesn't do anything in C, "hint" to compiler
* int
* long
* ~~register~~
  * Can be ignored by compiler and thus is unhelpful. [6.7.1.6](https://web.archive.org/web/20181230041359if_/http://www.open-std.org/jtc1/sc22/wg14/www/abq/c17_updated_proposed_fdis.pdf) "A declaration of an identifier for an object with storage-class specifier **register** suggests that
access to the object be as fast as possible. The extent to which such suggestions are effective is
implementation-defined."
* restrict
* return
* short
* ~~signed~~
  * come on, default to one or the other, don't add new keywords for both ways.
* sizeof
* static
* struct
* switch
* typedef
* union
* unsigned
* ~~void~~
  * why type something that's meaningless?
* ~~volatile~~
  * A compiler *could* use this keyword to disable optimizations around a variable but a compiler *may* also choose to ignore the qualifier entirely. It's undependable and not cross-compiler.
* while
* _Alignas
* _Alignof
* _Atomic
* ~~_Bool~~ 
  * bool. bit. just something. come-on, you gotta be willing to break things sometimes to make everything better.
* ~~_Complex~~ *how did this get added to the standard? I mean, honestly.*
* ~~_Generic~~ *wtf is this?*
* ~~_Imaginary~~ *same question as _Complex, why?*
* ~~_Noreturn~~ *why???*
* _Static_assert
* _Thread_local
