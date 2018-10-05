# Compiled Script
Parse and compile a prefix expression (source code) to a simple byte code.

## TODO

* [x] Add custom subroutines
* [x] Add value returns (I want to catch failure to return early, without having to define too much)
* [x] More inequality/comparisons (`<`, `>` `<>` etc)
* [x] Add rough language specs to readme
* [x] Import files (in compiler, compile and include generated, like functions. Have an include chain, warn and skip if file has been seen before)
* [x] Uniary minus (negation), list math.
* [x] List inequality (like `<(0 get(x) 10)` for 'x is between 1 and 9')
* [x] Assert function
* [x] Switch-like statement (for if-else chains)
* [x] Treat `variableName` as `get(variableName)`

* [x] Split into library and apps
* [x] Add a REPL.

* [x] Change assm definition to be less stringy
      - Tagged basic type (NaN-tagging)
      - All non-numbers are pointers to type
* [x] Basic types

* [x] Change list-equals to compare all to *first* item, so you can do like `equals(0 a b c d)` to check if *any* equal zero. Same for not-equal.

* [x] Underscores in numbers
* [x] String interning / deduplication (compiler)
* [x] Optimise get/set memory instructions
* [x] Compiler & runtime optimise for increment and decrement
* [x] Iteration of strings (maybe `get(str idx)`?)

* [x] Circular buffer in the repl console
* [x] Automatic formatter/prettifier (indent if more opens than closes, unindent on match)
* [x] Highlighting for current parenthesis scope

* [x] Short-string stack value to reduce GC gunk
* [x] Optimised compare&jump instructions? (like, `[C][>][argCount][opCodeCount]` -- packs 3 ops into 1 when opCodeCount is less than ~32K)
* [ ] Single step in repl gui
* [ ] Runtime interpreter exceptions to give position

* [ ] Memory model
* [ ] Built-ins for hash-tables, 1D arrays, 2D arrays.
* [ ] String builder in memory model (array concat?)
* [ ] Experiment with NAR propagation and a test function.
* [ ] Syntax for defining variadic functions ( `...{array name}` or something? Look at https://blog.taylorwood.io/2017/09/15/year-behind.html )

* [ ] Reference counting / basic GC / etc (follow scope? Maybe copy-arenas for simplicity. Be smart about params)
* [ ] String interning / deduplication (runtime)

* [ ] Handle malformed source code correctly
* [ ] Replace C# exceptions with internal error handling
* [ ] Skip and EndWhile for loops (like `continue` and `break` in C)
* [ ] More math: abs, sign, pow, floor, ceil, round, random, sin, cos, tan, ...etc
* [ ] More math: useful constants (pi, e, inf, neg-inf)
* [ ] Map files for byte code, to help with error messages and diagnosis.
* [ ] Make sure (de)serialisation is available
* [ ] Object-literal form? Functions-as-data, or special syntax?
* [ ] Built-ins to handle variadic functions (expose naming? Limit '__' prefixed names to local scope only?)
* [ ] Infix blocks? (like `set(x [y + (4 * z)])`?)
* [ ] Iterators/Generators?
* [ ] Try optimised 'top n' references in Scope?
* [ ] Try simple tail-recursion optimisation?

### Data types

* String
* Int
* Float/Number
* Set/flags
* Dictionary/map/hashtable
* Array (of type)
* List (heterogenous)
* Grid (2+D array, sparse)

# Language definitions

## Core language

A lisp-look-alike language. Atoms are treated as variable reads, except in the direct parameters of `get`, `set`, `isset`, `unset`.
Delimited strings are treated as a single value.
Values go on the value-stack. Variables and functions are case sensitive.
Function and variable names can contain any non-whitespace characters, but must not start with numbers.

Strings are quoted with either `"` or `'`. String escapes with `\`

Whitespace is significant as an atom separator, but all whitespace is treated equally (newlines, tabs, any number of spaces).
The `,` character is considered whitespace.
Everything else is function calls. Function calls start with the function name, then have parenthesis surrounding parameters.
Function calls may be parameters.

Language built-in functions are tried first, then custom defined functions.
The `def` function defines custom functions

### Keywords:

`true`, `false` (technically these are just string atoms at the moment)

### Reserved functions

```
=    equals    >    <    <>    not-equal
assert    random    eval    call    not    or
and    readkey    readline    print    substring
length    replace    concat    return    +    -
*    /    %
()
```

### get (1 param)
Get the value of a variable by name. If a variable is used *outside* of the params of a `get` or `set`, it is treated as a get.
```javascript
print("My value is " get(val))
print("My value is "     val )  // this is the same as above

set(dst get(src)) // copy src into dst (can't use implicit get here)
```

### get (2 params)
Get a subvalue of a variable by key. For strings, this returns a character. Also to be used for hash tables and sets (later)
Indexes are zero-based.
```javascript
set(str "Hello, world")
print(get(str 5))       // ","
```

### set (2 params)
Set the value of a variable
```javascript
set(myVar "hello")
set(var2 "world")
print(myVar ", " var2) // "hello, world"
```

### `=` or `equals` (2+ params)
Returns true if values are all the same

### `<>` (2+ params)
Returns true if each value is different to the last
```javascript
<>("Hello"  "world") // true
<>(1 1 1)            // false
<>(0 1 0)            // true
```

### `>` (2+ params)
Returns true if each number is less than the one before
```javascript
>(100 0)  // true
>(3 2 1)  // true -- like the infix  3 > 2 > 1
>(50 x 0) // is 'x' between 49 and 1
```

### `<` (2+ params)
Returns true if each number is bigger than the one before

### `isset` (1 param)
Returns true if the name has a value assigned

### `unset` (1 param)
Remove a name definition

```javascript
print(isset(x)) // false
set(x false)
print(isset(x)) // true
unset(x)
print(isset(x)) // false
```

### `def` (2 params)
Define a subroutine / custom function. First param is the new function's parameter list (may be empty). Second param is the function definition.
```javascript
def (
    timesTwo (x) (
        return( *(2 x) )
    )
)
def (
    meaning? () (
        print("42")
    )
)

print(timesTwo(4)) // "8"
meaning?()         // "42"
```

### `import` (1 param)
Includes another source file at compile time.
This function can only be called from the root level of files (not inside functions, parameters or evals).
Recursive imports will be ignored.
Root level statements will be run, and defined functions will be available after the import call.
```javascript
import("./my-other-file.ecs")

OtherFileFunction("hello")
```

### `while` (1+ params)
Loops through a set of calls while the first parameter returns true
```javascript
set(i 10)
while ( not(=(i 0))
    print(i)
    set(i -(i 1))
) // 10 9 8 7 6 5 4 3 2 1
```

### `if` (1+ params)
Run a set of calls once if the first parameter returns true
```javascript
set(i 10)
while ( not(=(i 0))
    if ( %(i 2)
        print(i)
    )
    set(i -(i 1))
) // 9 7 5 3 1
```

### `pick` (special)
Run the first `if` statement that matches, ignore all the others
```javascript
pick (
    if ( a
        print("A was truthy")
    )

    if ( b
        print("B was truthy, and A was not")
    )

    if ( true // this is how to do a 'default' case
        print("Neither A nor B were truthy")
    )
)
```
This gets converted to a new function definition by the compiler, so `return` will only escape from the `pick` and not any surrounding function.

### `not` (1 param)
Invert boolean. Given 0, "0", "false" or false, returns true. Otherwise returns true.

### `or` (0+ params)
Returns true if any of the parameters are not false or zero.

### `and` (0+ params)
Returns true only if all the parameters are not false or zero.

### `readkey` (0 params)
Pause and wait for a key to be pressed. Returns a string representation of the character pressed

### `readline` (0 params)
Pause and wait for `return` key to be pressed. Returns a string of everything typed until enter.

### `print` (0+ params)
Write all params to the console, then start a new line. If the last param is empty, new line is suppressed

### `substring` (2 params)
Return a shortened string
```javascript
set(long "hello, world")
print(substring(long 7)) // "world"
```

### `substring` (3 params)
Return a shortened string
```javascript
set(long "hello, world")
print(substring(long 3 2)) // "lo"
```

### `length` (1 param)
Return number of characters in a string

### `replace` (3 params)
Replace substrings
```javascript
print(
    replace(
        "this is the source" // string to change
        "th"                 // substring to search for
        "d"                  // replacement string
        ) // "dis is de source"
)
```

### `concat` (0+ params)
Join strings together
```javascript
set(str
    concat(
        "hello"
        ", "
        "world"
        ) // "hello, world"
)
```

### `return` (0+ params)
Exit a subroutine and add values to the value stack.
If *any* path in a subroutine uses `return`, they all must.

### `+` (1 param)
Return a number as-is (uniary plus)

### `+` (2+ params)
Add numbers

### `-` (1 param)
Negate a number (uniary minus)

### `-` (2+ params)
Subtract numbers. If more than two params given, a running subtraction will be made (e.g. `-(12 2 4)` is equivalent to the infix: `(12 - 2) - 4`)

### `*` (2+ params)
Multiply numbers

### `/` (2+ params)
Divide numbers. If more than two params given, a running division will be made (e.g. `/(12 2 4)` is equivalent to the infix: `(12 / 2) / 4`)

### `%` (2+ params)
Get remainder from integer division

### `random` (0+ params)
Get a random value

* 0 params - any size
* 1 param  - max size
* 2 params - range

## Advanced built-in functions

### `eval` (1 param)
(Advanced) Compile and run a program string.
```javascript
eval("print('what')")
```
The evaluated program is run with a copy of current variable scope. Programs run in `eval` can read values, but `set` will have no effect
outside of the `eval`. The last value on the value-stack will be returned from the call to `eval`.

### `call` (1+ params)
(Advanced) Execute a function by name. Used to switch function by a variable.
```javascript
set(i "print")
call(i "Hello, world")
```


## Byte code

The compiled output is encoded in NaN-valued 64-bit IEEE754 tags. Each tag has an type prefix, and potentially some values.
The runtime has a value-stack, from where parameters are pulled, and return values added.
There is also a return-stack for tracking function calls.

### VariableRef `[name hash: uint32]`
Adds a variable reference to the value-stack. This can relate to functions (both built-in and custom), to variable name, or to function parameters

### Opcode `[f][c] [param count: uint16]`
Call a function. The param count MUST be given, but can be zero.
The function name-hash will be popped from the value-stack.
Parameters will be popped from the value-stack in order (so the last pushed value will be the first parameter)
Any values returned by the function will be pushed to the value-stack
If this is a custom function (not reserved by the runtime) the exit position will be pushed to the return-stack.

### Opcode `[f][d] [parameter count: uint16]  [tag count: uint16]`
Define a function.
The function name-hash to bind will be popped from the value-stack.
Adds a function name to the function table, at the present location. Sets the expected number of parameters and the number of tokens of the function.

### Opcode `[c][c]  [token count: uint32]`
Pops a value from the value-stack. Makes a relative jump **forward** if that value is zero or false.
Token count is the number of white-space delimited tokens in the byte code to skip.

### Opcode `[c][j]  [token count: uint32]`
Make an unconditional jump **backward**.

### Opcode `[c][s]  [token count: uint32]`
Make an unconditional jump **forward**.

### Opcode `[C][cmp type: byte] [parameter count: uint16]  [tag count: uint16]`
Compare values on the value-stack and jump **forward** if result is false.

### Opcode `[c][r]`
Return from a function call. A absolute token position value is popped from the return-stack and program flow continues from there.

### Opcode `[c][t] [name hash: uint32]`
Runtime error: Function exit without value.
This is emitted by the compiler to trap functions where some but not all paths in a function return values

### Opcode `[m][g] [name hash: uint32]`
Memory get. Looks up a variable with the given name-hash, pushes its value to the value-stack

### Opcode `[m][s] [name hash: uint32]`
Memory set. Pops a value from the value-stack. That value is stored against the variable with the given name-hash

### Opcode `[m][G] [param count: uint16]`
Extended get. Looks up a variable that uses indexes. Pushes the returned value to the value-stack

### Opcode `[m][S] [param count: uint16]`
Extended set. Sets a subvalue in a variable that uses indexes.

### Opcode `[m][i] [name hash: uint32]`
Test varible scope.
If there is a variable in scope with the given name-hash, `true` is
pushed to the value-stack (regardless of the *actual* value of the variable)

### Opcode `[m][u] [name hash: uint32]`
Remove a variable from scope. If there is a variable in scope with that name, it is removed.
This only works for the most local scope, or the global scope.

### Opcode `[i] [inc: int8] [name hash: uint32]`
Increment/decrement by a fixed value.
Looks up a variable with the given name-hash, adds the value of `inc` and stores it back.
No value is added or removed from the value-stack.
