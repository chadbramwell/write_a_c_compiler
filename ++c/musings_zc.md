zc (Zero-Initialized C) - a thought-experiment on how to simplify C and empower developers.

The name comes from the first two pillars of zc:
 * All variables, unless otherwise specified, are initialized to 0 by default.
 * Zero undefined behavior
 * Fewer keywords than C

Problems with C that zc attacks:
 * header files
 * 44 keywords
 * ~200 undefined behaviors (ex: integer overflow)
 * ~150 implementation-defined behaviors (ex: number of bits in a byte)

(reference: http://www.iso-9899.info/wiki/The_Standard)

~~auto~~
break
case
char
const
continue
default
do
double
else
enum
~~extern~~ could be done a simpler way, no need with headers absent?
float
for
goto
if
~~inline~~ doesn't do anything in C, "hint" to compiler
int
long
~~register~~ not useful in C, except for embedding
~~restrict~~ should be part of 'compiler keywords' or something similar
return
short
signed
~~sizeof~~ should be part of 'compiler keywords' or something similar
static
struct
switch
typedef
union
unsigned
~~void~~ why type something that's meaningless?
~~volatile~~ doesn't do anything in C
while
~~_Alignas~~ should be part of 'compiler keywords' or something similar
_Alignof
_Atomic
~~_Bool~~ boolean values should have a non '__' type
~~_Complex~~ nope
~~_Generic~~ wtf is this?
~~_Imaginary~~ nope
~~_Noreturn~~ why???
~~_Static_assert~~ should be part of 'compiler keywords' or something similar
_Thread_local
