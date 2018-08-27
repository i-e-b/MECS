# Compiled Script
Parse and compile a prefix expression (source code) to a simple byte code.

## TODO

* [x] Add custom subroutines
* [x] Add value returns (I want to catch failure to return early, without having to define too much)
* [x] More inequality/comparisons (`<`, `>` `<>` etc)
* [ ] Add rough language specs to readme
* [ ] Import files
* [ ] Syntax sugar (treat `variableName` as `get(variableName)`, etc)
* [ ] Memory model (heap, arrays)
* [ ] Built-ins to handle variadic functions (expose naming?)
* [ ] Move value definitions into memory
* [ ] Change assm definition to be less stringy
* [ ] Infix blocks? (like `set(x [y + (4 * z)])`? All var names treated as 'get')

# Language definitions

## Core language

A list-like language. Atoms are treated as string values, delimited strings are treated as a single value.
Values go on the value stack. Variables and functions are case sensitive.
Function and variable names can contain any non-whitespace characters.

Strings are quoted with either `"` or `'`. String escapes with `\`

Whitespace is significant as an atom separator, but all whitespace is treated equally (newlines, tabs, any number of spaces).
Everything else is function calls. Function calls start with the function name, then have parenthesis surrounding parameters.
Function calls may be parameters.

Language built-in functions are tried first, then custom defined functions.
The `def` function defines custom functions

### get (1 param)
Get the value of a variable by name.

### set (2 params)
Set the value of a variable
```javascript
set(myVar "hello")
set(var2 "world")
print(get(myVar) ", " get(var2)) // "hello, world"
```

### `=` or `equals` (2 params)
Returns true if two values are the same

### `<>` (2 params)
Returns false if two numbers are the same

### `>` (2 params)
Returns true if the second param is a smaller number than the first

### `<` (2 params)
Returns true if the second param is a bigger number than the first

### isset (1 param)
Returns true if the name has a value assigned

### unset (1 param)
Remove a name definition

```javascript
print(isset(x)) // false
set(x false)
print(isset(x)) // true
unset(x)
print(isset(x)) // false
```

### def (2 params)
Define a subroutine / custom function
```javascript
def (
    timesTwo (x) (
        return( *(2 get(x)) )
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

### while (1+ params)
Loops through a set of calls while the first parameter returns true
```javascript
set(i 10)
while ( not(=(get(i) 0))
    print(get(i))
    set(i -(get(i) 1))
) // 10 9 8 7 6 5 4 3 2 1
```

### if (1+ params)
Run a set of calls once if the first parameter returns true
```javascript
set(i 10)
while ( not(=(get(i) 0))
    if ( %(get(i) 2)
        print(get(i))
    )
    set(i -(get(i) 1))
) // 9 7 5 3 1
```

### not (1 param)
Invert boolean. Given 0, "0", "false" or false, returns true. Otherwise returns true.

### or (0+ params)
Returns true if any of the parameters are not false or zero.

### and (0+ params)
Returns true only if all the parameters are not false or zero.

### readkey (0 params)
Pause and wait for a key to be pressed. Returns a string representation of the character pressed

### readline (0 params)
Pause and wait for `return` key to be pressed. Returns a string of everything typed until enter.

### print (0+ params)
Write all params to the console, then start a new line. If the last param is empty, new line is suppressed

### substring (2 params)
Return a shortened string
```javascript
set(long "hello, world")
print(substring(get(long) 7)) // "world"
```

### substring (3 params)
Return a shortened string
```javascript
set(long "hello, world")
print(substring(get(long) 3 2)) // "lo"
```

### length (1 param)
Return number of characters in a string

### replace (3 params)
Replace substrings
```javascript
print(
    replace(
        "this is the source"
        "th"
        "d"
        ) // "dis is de source"
)
```

### concat (0+ params)
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

### return (0+ params)
Exit a subroutine and add values to the value stack.
If *any* path in a subroutine uses `return`, they all must.


TODO - change math to sum lists
### + (2 params)
Add numbers

### - (2 params)
Subtract numbers

### * (2 params)
Multiply numbers

### / (2 params)
Divide numbers

### % (2 params)
Get remainder from integer division


### eval (1 param)
(Advanced) Compile and run a program string.
```javascript
eval("print('what')")
```
The evaluated program is run with the current variable scope left in place.

### call (1+ params)
(Advanced) Execute a function by name. Used to switch function by a variable.
```javascript
set(i "print")
call(get(i) "Hello, world")
```


## Byte code

# Examples

## First, compose the script.
```
set(i 0)
/* while i < 10 */
while(not(=(10 get(i)))
	print("Counter: " get(i))
	if(=(get(i), 3)
		// If i = 3, show this message.
		print("this is 3.")
	)
	// Increment i.
	set(i,+(get(i) 1))
)
// Optional.
unset(i)
print('Press enter to continue...')
readline()
```

## Then, compile it.
It will transform while and if statements to cmp and jmp statement to speed up script execution.

You will get something similar to:
```
v0
vi
ms
v10
vi
mg
f%3d 2
fnot 1
ccmp 24
vCounter%3A%20
vi
mg
fprint 2
vi
mg
v3
f%3d 2
ccmp 3
vthis%20is%203.
fprint 1
vi
mg
v1
f%2b 2
vi
ms
cjmp 31
vi
mu
vPress%20enter%20to%20continue...
fprint 1
freadline 0
```

## Finally, run it

First, you start with an empty stack. Everytime, it evaluate one element and then, it goes to the next element.
It can add or remove elements on the stack.
The cmp and jmp statements can jump in the program.

- The cmp statement evaluate the last element in the stack. If it is false, it will jump in the program based on the next element in the program. (this is typically a if or a while statement)
- The jmp statement. (the end of a while statement, we need to go up in the program to evaluate the condition and eventually, we will do cmp statement to determine if we continue in the while statement)

## Output
```
Counter: 0
Counter: 1
Counter: 2
Counter: 3
this is 3.
Counter: 4
Counter: 5
Counter: 6
Counter: 7
Counter: 8
Counter: 9
Press enter to continue...
```
