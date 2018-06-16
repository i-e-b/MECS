# Compiled Script
Parse and compile a prefix expression (source code) to a postfix expression (compiled source code).

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
