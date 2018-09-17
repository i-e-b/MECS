﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using EvieCompilerSystem.Compiler;
using EvieCompilerSystem.InputOutput;
using EvieCompilerSystem.Utils;

namespace EvieCompilerSystem.Runtime
{
    public class ByteCodeInterpreter
    {
        private List<double> program;
        public HashTable<FunctionDefinition>  Functions;
        public static HashTable<string> DebugSymbols;
        public Scope Variables;
        public static Random rnd = new Random();
       
        private int stepsTaken;
        private TextReader _input;
        private TextWriter _output;
        private RuntimeMemoryModel _memory;
        private bool runningVerbose;

        public void Init(RuntimeMemoryModel bin, TextReader input, TextWriter output, Scope importVariables = null, HashTable<string> debugSymbols = null)
        {
            _memory = bin;

            Functions = BuiltInFunctionSymbols();

            program = bin.Tokens();

            Variables = new Scope(importVariables);
            DebugSymbols = debugSymbols;

            _input  = input;
            _output = output;
        }

        /// <summary>
        /// Symbol mapping for built-in functions
        /// </summary>
        public static HashTable<FunctionDefinition> BuiltInFunctionSymbols()
        {
            var tmp = new HashTable<FunctionDefinition>(64);
            Action<string, FuncKind> add = (name, type) => {
                tmp.Add(NanTags.GetCrushedName(name), new FunctionDefinition{ Kind = type});
            };

            add("=", FuncKind.Equal); add("equals", FuncKind.Equal); add(">", FuncKind.GreaterThan);
            add("<", FuncKind.LessThan); add("<>", FuncKind.NotEqual); add("not-equal", FuncKind.NotEqual);
            add("assert", FuncKind.Assert); add("random", FuncKind.Random); add("eval", FuncKind.Eval);
            add("call", FuncKind.Call); add("not", FuncKind.LogicNot); add("or", FuncKind.LogicOr);
            add("and", FuncKind.LogicAnd); add("readkey", FuncKind.ReadKey); add("readline", FuncKind.ReadLine);
            add("print", FuncKind.Print); add("substring", FuncKind.Substring);
            add("length", FuncKind.Length); add("replace", FuncKind.Replace); add("concat", FuncKind.Concat);
            add("+", FuncKind.MathAdd); add("-", FuncKind.MathSub); add("*", FuncKind.MathProd);
            add("/", FuncKind.MathDiv); add("%", FuncKind.MathMod);
            add("()", FuncKind.UnitEmpty); // empty value marker
            return tmp;
        }

        public static HashTable<string> DebugSymbolsForBuiltIns()
        {
            var tmp = new HashTable<string>(64);
            Action<string> add = name => { tmp.Add(NanTags.GetCrushedName(name), name); };

            add("="); add("equals"); add(">"); add("<"); add("<>"); add("not-equal");
            add("assert"); add("random"); add("eval"); add("call"); add("not"); add("or");
            add("and"); add("readkey"); add("readline"); add("print"); add("substring");
            add("length"); add("replace"); add("concat"); add("+"); add("-"); add("*");
            add("/"); add("%"); add("()");
            return tmp;
        }

        public double Execute(bool resetVars, bool verbose)
        {
		    double evalResult;
            runningVerbose = verbose;

            if (resetVars) { Variables.Clear(); }

		    var valueStack = new Stack<double>();
            var returnStack = new Stack<int>(); // absolute position for call and return
		    
            int position = 0; // the PC. This is in TOKEN positions, not bytes
		    int programCount = program.Count;
		    
		    while (position < programCount)
            {
			    stepsTaken++;
                //if (stepsTaken > 1000) throw new Exception("trap");
                // TODO: add a hook for a step event here?
                
                // Prevent stackoverflow.
                // Ex: if(true 1 10 20)
			    if ((stepsTaken & 127) == 0 && valueStack.Count > 100) // TODO: improve this mess. Might be able to use void returns (and add them to loops?)
                {
                    var oldValues = valueStack.ToArray();
                    valueStack = new Stack<double>(oldValues.Skip(oldValues.Length - 100));
                }

                double word = program[position];

                if (verbose)
                {
                    _output.WriteLine("          stack :"+string.Join(", ",valueStack.ToArray().Select(t=> _memory.DiagnosticString(t, DebugSymbols))));
                    _output.WriteLine("          #" + stepsTaken + "; p="+position
                                      +"; w="+_memory.DiagnosticString(word, DebugSymbols) );
                }

                var type = NanTags.TypeOf(word);
                switch (type) {
                    case DataType.Invalid:
                        throw new Exception("Unknown code point at " + position);

                    case DataType.Opcode:
                        // decode opcode and do stuff
                        NanTags.DecodeOpCode(word, out var codeClass, out var codeAction, out var p1, out var p2);
                        ProcessOpCode(codeClass, codeAction, p1, p2, ref position, valueStack, returnStack, word);
                        break;

                    default:
                        valueStack.Push(word); // these could be raw doubles, encoded real values, or references/pointers
                        break;
                }

                
                position++;
		    }

		    if (valueStack.Count != 0)
            {
                evalResult = valueStack.Pop();
		    }
            else
            {
			    evalResult = 0;
		    }

		    valueStack.Clear();

		    return evalResult;
	    }

        private void ProcessOpCode(char codeClass, char codeAction, ushort p1, ushort p2, ref int position, Stack<double> valueStack, Stack<int> returnStack, double word)
        {
            switch (codeClass)
            {
                case 'f': // Function *CALLS*
                    if (codeAction == 'c') {
                        position = PrepareFunctionCall(position, p1, valueStack, returnStack);
                    }
                    else if (codeAction == 'd') {
                        position = HandleFunctionDefinition(p1, p2, position, valueStack);
                    }
                    break;

                case 'c': // flow Control -- conditions, jumps etc
                    {
                        int opCodeCount = p2 + (p1 << 16); // we use 31 bit jumps, in case we have lots of static data
                        position = HandleControlSignal(codeAction, opCodeCount, valueStack, returnStack, position);
                    }
                    break;

                case 'm': // Memory access - get|set|isset|unset
                    if (valueStack.Count < 1) throw new Exception("Empty stack before memory access at position " + position);
                    HandleMemoryAccess(codeAction, valueStack, position);
                    break;

                case 's': // reserved for System operation.
                    break;

                default:
                    throw new Exception("Unexpected op code at " + position + " : " + _memory.DiagnosticString(word, DebugSymbols));
            }
        }

        private int HandleFunctionDefinition(ushort argCount, ushort tokenCount, int position, Stack<double> valueStack)
        {
            var functionNameHash = NanTags.DecodeVariableRef(valueStack.Pop());
            if (Functions.ContainsKey(functionNameHash)) throw new Exception("Function '" + functionNameHash + "' redefined at " + position + ". Original at " + Functions[functionNameHash]);

            Functions.Add(functionNameHash, new FunctionDefinition{
                StartPosition = position,
                ParamCount = argCount
            });

            return position + tokenCount + 1; // + definition length + terminator
        }

        private void HandleMemoryAccess(char action, Stack<double> valueStack, int position)
        {
            var varName = NanTags.DecodeVariableRef(valueStack.Pop());
            double value;

            switch (action)
            {
                case 'g': // get (adds a value to the stack, false if not set)
                    value = Variables.Resolve(varName);
                    valueStack.Push(value);

                    break;

                case 's': // set
                    if (valueStack.Count < 1) throw new Exception("There were no values to save. Did you forget a `return` in a function? Position: " + position);
                    value = valueStack.Pop();
                    Variables.SetValue(varName, value);
                    break;

                case 'i': // is set? (adds a bool to the stack)
                    valueStack.Push(NanTags.EncodeBool(Variables.CanResolve(varName)));
                    break;

                case 'u': // unset
                    Variables.Remove(varName);
                    break;
            }
        }

        private int HandleControlSignal(char action, int opCodeCount, Stack<double> valueStack, Stack<int> returnStack, int position)
        {
            switch (action)
            {
                // cmp - relative jump *DOWN* if top of stack is false
                case 'c':
                    var condition = _memory.CastBoolean(valueStack.Pop());

                    //position++;
                    if (condition == false) { position += opCodeCount; }
                    break;

                // jmp - unconditional relative jump *UP*
                case 'j':
                    position++;
                    int jmpLength = opCodeCount;
                    position -= 1; // this opcode
                    position -= jmpLength;
                    break;
                    

                // skip - unconditional relative jump *DOWN*
                case 's':
                    //position++;
                    position += opCodeCount;
                    break;

                // ct - call term - a function that returns values ended without returning
                case 't':
                    throw new Exception("A function returned without setting a value. Did you miss a 'return' in a function?");

                // ret - pop return stack and jump to absolute position
                case 'r':
                    HandleReturn(ref position, returnStack);
                    break;
            }

            return position;
        }

        private void HandleReturn(ref int position, Stack<int> returnStack)
        {
            if (returnStack.Count < 1) throw new Exception("Return stack empty. Check program logic");
            Variables.DropScope();
            position = returnStack.Pop();
        }

        private int PrepareFunctionCall(int position, ushort nbParams, Stack<double> valueStack, Stack<int> returnStack)
        {
            var functionNameHash = NanTags.DecodeVariableRef(valueStack.Pop());

            var param = new double[nbParams];
            // Pop values from stack into a param cache
            try
            {
                for (int i = nbParams - 1; i >= 0; i--) {
                    param[i] = valueStack.Pop();
                }
            }
            catch (Exception ex)
            {
                throw new Exception("Stack underflow. Ran out of values before function call at " + position, ex);
            }

            // Evaluate function.
            var evalResult = EvaluateFunctionCall(ref position, functionNameHash, nbParams, param, returnStack, valueStack);

            // Add result on stack as a value.
            if (NanTags.TypeOf(evalResult) != DataType.NoValue)
            {
                valueStack.Push(evalResult);
            } else if (NanTags.DecodeNonValue(evalResult) == NonValueType.Unit) {
                valueStack.Push(evalResult);
            }

            return position;
        }

        // Evaluate a function call
	    public double EvaluateFunctionCall(ref int position, uint functionNameHash, int nbParams, double[] param, Stack<int> returnStack, Stack<double> valueStack)
        {
            var found = Functions.TryGetValue(functionNameHash, out var fun);

            if (!found) {
                throw new Exception("Tried to call an undefined function '"
                                    + DbgStr(functionNameHash)
                                    + "' at position " + position
                                    + "\r\nKnown functions: " + string.Join(", ", Functions.Keys.Select(DbgStr))
                                    + "\r\nAs a string: " + TryDeref(functionNameHash) + "?"
                                    + "\r\nKnown symbols: " + string.Join(", ", DebugSymbols.Keys.Select(DbgStr)));
            }

            if (fun.Kind != FuncKind.Custom)
                return EvaluateBuiltInFunction(ref position, fun.Kind, nbParams, param, returnStack, valueStack);

            // handle functions that are defined in the program
            Variables.PushScope(param); // write parameters into new scope
            returnStack.Push(position); // set position for 'cret' call
            position = Functions[functionNameHash].StartPosition; // move pointer to start of function
            return NanTags.VoidReturn(); // return no value, continue execution elsewhere

        }

        private string TryDeref(uint functionNameHash)
        {
            try
            {
                return _memory.DereferenceString(functionNameHash);
            }
            catch
            {
                return "<not a known string>";
            }
        }

        private string DbgStr(uint hash)
        {
            if (DebugSymbols == null) return hash.ToString("X");
            if ( ! DebugSymbols.ContainsKey(hash)) return "<unknown> " + hash.ToString("X");

            return DebugSymbols[hash] + " ("+hash.ToString("X")+")";
        }

        private double EvaluateBuiltInFunction(ref int position, FuncKind kind, int nbParams, double[] param, Stack<int> returnStack, Stack<double> valueStack)
        {
            switch (kind)
            {
                // each element equal to the last
                case FuncKind.Equal:
                    if (nbParams < 2) throw new Exception("equals ( = ) must have at least two things to compare");
                    return FoldInequality(param, (a, b) => Math.Abs(a - b) <= double.Epsilon);

                // Each element smaller than the last
                case FuncKind.GreaterThan:
                    if (nbParams < 2) throw new Exception("greater than ( > ) must have at least two things to compare");
                    return FoldInequality(param, (a, b) => a > b);

                // Each element larger than the last
                case FuncKind.LessThan:
                    if (nbParams < 2) throw new Exception("less than ( < ) must have at least two things to compare");
                    return FoldInequality(param, (a, b) => a < b);

                // Each element DIFFERENT TO THE LAST (does not check set uniqueness!)
                case FuncKind.NotEqual:
                    if (nbParams < 2) throw new Exception("not-equal ( <> ) must have at least two things to compare");
                    return FoldInequality(param, (a, b) => Math.Abs(a - b) > double.Epsilon);

                case FuncKind.Assert:
                    if (nbParams < 1) return NanTags.VoidReturn(); // assert nothing passes
                    var condition = param.ElementAt(0);
                    if (_memory.CastBoolean(condition) == false)
                    {
                        var msg = ConcatList(param, 1);
                        throw new Exception("Assertion failed: " + msg);
                    }
                    return NanTags.VoidReturn();

                case FuncKind.Random:
                    if (nbParams < 1) return rnd.NextDouble(); // 0 params - any size
                    if (nbParams < 2) return rnd.Next(_memory.CastInt(param.ElementAt(0))); // 1 param  - max size
                    return rnd.Next(_memory.CastInt(param.ElementAt(0)), _memory.CastInt(param.ElementAt(1))); // 2 params - range

                case FuncKind.Eval:
                    var reader = new SourceCodeTokeniser();
                    var statements = _memory.CastString(param.ElementAt(0));
                    var programTmp = reader.Read(statements);
                    var bin = Compiler.Compiler.CompileRoot(programTmp, false);
                    var interpreter = new ByteCodeInterpreter();
                    interpreter.Init(new RuntimeMemoryModel(bin), _input, _output, Variables, DebugSymbols); // todo: optional other i/o for eval?
                    return interpreter.Execute(false, runningVerbose);

                case FuncKind.Call:
                    NanTags.DecodePointer(param.ElementAt(0), out var target, out var type);
                    if (type != DataType.PtrString) throw new Exception("Tried to call a function by name, but passed a '" + type + "' at " + position);
                    // this should be a string, but we need a function name hash -- so calculate it:
                    var strName = _memory.DereferenceString(target);
                    var functionNameHash = NanTags.GetCrushedName(strName);
                    nbParams--;
                    var newParam = param.Skip(1).ToArray();
                    return EvaluateFunctionCall(ref position, functionNameHash, nbParams, newParam, returnStack, valueStack);

                case FuncKind.LogicNot:
                    if (nbParams != 1) throw new Exception("'not' should be called with one argument");
                    var bval = _memory.CastBoolean(param.ElementAt(0));
                    return NanTags.EncodeBool(!bval);

                case FuncKind.LogicOr:
                {
                    bool more = nbParams > 0;
                    int i = 0;
                    while (more)
                    {
                        var bresult = _memory.CastBoolean(param.ElementAt(i));
                        if (bresult) return NanTags.EncodeBool(true);

                        i++;
                        more = i < nbParams;
                    }

                    return NanTags.EncodeBool(false);
                }

                case FuncKind.LogicAnd:
                {
                    bool more = nbParams > 0;
                    int i = 0;
                    while (more)
                    {
                        var bresult = _memory.CastBoolean(param.ElementAt(i));
                        if (!bresult) return NanTags.EncodeBool(false);

                        i++;
                        more = i < nbParams;
                    }

                    return NanTags.EncodeBool(true);
                }

                case FuncKind.ReadKey:
                    return _memory.StoreStringAndGetReference(((char) _input.Read()).ToString());

                case FuncKind.ReadLine:
                    return _memory.StoreStringAndGetReference(_input.ReadLine());

                case FuncKind.Print:
                    {
                        string lastStr = null;
                        foreach (var v in param)
                        {
                            lastStr = _memory.CastString(v);
                            _output.Write(lastStr);
                        }

                        if (lastStr != "") _output.WriteLine();
                    }
                    return NanTags.VoidReturn();

                case FuncKind.Substring:
                    if (nbParams == 2)
                    {

                        var newString = _memory.CastString(param.ElementAt(0)).Substring(_memory.CastInt(param.ElementAt(1)));
                        return _memory.StoreStringAndGetReference(newString);
                    }
                    else if (nbParams == 3)
                    {
                        int start = _memory.CastInt(param.ElementAt(1));
                        int length = _memory.CastInt(param.ElementAt(2));

                        string s = _memory.CastString(param.ElementAt(0)).Substring(start, length);
                        return _memory.StoreStringAndGetReference(s);
                    } else {
                        throw new Exception("'Substring' should be called with 2 or 3 parameters");
                    }

                case FuncKind.Length:
                    return _memory.CastString(param.ElementAt(0)).Length; // TODO: lengths of other things

                case FuncKind.Replace:
                    if (nbParams != 3) throw new Exception("'Replace' should be called with 3 parameters");
                    string exp = _memory.CastString(param.ElementAt(0));
                    string oldValue = _memory.CastString(param.ElementAt(1));
                    string newValue = _memory.CastString(param.ElementAt(2));
                    exp = exp.Replace(oldValue, newValue);
                    return _memory.StoreStringAndGetReference(exp);

                case FuncKind.Concat:
                    var builder = new StringBuilder();

                    foreach (var v in param)
                    {
                        builder.Append(_memory.CastString(v));
                    }

                    return _memory.StoreStringAndGetReference(builder.ToString());

                case FuncKind.UnitEmpty:
                    { // valueless marker (like an empty object)
                        return NanTags.EncodeNonValue(NonValueType.Unit);
                    }

                
                case FuncKind.MathAdd:
                    if (nbParams == 1) return param[0];
                    return param.Sum();

                case FuncKind.MathSub:
                    if (nbParams == 1) return -param[0];
                    return param.ChainDifference();

                case FuncKind.MathProd:
                    if (nbParams == 1) throw new Exception("Uniary '*' is not supported");
                    return param.ChainProduct();

                case FuncKind.MathDiv:
                    if (nbParams == 1) throw new Exception("Uniary '/' is not supported");
                    return param.ChainDivide();

                case FuncKind.MathMod:
                    if (nbParams == 1) return param[0] % 2;
                    return param.ChainRemainder();

                default:
                    throw new Exception("Unrecognised built-in! Type = "+((int)kind));
            }
        }

        private string ConcatList(double[] list, int startIndex)
        {
            var sb = new StringBuilder();
            for (int i = startIndex; i < list.Length; i++)
            {
                sb.Append(_memory.CastString(list[i]));
            }
            return sb.ToString();
        }

        private double FoldInequality(IEnumerable<double> list, Func<double, double, bool> comparitor)
        {
            bool first = true;
            double prev = 0;
            foreach (var encoded in list)
            {
                if (first) {
                    prev = _memory.CastDouble(encoded);
                    first = false;
                    continue;
                }
                var current = _memory.CastDouble(encoded);
                if ( ! comparitor(prev, current)) return NanTags.EncodeBool(false);
                prev = current;
            }
            return NanTags.EncodeBool(true);
        }
    }
}
