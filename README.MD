# MECS
Compiler, Interpreter, and Runtime engine for a simple language.
Intended to contain a complete memory model, for embedding onto a SBC.

### Data types

* String                      (Ascii block with header -- not a null-term string)
* Int                         (tail 32 bits of tag)
* Float/Number                (non-nan double)
* Dictionary/map/hashtable    (Hash table keyed and valued with tags)
* Array/List                  (Scalable vector of tags)

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
`get`, `set`, `isset`, `unset`, `def`.

### Reserved functions

```
=    equals    >    <    <>    not-equal

new-list  push     pop    dequeue
new-map

assert    random     eval        call     not    or
and       readkey    readline    print    substring
length    replace    concat      return

+    -    *    /    %
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
Get a subvalue of a variable by key. For strings, this returns a character.
For lists, this will read the element at the index
Indexes are zero-based.
```javascript
set(str "Hello, world")
print(get(str 5))       // ","
```

If more than 1 index is given, each index is read and concatenated into the result

```javascript
set(str "Hello, world")
print(get(str 7 8 9 11))       // "word"
```

### set (2 params)
Set the value of a variable
```javascript
set(myVar "hello")
set(var2 "world")
print(myVar ", " var2) // "hello, world"
```

To set the value of a list element, use the function-call syntax for the index
```
set(myList new-list(1 2 3)) // new list of [1,2,3]
set(myList(1) 5)            // now list is [1,5,3]
```

### push (2 params)
Add an item to the end of a list

### pop (1 param)
Read and remove the last item from a list

### dequeue (1 param)
Read and remove the first item from a list

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

Note: the C# code uses an older version of the definitions. It should be considered deprecated.

### VariableRef `[name hash: uint32]`
Adds a variable reference to the value-stack. This can relate to functions (both built-in and custom), to variable name, or to function parameters

### Opcode `[f][c] [param count: uint8] [name hash: uint32]`
Call a function. The param count MUST be given, but can be zero.
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

### Opcode `[m][g] [param count: uint8] [name hash: uint32]`
Memory get. Looks up a variable with the given name-hash, pushes its value to the value-stack
If param count is greater than zero, looks up a variable that using indexes. Pushes the returned value(s) to the value-stack.

### Opcode `[m][s] [param count: uint8] [name hash: uint32]`
Memory set. Pops a value from the value-stack. That value is stored against the variable with the given name-hash.
If param count is greater than one, sets a subvalue in a variable that uses indexes.

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
