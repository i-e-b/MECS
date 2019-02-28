﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using MecsCore.Compiler;
using MecsCore.InputOutput;
using MecsCore.Utils;

namespace MecsCore.Runtime
{
    public struct ExecutionResult {
        /// <summary>
        /// How the interpreter stopped
        /// </summary>
        public ExecutionState State;

        /// <summary>
        /// If completed, the result of execution
        /// </summary>
        public double Result;
    }

    public enum ExecutionState
    {
        /// <summary>
        /// Program could continue, but stopped by request (debug, step, etc.)
        /// </summary>
        Paused,

        /// <summary>
        /// Program ran to completion
        /// </summary>
        Complete
    }

    public class ByteCodeInterpreter
    {
        /// <summary>
        /// Any difference less than or equal to this, are considered equal numbers
        /// </summary>
        public const double ComparisonPrecision = 1e-20;

        private List<double> program;
        public HashLookup<FunctionDefinition>  Functions;
        public static HashLookup<string> DebugSymbols;
        public static Random rnd = new Random();
       
        private int _stepsTaken;
        private TextReader _input;
        private TextWriter _output;
        private RuntimeMemoryModel _memory;
        private bool _runningVerbose;
        private int _position; // the PC. This is in TOKEN positions, not bytes

        private Stack<double> _valueStack;
        private Stack<int> _returnStack;

        public void Init(RuntimeMemoryModel bin, TextReader input, TextWriter output, HashLookup<string> debugSymbols = null)
        {
            _position = 0;
            _memory = bin;

            Functions = BuiltInFunctionSymbols();

            program = bin.Tokens();

            DebugSymbols = debugSymbols;

            _input  = input;
            _output = output;

		    _valueStack = new Stack<double>();
            _returnStack = new Stack<int>(); // absolute position for call and return
        }


        /// <summary>
        /// Main loop.
        /// Good default for all the flags is `false`
        /// </summary>
        /// <param name="resetVars">If true, all scopes are DELETED before running</param>
        /// <param name="traceExecution">If true, console output of state is written</param>
        /// <param name="singleStep">If true, the interpreter will run a single step, then return. Internal `eval` statements will run to completion</param>
        public ExecutionResult Execute(bool resetVars, bool traceExecution, bool singleStep)
        {
		    double evalResult;
            _runningVerbose = traceExecution;

            if (resetVars) { _memory.Variables.Clear(); }

            while (_position < program.Count)
            {
			    _stepsTaken++;
                //if (stepsTaken > 1000) throw new Exception("trap");
                
                // Prevent stackoverflow.
                // Ex: if(true 1 10 20)
			    if ((_stepsTaken & 127) == 0 && _valueStack.Count > 100)
                {
                    var oldValues = _valueStack.ToArray();
                    _valueStack = new Stack<double>(oldValues.Skip(oldValues.Length - 100));
                }

                double word = program[_position];

                if (traceExecution)
                {
                    _output.WriteLine("          stack :"+string.Join(", ",_valueStack.ToArray().Select(t=> _memory.DiagnosticString(t, DebugSymbols))));
                    _output.WriteLine("          #" + _stepsTaken + "; p="+_position
                                      +"; w="+_memory.DiagnosticString(word, DebugSymbols) );
                }

                var type = NanTags.TypeOf(word);
                switch (type) {
                    case DataType.Invalid:
                        throw new Exception("Unknown code point at " + _position);

                    case DataType.Opcode:
                        // decode opcode and do stuff
                        NanTags.DecodeOpCode(word, out var codeClass, out var codeAction, out var p1, out var p2);
                        ProcessOpCode(codeClass, codeAction, p1, p2, ref _position, _valueStack, _returnStack, word);
                        break;

                    default:
                        _valueStack.Push(word); // these could be raw doubles, encoded real values, or references/pointers
                        break;
                }

                
                _position++;
                if (singleStep) return new ExecutionResult{ State = ExecutionState.Paused, Result = NanTags.EncodeNonValue(NonValueType.Not_a_Result)};
		    }

		    if (_valueStack.Count != 0)
            {
                evalResult = _valueStack.Pop();
		    }
            else
            {
			    evalResult = NanTags.EncodeNonValue(NonValueType.Void);
		    }

		    _valueStack.Clear();

            return new ExecutionResult { State = ExecutionState.Complete, Result = evalResult };
        }

        /// <summary>
        /// Core instruction dispatch
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private void ProcessOpCode(char codeClass, char codeAction, ushort p1, ushort p2, ref int position, Stack<double> valueStack, Stack<int> returnStack, double word)
        {
            unchecked
            {
                uint varRef;
                switch (codeClass)
                {
                    case 'f': // Function operations
                        if (codeAction == 'c')
                        {
                            position = PrepareFunctionCall(position, p1, valueStack, returnStack);
                        }
                        else if (codeAction == 'd')
                        {
                            position = HandleFunctionDefinition(p1, p2, position, valueStack);
                        }
                        break;

                    case 'c': // flow Control -- conditions, jumps etc
                        {
                            int opCodeCount = p2 + (p1 << 16); // we use 31 bit jumps, in case we have lots of static data
                            position = HandleControlSignal(codeAction, opCodeCount, valueStack, returnStack, position);
                        }
                        break;

                    case 'C': // compound compare and jump
                        {
                            position = HandleCompoundCompare((byte)codeAction, p1, p2, valueStack, position);
                        }
                        break;

                    case 'm': // Memory access - get|set|isset|unset
                        varRef = p2 + ((uint)p1 << 16);
                        HandleMemoryAccess(codeAction, valueStack, position, varRef, p1);
                        break;

                    case 'i': // special 'increment' operator. Uses the 'codeAction' slot to hold a small signed number
                        varRef = p2 + ((uint)p1 << 16);
                        _memory.Variables.MutateNumber(varRef, (sbyte)codeAction);
                        break;

                    case 's': // reserved for System operation.
                        throw new Exception("Unimplemented System operation op code at " + position + " : " + _memory.DiagnosticString(word, DebugSymbols));

                    default:
                        throw new Exception("Unexpected op code at " + position + " : " + _memory.DiagnosticString(word, DebugSymbols));
                }
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

        private void HandleMemoryAccess(char action, Stack<double> valueStack, int position, uint varRef, ushort paramCount)
        {
            switch (action)
            {
                case 'g': // get (adds a value to the stack, false if not set)
                    valueStack.Push(_memory.Variables.Resolve(varRef));
                    break;

                case 's': // set
                    if (valueStack.Count < 1) throw new Exception("There were no values to save. Did you forget a `return` in a function? Position: " + position);
                    _memory.Variables.SetValue(varRef, valueStack.Pop());
                    break;

                case 'i': // is set? (adds a bool to the stack)
                    valueStack.Push(NanTags.EncodeBool(_memory.Variables.CanResolve(varRef)));
                    break;

                case 'u': // unset
                    _memory.Variables.Remove(varRef);
                    break;

                case 'G': // indexed get
                    DoIndexedGet(valueStack, paramCount);
                    break;

                default:
                    throw new Exception("Unknown memory opcode: '" + action + "'");
            }
        }

        private void DoIndexedGet(Stack<double> valueStack, ushort paramCount)
        {
            var target = valueStack.Pop();
            var value = _memory.Variables.Resolve(NanTags.DecodeVariableRef(target));
            var type = NanTags.TypeOf(value);
            switch (type)
            {
                // Note: Numeric types should read the bit at index
                // Hashes and arrays do the obvious lookup
                // Sets return true/false for occupancy
                case DataType.PtrString:
                case DataType.PtrStaticString:
                    // get the other indexes. If more than one, build a string out of the bits?
                    // What to do with out-of-range?
                    var str = _memory.DereferenceString(NanTags.DecodePointer(value));
                    var sb = new StringBuilder(paramCount - 1);
                    for (int i = 0; i < paramCount; i++)
                    {
                        var idx = _memory.CastInt(valueStack.Pop());
                        if (idx >= 0 && idx < str.Length) sb.Append(str[idx]);
                    }

                    var result = _memory.StoreStringAndGetReference(sb.ToString());
                    valueStack.Push(result);
                    return;

                default:
                    throw new Exception("Indexing of type '" + type + "' is not yet supported");
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

                default:
                    throw new Exception("Unknown control signal '" + action + "'");
            }

            return position;
        }

        private int HandleCompoundCompare(byte action, ushort argCount, ushort opCodeCount, Stack<double> valueStack, int position)
        {
            var param = ReadParams(position, argCount, valueStack);
            var cmp = (CmpOp)action;
            switch (cmp) {
                case CmpOp.Equal:
                    return ListEquals(param) ? position : position + opCodeCount;
                case CmpOp.NotEqual:
                    return ListEquals(param) ? position + opCodeCount : position;
                case CmpOp.Less:
                    return FoldLessThan(param) ? position : position + opCodeCount;
                case CmpOp.Greater:
                    return FoldGreaterThan(param) ? position : position + opCodeCount;
                default:
                    throw new ArgumentOutOfRangeException();
            }
        }

        private void HandleReturn(ref int position, Stack<int> returnStack)
        {
            if (returnStack.Count < 1) throw new Exception("Return stack empty. Check program logic");
            _memory.Variables.DropScope();
            position = returnStack.Pop();
        }
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private int PrepareFunctionCall(int position, ushort nbParams, Stack<double> valueStack, Stack<int> returnStack)
        {
            var functionNameHash = NanTags.DecodeVariableRef(valueStack.Pop());

            var param = ReadParams(position, nbParams, valueStack);

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
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static double[] ReadParams(int position, ushort nbParams, Stack<double> valueStack)
        {
            var param = new double[nbParams];
            // Pop values from stack into a param cache
            try
            {
                for (int i = nbParams - 1; i >= 0; i--)
                {
                    param[i] = valueStack.Pop();
                }
            }
            catch (Exception ex)
            {
                throw new Exception("Stack underflow. Ran out of values before function call at " + position, ex);
            }

            return param;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
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

            if (fun.Kind != FuncDef.Custom)
                return EvaluateBuiltInFunction(ref position, fun.Kind, nbParams, param, returnStack, valueStack);

            // handle functions that are defined in the program
            _memory.Variables.PushScope(param); // write parameters into new scope
            returnStack.Push(position); // set position for 'cret' call
            position = Functions.Get(functionNameHash).StartPosition; // move pointer to start of function
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

        private double EvaluateBuiltInFunction(ref int position, FuncDef kind, int nbParams, double[] param, Stack<int> returnStack, Stack<double> valueStack)
        {
            switch (kind)
            {
                // each element equal to the first
                case FuncDef.Equal:
                    if (nbParams < 2) throw new Exception("equals ( = ) must have at least two things to compare");
                    return NanTags.EncodeBool(ListEquals(param));

                // Each element smaller than the last
                case FuncDef.GreaterThan:
                    if (nbParams < 2) throw new Exception("greater than ( > ) must have at least two things to compare");
                    return NanTags.EncodeBool(FoldGreaterThan(param));

                // Each element larger than the last
                case FuncDef.LessThan:
                    if (nbParams < 2) throw new Exception("less than ( < ) must have at least two things to compare");
                    return NanTags.EncodeBool(FoldLessThan(param));

                // Each element DIFFERENT TO THE FIRST (does not check set uniqueness!)
                case FuncDef.NotEqual:
                    if (nbParams < 2) throw new Exception("not-equal ( <> ) must have at least two things to compare");
                    return NanTags.EncodeBool(! ListEquals(param));

                case FuncDef.Assert:
                    if (nbParams < 1) return NanTags.VoidReturn(); // assert nothing passes
                    var condition = param.ElementAt(0);
                    if (_memory.CastBoolean(condition) == false)
                    {
                        var msg = ConcatList(param, 1);
                        throw new Exception("Assertion failed: " + msg);
                    }
                    return NanTags.VoidReturn();

                case FuncDef.Random:
                    if (nbParams < 1) return rnd.NextDouble(); // 0 params - any size
                    if (nbParams < 2) return rnd.Next(_memory.CastInt(param.ElementAt(0))); // 1 param  - max size
                    return rnd.Next(_memory.CastInt(param.ElementAt(0)), _memory.CastInt(param.ElementAt(1))); // 2 params - range

                case FuncDef.Eval:
                    var reader = new SourceCodeTokeniser();
                    var statements = _memory.CastString(param.ElementAt(0));
                    var programTmp = reader.Read(statements, false);
                    var bin = ToNanCodeCompiler.CompileRoot(programTmp, false);
                    var interpreter = new ByteCodeInterpreter();
                    interpreter.Init(new RuntimeMemoryModel(bin, _memory.Variables), _input, _output, DebugSymbols);
                    return interpreter.Execute(false, _runningVerbose, false).Result;

                case FuncDef.Call:
                    NanTags.DecodePointer(param.ElementAt(0), out var target, out var type);
                    if (type != DataType.PtrString && type != DataType.PtrStaticString)
                        throw new Exception("Tried to call a function by name, but passed a '" + type + "' at " + position);
                    // this should be a string, but we need a function name hash -- so calculate it:
                    var strName = _memory.DereferenceString(target);
                    var functionNameHash = NanTags.GetCrushedName(strName);
                    nbParams--;
                    var newParam = param.Skip(1).ToArray();
                    return EvaluateFunctionCall(ref position, functionNameHash, nbParams, newParam, returnStack, valueStack);

                case FuncDef.LogicNot:
                    if (nbParams != 1) throw new Exception("'not' should be called with one argument");
                    var bval = _memory.CastBoolean(param.ElementAt(0));
                    return NanTags.EncodeBool(!bval);

                case FuncDef.LogicOr:
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

                case FuncDef.LogicAnd:
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

                case FuncDef.ReadKey:
                    return _memory.StoreStringAndGetReference(((char) _input.Read()).ToString());

                case FuncDef.ReadLine:
                    return _memory.StoreStringAndGetReference(_input.ReadLine());

                case FuncDef.Print:
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

                case FuncDef.Substring:
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

                case FuncDef.Length:
                    return _memory.CastString(param.ElementAt(0)).Length;

                case FuncDef.Replace:
                    if (nbParams != 3) throw new Exception("'Replace' should be called with 3 parameters");
                    string exp = _memory.CastString(param.ElementAt(0));
                    string oldValue = _memory.CastString(param.ElementAt(1));
                    string newValue = _memory.CastString(param.ElementAt(2));
                    exp = exp.Replace(oldValue, newValue);
                    return _memory.StoreStringAndGetReference(exp);

                case FuncDef.Concat:
                    var builder = new StringBuilder();

                    foreach (var v in param)
                    {
                        builder.Append(_memory.CastString(v));
                    }

                    return _memory.StoreStringAndGetReference(builder.ToString());

                case FuncDef.UnitEmpty:
                    { // valueless marker (like an empty object)
                        return NanTags.EncodeNonValue(NonValueType.Unit);
                    }

                
                case FuncDef.MathAdd:
                    if (nbParams == 1) return param[0];
                    return param.ChainSum();

                case FuncDef.MathSub:
                    if (nbParams == 1) return -param[0];
                    return param.ChainDifference();

                case FuncDef.MathProd:
                    if (nbParams == 1) throw new Exception("Uniary '*' is not supported");
                    return param.ChainProduct();

                case FuncDef.MathDiv:
                    if (nbParams == 1) throw new Exception("Uniary '/' is not supported");
                    return param.ChainDivide();

                case FuncDef.MathMod:
                    if (nbParams == 1) return param[0] % 2;
                    return param.ChainRemainder();

                default:
                    throw new Exception("Unrecognised built-in! Type = "+((int)kind));
            }
        }

        /// <summary>
        /// First param is compared all others. If *any* are equal, the function returns true.
        /// </summary>
        private bool ListEquals(double[] list)
        {
            var type = NanTags.TypeOf(list[0]);
            switch (type)
            {
                // Non-comparable types
                case DataType.Invalid:
                case DataType.NoValue:
                case DataType.Opcode:
                    return false;

                // Numeric types
                case DataType.Number:
                case DataType.ValInt32:
                case DataType.ValUInt32:
                    {
                        var target = _memory.CastDouble(list[0]);
                        for (int i = 1; i < list.Length; i++)
                        {
                            if (Math.Abs(target - _memory.CastDouble(list[i])) <= ComparisonPrecision) return true;
                        }
                        return false;
                    }

                // String types
                case DataType.ValSmallString:
                case DataType.PtrStaticString:
                case DataType.PtrString:
                    {
                        var target = _memory.CastString(list[0]);
                        for (int i = 1; i < list.Length; i++)
                        {
                            if (target == _memory.CastString(list[i])) return true;
                        }
                        return false;
                    }

                // Pointer equality
                case DataType.VariableRef:
                case DataType.PtrHashtable:
                case DataType.PtrGrid:
                case DataType.PtrSet:
                case DataType.PtrVector:
                    {
                        var target = NanTags.DecodeRaw(list[0]);
                        for (int i = 1; i < list.Length; i++)
                        {
                            if (target == NanTags.DecodeRaw(list[i])) return true;
                        }
                        return false;
                    }

                default:
                    throw new ArgumentOutOfRangeException();
            }
        }

        
        /// <summary>
        /// Last interpreter position
        /// </summary>
        public int LastPosition() {
            return _position;
        }

        /// <summary>
        /// Symbol mapping for built-in functions
        /// </summary>
        public static HashLookup<FunctionDefinition> BuiltInFunctionSymbols()
        {
            var tmp = new HashLookup<FunctionDefinition>(64);
            Action<string, FuncDef> add = (name, type) => {
                tmp.Add(NanTags.GetCrushedName(name), new FunctionDefinition{ Kind = type});
            };

            add("=", FuncDef.Equal); add("equals", FuncDef.Equal); add(">", FuncDef.GreaterThan);
            add("<", FuncDef.LessThan); add("<>", FuncDef.NotEqual); add("not-equal", FuncDef.NotEqual);
            add("assert", FuncDef.Assert); add("random", FuncDef.Random); add("eval", FuncDef.Eval);
            add("call", FuncDef.Call); add("not", FuncDef.LogicNot); add("or", FuncDef.LogicOr);
            add("and", FuncDef.LogicAnd); add("readkey", FuncDef.ReadKey); add("readline", FuncDef.ReadLine);
            add("print", FuncDef.Print); add("substring", FuncDef.Substring);
            add("length", FuncDef.Length); add("replace", FuncDef.Replace); add("concat", FuncDef.Concat);
            add("+", FuncDef.MathAdd); add("-", FuncDef.MathSub); add("*", FuncDef.MathProd);
            add("/", FuncDef.MathDiv); add("%", FuncDef.MathMod);
            add("()", FuncDef.UnitEmpty); // empty value marker
            return tmp;
        }

        public static HashLookup<string> DebugSymbolsForBuiltIns()
        {
            var tmp = new HashLookup<string>(64);
            Action<string> add = name => { tmp.Add(NanTags.GetCrushedName(name), name); };

            add("="); add("equals"); add(">"); add("<"); add("<>"); add("not-equal");
            add("assert"); add("random"); add("eval"); add("call"); add("not"); add("or");
            add("and"); add("readkey"); add("readline"); add("print"); add("substring");
            add("length"); add("replace"); add("concat"); add("+"); add("-"); add("*");
            add("/"); add("%"); add("()");
            return tmp;
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

        private bool FoldLessThan(double[] list)
        {
            bool first = true;
            double prev = 0;
            for (var i = 0; i < list.Length; i++)
            {
                var encoded = list[i];
                if (first)
                {
                    prev = _memory.CastDouble(encoded);
                    first = false;
                    continue;
                }

                var current = _memory.CastDouble(encoded);
                if (prev >= current) return false; // inverted for short-circuit
                prev = current;
            }

            return true;
        }
        
        private bool FoldGreaterThan(double[] list)
        {
            bool first = true;
            double prev = 0;
            for (var i = 0; i < list.Length; i++)
            {
                var encoded = list[i];
                if (first)
                {
                    prev = _memory.CastDouble(encoded);
                    first = false;
                    continue;
                }

                var current = _memory.CastDouble(encoded);
                if (prev <= current) return false; // inverted for short-circuit
                prev = current;
            }

            return true;
        }
    }
}
