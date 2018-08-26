using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using CompiledScript.Utils;
using CompiledScript.Compiler;

namespace CompiledScript.Runner
{
    class BasicInterpreter
    {
        private List<string> program;
        public Dictionary<string, string> GlobalVariables; // TODO: remove and use only scope
        public Dictionary<string, FDef>  Functions;
        public Scope LocalVariables;
       
        private int words;

        public virtual void Init(string bin)
        {
            program = new List<string>();

            var tokens = bin.Split('\t', '\n', '\r', ' ');
            foreach (var token in tokens)
            {
                if (token.Length == 0) continue;

                program.Add(StringEncoding.Decode(token));
            }
            GlobalVariables = new Dictionary<string, string>();
            Functions = new Dictionary<string, FDef>();
        }

        public string Execute(bool resetVars, bool verbose)
        {
		    string evalResult;

            if (resetVars) { GlobalVariables.Clear(); }

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
				        position = HandleFunctionCall(position, param, valueStack, word);
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

            Functions.Add(funcName, new FDef{
                TokenCount = position + 2,
                ParamCount = paramCount
            });

            return position + offset + 3; // + definition length + args
        }

        private void HandleMemoryAccess(string word, LinkedList<string> valueStack)
        {
            word = word.Substring(1);
            var action = word[0];

            string varName = valueStack.Last();
            valueStack.RemoveLast();
            varName = varName.Substring(1);

            switch (action)
            {
                case 'g': // get (adds a value to the stack, false if not set)
                    // TODO: need to scopes for function calls
                    if (GlobalVariables.ContainsKey(varName))
                    {
                        valueStack.AddLast("v" + GlobalVariables[varName]);
                    }
                    else
                    {
                        // TODO: strict mode?
                        valueStack.AddLast("vfalse");
                    }

                    break;
                case 's': // set
                    string value = valueStack.Last();
                    valueStack.RemoveLast();
                    //value = StringEncoding.decode(valeur).Substring(1);
                    value = value.Substring(1);
                    {
                        GlobalVariables.Remove(varName);
                    }
                    GlobalVariables[varName] = value;
                    break;
                case 'i': // is set? (adds a bool to the stack)
                    valueStack.AddLast("v" + GlobalVariables.ContainsKey(varName));
                    break;
                case 'u': // unset
                    GlobalVariables.Remove(varName);
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

                // ret - pop return stack and jump to absolute position
                case 'r':
                    if (returnStack.Count < 1) throw new Exception("Return stack empty. Check program logic");
                    position = returnStack.Pop();
                    break;
            }

            return position;
        }

        private int HandleFunctionCall(int position, LinkedList<string> param, LinkedList<string> valueStack, string word)
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
                    Console.WriteLine("Runner error: " + ex);
                }
            }

            // Evaluate function.
            var evalResult = Eval(word.Substring(1), nbParams, param);

            // Add result on stack as a value.
            if (evalResult != null)
            {
                valueStack.AddLast("v" + evalResult);
            }

            return position;
        }

        // Evaluate a function call
	    public string Eval(string functionName, int nbParams, LinkedList<string> param)
        {
            //functionName = functionName.ToLower(); // uncomment to make case insensitive
            string condition;

		    if (functionName == "()" && nbParams != 0)
            {
			    return param.ElementAt(nbParams - 1);
		    }

		    if ((functionName == "=" || functionName == "equals") && nbParams == 2)
            {
			    return ( param.ElementAt(0) == (param.ElementAt(1)) ) + "";
		    }

            switch (functionName)
            {
                case "eval":
                    SourceCodeReader reader = new SourceCodeReader();
                    Node programTmp = reader.Read(param.ElementAt(0));
                    string contenuBin = CompilerWriter.Compile(programTmp, false);
                    BasicInterpreter basicReader = new BasicInterpreter();
                    basicReader.Init(contenuBin);
                    basicReader.Execute(false, false);
                    break;

                case "call":
                    functionName = param.ElementAt(0);
                    nbParams--;
                    param.RemoveFirst();
                    return Eval(functionName, nbParams, param);

                case "not" when nbParams == 1:
                    condition = param.ElementAt(0);

                    condition = condition.ToLower();

                    return "" + (condition == "false" || condition == "0");

                case "or":
                {
                    bool continuer = nbParams > 0;
                    int i = 0;
                    string result = "false";
                    while (continuer)
                    {
                        condition = param.ElementAt(i);
                        condition = condition.ToLower();

                        if (condition != "false" && condition != "0")
                        {
                            result = "true";
                            break;
                        }
                        i++;
                        continuer = i < nbParams;
                    }
                    return result;
                }

                case "and":
                {
                    bool continuer = nbParams > 0;
                    int i = 0;
                    string result = continuer + "";
                    while (continuer)
                    {
                         condition = param.ElementAt(i);
                        condition = condition.ToLower();

                        if (condition == "false" || condition == "0")
                        {
                            result = "false";
                            break;
                        }
                        i++;
                        continuer = i < nbParams;
                    }
                    return result;
                }

                case "readkey":
                    Console.ReadKey();
                    return null;

                case "readline":
                    return Console.ReadLine();

                case "print":
                    foreach (string s in param)
                    {
                        Console.Write(s);
                    }
                    Console.WriteLine();
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


                default:
                    if (IsMathFunc(functionName) && nbParams == 2)
                    {
                        try
                        {
                            return EvalMath(functionName[0], TryParseInt(param.ElementAt(0)),
                                TryParseInt(param.ElementAt(1)));
                        }
                        catch (Exception e)
                        {
                            return "Math Error : " + e.Message;
                        }
                    } else if (Functions.ContainsKey(functionName)) {
                        // TODO: call the function
                        LocalVariables.PushScope(param);
                        throw new  Exception("Known function '"+functionName+"'; Expects 2 parameters, loaded: "+string.Join(", ",param));
                        // Not entirely sure how to pass parameters, and pick them up on the other side

                    } else {
                        throw new Exception("Tried to call an undefined function '" + functionName + "'\r\nKnown functions: "+string.Join(", ", Functions.Keys));
                    }

                    break;
            }

		    return null; // Void.
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

	    private static string EvalMath(char op, int opa, int opb)
        {
		    switch (op)
            {
		        case '+':
			        return (opa + opb) + "";
		        case '-':
			        return (opa - opb) + "";
		        case '*':
			        return (opa * opb) + "";
		        case '/':
			        if (opb == 0)
                    {
				        return "Error Math : Divide by 0";
			        }
			        return (opa / opb) + "";
		        case '%':
			        if (opb == 0)
                    {
				        return "Error Math : Divide by 0";
			        }
			        return (opa % opb) + "";
		    }
		    return "Error Math : unknow op : " + op;
	    }
    }
}
