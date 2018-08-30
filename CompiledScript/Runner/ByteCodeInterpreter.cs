using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using CompiledScript.Utils;
using CompiledScript.Compiler;

namespace CompiledScript.Runner
{
    class ByteCodeInterpreter
    {
        private List<string> program;
        public Dictionary<string, FunctionDefinition>  Functions;
        public Scope Variables;
       
        private int words;

        public virtual void Init(string bin, Scope importVariables = null)
        {
            program = new List<string>();

            var tokens = bin.Split('\t', '\n', '\r', ' ');
            foreach (var token in tokens)
            {
                if (token.Length == 0) continue;

                program.Add(StringEncoding.Decode(token));
            }
            Variables = new Scope(importVariables);
            Functions = new Dictionary<string, FunctionDefinition>();
        }

        public string Execute(bool resetVars, bool verbose)
        {
		    string evalResult;

            if (resetVars) { Variables.Clear(); }

		    var valueStack = new LinkedList<string>();
            var returnStack = new Stack<int>(); // absolute position for call and return TODO: implement 
		    
            var param = new LinkedList<string>();
            int position = 0; // the PC. This is in TOKEN positions, not bytes
		    int programCount = program.Count;
		    
		    while (position < programCount)
            {
			    words++;
                
                // Prevent stackoverflow.
                // Ex: if(true 1 10 20)
			    if ((words & 127) == 0)
                {
					while (valueStack.Count > 200)
                    {
						valueStack.RemoveFirst();
				    }
			    }

			    string word = program[position];

                if (verbose)
                {
                    Console.WriteLine("---------------------------------------------");
                    Console.WriteLine("Iteration #" + words);
                    Console.WriteLine("pos = "+position + "/" + programCount);
                    Console.WriteLine("word = " + word);
                }

			    char action = word[0];

			    switch (action)
                {
			        case 'v': // Value.
				        valueStack.AddLast(word);
				        break;

			        case 'f': // Function *CALLS*
				        position = HandleFunctionCall(position, param, valueStack, word, returnStack, Variables);
			            break;

			        case 'c': // flow Control -- conditions, jumps etc
				        position = HandleControlSignal(word, valueStack, returnStack, position);
			            break;

			        case 'm': // Memory access - get|set|isset|unset
				        HandleMemoryAccess(word, valueStack);
			            break;

			        case 's': // reserved for System operation.
				        break;

                    case 'd': // function Definition
                        position = HandleFunctionDefinition(word, position);
                        break;

                    default:
                        throw new Exception("Unexpected op code at " + position + " : " + word);
                }
                position++;
		    }

		    if (valueStack.Count != 0)
            {
                evalResult = valueStack.Last();
                valueStack.RemoveLast();
		    }
            else
            {
			    evalResult = "";
		    }

		    valueStack.Clear();

		    return evalResult;
	    }

        private int HandleFunctionDefinition(string word, int position)
        {
            // [d, FunctionName] [parameter count] [skip length]

            var funcName = word.Substring(1);
            if (Functions.ContainsKey(funcName)) throw new Exception("Function '"+funcName+"' redefined at "+position+". Original at "+Functions[funcName]);
            var paramCount = TryParseInt(program[position + 1]);
            var offset = TryParseInt(program[position + 2]);

            Functions.Add(funcName, new FunctionDefinition{
                StartPosition = position + 2,
                ParamCount = paramCount
            });

            return position + offset + 3; // + definition length + args
        }

        private void HandleMemoryAccess(string word, LinkedList<string> valueStack)
        {
            word = word.Substring(1);
            var action = word[0];

            string varName = valueStack.Last();
            string value;
            valueStack.RemoveLast();
            varName = varName.Substring(1);

            switch (action)
            {
                case 'g': // get (adds a value to the stack, false if not set)
                    // TODO: need to scopes for function calls
                    value = Variables.Resolve(varName);
                    if (value != null)
                    {
                        valueStack.AddLast("v" + value);
                    }
                    else
                    {
                        // TODO: strict mode?
                        valueStack.AddLast("vfalse");
                    }

                    break;
                case 's': // set
                    if (valueStack.Count < 1) throw new Exception("There were no values to save. Did you forget a `return` in a function?");
                    value = valueStack.Last();
                    valueStack.RemoveLast();
                    value = value.Substring(1);
                    Variables.SetValue(varName, value);
                    break;
                case 'i': // is set? (adds a bool to the stack)
                    valueStack.AddLast("v" + Variables.CanResolve(varName));
                    break;
                case 'u': // unset
                    Variables.Remove(varName);
                    break;
            }
        }

        private int HandleControlSignal(string word, LinkedList<string> valueStack, Stack<int> returnStack,  int position)
        {
            word = word.Substring(1);
            var action = word[0];
            switch (action)
            {
                // cmp - relative jump *DOWN* if top of stack is false
                case 'c':
                    string condition = valueStack.Last();
                    valueStack.RemoveLast();
                    condition = condition.Length == 0 ? "false" : condition.Substring(1);

                    position++;
                    var bodyLengthString = program[position];
                    var bodyLength = TryParseInt(bodyLengthString);

                    condition = condition.ToLower();

                    if (condition.Equals("false") || condition.Equals("0"))
                    {
                        position += bodyLength;
                    }
                    break;

                // jmp - unconditional relative jump *UP*
                case 'j':
                    position++;
                    string jmpLengthString = program[position];
                    int jmpLength = TryParseInt(jmpLengthString);
                    position -= 2;
                    position -= jmpLength;
                    break;

                // ct - call term - a function that returns values ended without returning
                case 't':
                    throw new Exception("A function returned without setting a value. Did you miss a 'return' in '"+word.Substring(1)+"'");

                // ret - pop return stack and jump to absolute position
                case 'r':
                    // set a special value for "void return" here, in case someone tries to use the result of a void function
                    valueStack.AddLast("v");
                    if (returnStack.Count < 1) throw new Exception("Return stack empty. Check program logic");
                    Variables.DropScope();
                    position = returnStack.Pop();
                    break;
            }

            return position;
        }

        private int HandleFunctionCall(int position, LinkedList<string> param, LinkedList<string> valueStack, string word, Stack<int> returnStack, Scope parameterNames)
        {
            position++;
            var nbParams = TryParseInt(program[position].Trim());
            param.Clear();

            // Pop values from stack.
            for (int i = 0; i < nbParams; i++)
            {
                try
                {
                    param.AddFirst(valueStack.Last().Substring(1));
                    valueStack.RemoveLast();
                }
                catch (Exception ex)
                {
                    Console.WriteLine("Stack underflow. Ran out of values before function call at " + position + ": " + ex);
                }
            }

            // Evaluate function.
            var evalResult = Eval(ref position, word.Substring(1), nbParams, param, returnStack, valueStack);

            // Add result on stack as a value.
            if (evalResult != null)
            {
                valueStack.AddLast("v" + evalResult);
            }

            return position;
        }

        // Evaluate a function call
	    public string Eval(ref int position, string functionName, int nbParams, LinkedList<string> param, Stack<int> returnStack, LinkedList<string> valueStack)
        {
            string condition;

		    if (functionName == "()" && nbParams != 0) // todo: better listing behaviour
            {
			    return param.ElementAt(nbParams - 1);
		    }

            // each element equal to the last
		    if (functionName == "=" || functionName == "equals")
            {
                if (nbParams < 2) throw new Exception("equals ( = ) must have at least two things to compare");
                return "" + FoldInequality(param, (a, b) => a == b);
            }

            // Each element smaller than the last
            if (functionName == ">")
            {
                if (nbParams < 2) throw new Exception("greater than ( > ) must have at least two things to compare");
                return "" + FoldInequality(param, (a, b) => TryParseInt(a) > TryParseInt(b));
            }

            // Each element larger than the last
            if (functionName == "<")
            {
                if (nbParams < 2) throw new Exception("less than ( < ) must have at least two things to compare");
                return "" + FoldInequality(param, (a, b) => TryParseInt(a) < TryParseInt(b));
            }

            // Each element DIFFERENT TO THE LAST (does not check set uniqueness!)
            if (functionName == "<>" || functionName == "not-equal")
            {
                if (nbParams < 2) throw new Exception("not-equal ( <> ) must have at least two things to compare");
                return "" + FoldInequality(param, (a, b) => a != b);
            }

            string result;
            switch (functionName)
            {
                case "eval":
                    var reader = new SourceCodeReader();
                    var statements = param.ElementAt(0);
                    var programTmp = reader.Read(statements);
                    var bin = Compiler.Compiler.CompileRoot(programTmp, false);
                    //var bin = Compiler.Compiler.Compile(programTmp, 0, false, null, null).ByteCode;
                    var interpreter = new ByteCodeInterpreter();
                    interpreter.Init(bin, Variables);
                    result = interpreter.Execute(false, false);
                    if (result != null && result.Length > 1) result = result.Substring(1);
                    return result;

                case "call":
                    functionName = param.ElementAt(0);
                    nbParams--;
                    param.RemoveFirst();
                    return Eval(ref position, functionName, nbParams, param, returnStack, valueStack);

                case "not" when nbParams == 1:
                    condition = param.ElementAt(0);

                    condition = condition.ToLower();

                    return "" + (condition == "false" || condition == "0");

                case "or":
                {
                    bool more = nbParams > 0;
                    int i = 0;
                    result = "false";
                    while (more)
                    {
                        condition = param.ElementAt(i);
                        condition = condition.ToLower();

                        if (condition != "false" && condition != "0")
                        {
                            result = "true";
                            break;
                        }
                        i++;
                        more = i < nbParams;
                    }
                    return result;
                }

                case "and":
                {
                    bool more = nbParams > 0;
                    int i = 0;
                    result = more + "";
                    while (more)
                    {
                        condition = param.ElementAt(i);
                        condition = condition.ToLower();

                        if (condition == "false" || condition == "0")
                        {
                            result = "false";
                            break;
                        }
                        i++;
                        more = i < nbParams;
                    }
                    return result;
                }

                case "readkey":
                    return Console.ReadKey().KeyChar.ToString();

                case "readline":
                    return Console.ReadLine();

                case "print":
                    foreach (string s in param)
                    {
                        Console.Write(s);
                    }
                    if (param.Last.Value != "") Console.WriteLine();
                    break;

                case "substring" when nbParams == 2:
                    return param.ElementAt(0).Substring(TryParseInt(param.ElementAt(1)));

                case "substring":
                    if(nbParams == 3)
                    {
                        int start = TryParseInt(param.ElementAt(1));
                        int length = TryParseInt(param.ElementAt(2));
                    
                        try
                        {
                            string s = param.ElementAt(0).Substring(start, length);
                            return s;
                        }
                        catch(Exception ex)
                        {
                            Console.WriteLine("Runner error: " + ex);
                        }
                    }

                    break;

                case "length" when nbParams == 1:
                    return param.ElementAt(0).Length + "";

                case "replace" when nbParams == 3:
                    string exp = param.ElementAt(0);
                    string oldValue = param.ElementAt(1);
                    string newValue = param.ElementAt(2);
                    exp = exp.Replace(oldValue, newValue);
                    return exp;

                case "concat":
                    StringBuilder builder = new StringBuilder();
                    foreach (string s in param)
                    {
                        builder.Append(s);
                    }
                    return builder.ToString();

                case "return":
                    // need to stop flow, check we have a return stack, check value need?
                    foreach (string s in param)
                    {
                        valueStack.AddLast("v" + s);
                    }
                    
                    if (returnStack.Count < 1) throw new Exception("Return stack empty. Check program logic");
                    Variables.DropScope();
                    position = returnStack.Pop();
                    break;


                default:
                    if (functionName == "()") { // empty object. TODO: when we have better values, have an empty list
                        return "";
                    }
                    else if (IsMathFunc(functionName)) // TODO: allow list math
                    {
                        // handle math functions
                        try
                        {
                            return EvalMath(functionName[0], param);
                        }
                        catch (Exception e)
                        {
                            return "Math Error : " + e.Message;
                        }
                    }
                    else if (Functions.ContainsKey(functionName))
                    {
                        // handle functions that are defined in the program

                        Variables.PushScope(param); // write parameters into new scope
                        returnStack.Push(position); // set position for 'cret' call
                        position = Functions[functionName].StartPosition; // move pointer to start of function
                        return null; // return no value, continue execution elsewhere
                    }
                    else
                    {
                        throw new Exception("Tried to call an undefined function '" + functionName + "'\r\nKnown functions: " + string.Join(", ", Functions.Keys));
                    }
            }

            return null; // Void.
	    }

        private bool FoldInequality(IEnumerable<string> list, Func<string, string, bool> comparitor)
        {
            bool first = true;
            string prev = null;
            foreach (var str in list)
            {
                if (first) {
                    prev = str;
                    first = false;
                    continue;
                }
                if ( ! comparitor(prev, str)) return false;
                prev = str;
            }
            return true;
        }

        private bool IsMathFunc(string functionName)
        {
            if (functionName.Length != 1) return false;
            switch (functionName[0])
            {
                case '+':
                case '-':
                case '*':
                case '/':
                case '%':
                    return true;
                default:
                    return false;
            }
        }

        private static int TryParseInt(string s)
        {
            int.TryParse(s, out int result);
            return result;
	    }

	    private static string EvalMath(char op, LinkedList<string> args)
        {
            var argCount = args.Count;

            if (argCount == 0) throw new Exception("Math funtion called with no arguments");

            var arg0 = TryParseInt(args.First());

            if (argCount == 1) {
                switch (op) {
                    case '+': // uniary plus: no-op
                        return arg0 + "";

                    case '-': // uniary minus: value negation
                        return (-arg0) + "";

                    case '%': // uniary remainder: common case, odd/even
                        return (arg0 % 2) + "";

                    default:
                        throw new Exception("Uniary '"+op+"' is not supported");
                }
            }

		    switch (op)
            {
		        case '+':
			        return "" + args.Sum(TryParseInt);

		        case '-':
			        return "" + args.ChainDifference(TryParseInt);

		        case '*':
		            return "" + args.ChainProduct(TryParseInt);

		        case '/':
		            return "" + args.ChainDivide(TryParseInt);

		        case '%':
		            return "" + args.ChainRemainder(TryParseInt);
		    }

            throw new Exception("Error Math : unknown op : " + op); // this should never happen. It would be an error in the interpreter
	    }
    }
}
