﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using MecsCore.Runtime;

namespace MecsCore.InputOutput
{
    /// <summary>
    /// Outputs Nan-Boxed values and opcodes
    /// </summary>
    public class NanCodeWriter
    {
        /// <summary>
        /// Literal strings to be written into data section of code
        /// </summary>
        private readonly List<string> _stringTable;

        /// <summary>
        /// Nan-boxed opcodes and values
        /// </summary>
        private readonly List<double> _opcodes;

        /// <summary>
        /// Names that we've hashed
        /// </summary>
        private readonly HashLookup<string> _symbols;

        /// <summary>
        /// The program fragment expects to produce result values. Defaults to false.
        /// </summary>
        public bool ReturnsValues { get; set; }

        /// <summary>
        /// Create a new writer for each compilation
        /// </summary>
        public NanCodeWriter()
        {
            ReturnsValues = false;
            _stringTable = new List<string>(100);
            _opcodes = new List<double>(1024);
            _symbols = new HashLookup<string>();
        }

        /// <summary>
        /// Read opcode at index
        /// </summary>
        public double this[int idx]
        {
            get
            {
                return _opcodes[idx];
            }
        }


        /// <summary>
        /// Current written opcode position (relative only, for calculating relative jumps)
        /// </summary>
        public int Position()
        {
            return _opcodes.Count;
        }

        /// <summary>
        /// Inject a compiled sub-unit into this writer
        /// References to string constants will be recalculated
        /// </summary>
        public void Merge(NanCodeWriter fragment) {
            var codes = fragment._opcodes;
            var strings = fragment._stringTable;

            AddSymbols(fragment._symbols);

            foreach (var code in codes)
            {
                var type = NanTags.TypeOf(code);
                switch (type) {
                    case DataType.Invalid: throw new Exception("Invalid opcode when merging");

                    case DataType.ValInt32:
                    case DataType.ValUInt32:
                    case DataType.VariableRef:
                    case DataType.Number:
                    case DataType.Opcode:
                        _opcodes.Add(code);
                        break;
                    
                    case DataType.PtrString:
                    case DataType.PtrStaticString:
                        NanTags.DecodePointer(code, out var target, out _);
                        LiteralString(strings[(int)target]);
                        break;

                    // all other types are only expected at runtime (so far)
                    default: throw new Exception("Unexpected opcode when merging");
                }
            }
        }

        /// <summary>
        /// Write opcodes and data section to stream
        /// References to string constants will be recalculated
        /// </summary>
        public void WriteToStream(Stream output) {
            // a string is [NanTag(UInt32): byte length] [string bytes, padded to 8 byte chunks]

            // 1) Calculate the string table size
            var dataLength = _stringTable.Select(CalculatePaddedSize).Sum() + _stringTable.Count;

            // 2) Write a jump command to skip the table
            var jumpCode = NanTags.EncodeLongOpcode('c','s', (uint)dataLength);
            WriteCode(output, jumpCode);

            // 3) Write the strings, with a mapping dictionary
            long location = 1; // counting initial jump as 0
            var mapping = new Dictionary<long, long>(); // original index, final memory location
            for (var index = 0; index < _stringTable.Count; index++)
            {
                var staticStr = _stringTable[index];
                var bytes = Encoding.ASCII.GetBytes(staticStr);
                var chunks = CalculatePaddedSize(staticStr);
                var padSize = (chunks * 8) - bytes.Length;

                mapping.Add(index, location);

                var headerOpCode = NanTags.EncodeUInt32((uint)bytes.Length);
                WriteCode(output, headerOpCode);
                location++;

                output.Write(bytes, 0, bytes.Length);
                location += chunks;

                for (int p = 0; p < padSize; p++) { output.WriteByte(0); }
            }

            // 4) Write the op-codes
            foreach (var code in _opcodes)
            {
                var type = NanTags.TypeOf(code);
                switch (type) {
                    case DataType.PtrString:
                    case DataType.PtrStaticString:
                        NanTags.DecodePointer(code, out var original, out _);
                        var final = mapping[original];
                        WriteCode(output, NanTags.EncodePointer(final, DataType.PtrStaticString));
                        break;

                    default: 
                        WriteCode(output, code);
                        break;
                }
            }

            output.Flush();
        }

        private void WriteCode(Stream output, double code)
        {
            var bytes = BitConverter.GetBytes(code);
            output.Write(bytes, 0, bytes.Length);
        }

        /// <summary>
        /// Number of 8-byte chunks required to store this string
        /// </summary>
        private int CalculatePaddedSize(string s)
        {
            double bytes = Encoding.ASCII.GetByteCount(s);
            return (int)Math.Ceiling(bytes / 8.0d);
        }

        /// <summary>
        /// Add an inline comment into the stream
        /// </summary>
        /// <param name="s"></param>
        public void Comment(string s)
        {
            //_stringTable.Add(s);
            //_opcodes.Add(NanTags.EncodePointer(_stringTable.Count - 1, DataType.PtrDiagnosticString));
        }

        /// <summary>
        /// Reference a value name
        /// </summary>
        public void VariableReference(string valueName)
        {
            _opcodes.Add(NanTags.EncodeVariableRef(valueName, out var crushed));
            AddSymbol(crushed, valueName);
        }

        public void Memory(char action, string targetName, int paramCount)
        {
            uint crush;
            switch (action)
            {
                case 's' when paramCount > 1: //set indexed
                    _opcodes.Add(NanTags.EncodeVariableRef(targetName, out crush));
                    AddSymbol(crush, targetName);
                    _opcodes.Add(NanTags.EncodeOpcode('m', 'S', (ushort)paramCount,0));

                    break;

                case 'g' when paramCount > 0: // get indexed
                    _opcodes.Add(NanTags.EncodeVariableRef(targetName, out crush));
                    AddSymbol(crush, targetName);
                    _opcodes.Add(NanTags.EncodeOpcode('m', 'G', (ushort)paramCount,0));
                    break;

                default:
                    NanTags.EncodeVariableRef(targetName, out crush);
                    AddSymbol(crush, targetName);
                    _opcodes.Add(NanTags.EncodeLongOpcode('m', action, crush));
                    break;
            }
        }

        public void Increment(sbyte incr, string targetName)
        {
            NanTags.EncodeVariableRef(targetName, out var crush);
            AddSymbol(crush, targetName);
            _opcodes.Add(NanTags.EncodeLongOpcode('i', (char)incr, crush));
        }

        public void Memory(char action, double opcode)
        {
            var crush = NanTags.DecodeUInt32(opcode);
            _opcodes.Add(NanTags.EncodeLongOpcode('m', action, crush));
        }

        public void FunctionCall(string functionName, int parameterCount)
        {
            _opcodes.Add(NanTags.EncodeVariableRef(functionName, out _));
            _opcodes.Add(NanTags.EncodeOpcode('f','c', (ushort)parameterCount, 0));
        }

        public uint OpCodeCount()
        {
            return (uint)_opcodes.Count;
        }

        /// <summary>
        /// Add a define-and-skip set of opcodes *before* merging in the compiled function opcodes.
        /// </summary>
        public void FunctionDefine(string functionName, int argCount, uint tokenCount)
        {
            _opcodes.Add(NanTags.EncodeVariableRef(functionName, out var crushed));
            AddSymbol(crushed, functionName);
            _opcodes.Add(NanTags.EncodeOpcode('f','d', (ushort)argCount, (ushort)tokenCount));
        }

        /// <summary>
        /// Add a symbol set to the known symbols table
        /// </summary>
        public void AddSymbols(HashLookup<string> sym)
        {
            foreach (var symbol in sym)
            {
                AddSymbol(symbol.Key, symbol.Value);
            }
        }

        /// <summary>
        /// Add a single symbol reference
        /// </summary>
        public void AddSymbol(uint crushed, string name)
        {
            try {
                _symbols.Add(crushed, name);
            } catch {
                if (name == _symbols[crushed]) return; // same symbol.

                throw new Exception("Hash collision between symbols!" +
                                    " This is a compiler limitation, sorry." +
                                    " Try renaming '"+_symbols[crushed]+"' or '"+name+"'");
            }
        }

        /// <summary>
        /// Function return that should not happen
        /// </summary>
        public void InvalidReturn()
        {
            _opcodes.Add(NanTags.EncodeOpcode('c', 't', 0, 0));
        }

        public void Return(int pCount)
        {
            _opcodes.Add(NanTags.EncodeOpcode('c', 'r', 0, (ushort)pCount));
        }

        /// <summary>
        /// Encode a compound compare
        /// </summary>
        /// <param name="operation">compare to do</param>
        /// <param name="argCount">number of value-stack args to compare</param>
        /// <param name="opCodeCount">opcodes to jump relative down if top if comparison result is false</param>
        public void CompoundCompareJump(CmpOp operation, ushort argCount, ushort opCodeCount)
        {
            _opcodes.Add(NanTags.EncodeOpcode('C', (byte)operation, argCount, opCodeCount));
        }

        /// <summary>
        /// Jump relative down if top of value-stack is false
        /// </summary>
        public void CompareJump(uint opCodeCount)
        {
            _opcodes.Add(NanTags.EncodeLongOpcode('c', 'c', opCodeCount));
        }

        /// <summary>
        /// Jump relative up
        /// </summary>
        public void UnconditionalJump(uint opCodeCount)
        {
            _opcodes.Add(NanTags.EncodeLongOpcode('c', 'j', opCodeCount));
        }

        public void LiteralNumber(double d)
        {
            _opcodes.Add(d);
        }

        /// <summary>
        /// Write a static string.
        /// Static strings aren't seen by the GC and exist in memory outside of the normal allocation space
        /// </summary>
        public void LiteralString(string s)
        {
            // duplication check
            if (_stringTable.Contains(s)) {
                var idx = _stringTable.IndexOf(s);
                _opcodes.Add(NanTags.EncodePointer(idx, DataType.PtrStaticString));
                return;
            }

            // no existing matches
            _stringTable.Add(s);
            _opcodes.Add(NanTags.EncodePointer(_stringTable.Count - 1, DataType.PtrStaticString));
        }

        public void LiteralInt32(int i)
        {
            _opcodes.Add(NanTags.EncodeInt32(i));
        }

        public void RawToken(double value)
        {
            _opcodes.Add(value);
        }

        /// <summary>
        /// Return the original names of variable references we've hashed.
        /// Keys are the Variable Ref byte codes
        /// </summary>
        public HashLookup<string> GetSymbols()
        {
            return _symbols;
        }
    }
}