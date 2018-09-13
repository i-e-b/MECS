using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace EvieCompilerSystem.InputOutput
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
        }

        /// <summary>
        /// Inject a compiled sub-unit into this writer
        /// References to string constants will be recalculated
        /// </summary>
        public void Merge(NanCodeWriter fragment) {
            var codes = fragment._opcodes;
            var strings = fragment._stringTable;

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
            Split16(dataLength, out var lower, out var upper);
            var jumpCode = NanTags.EncodeOpcode('c','s', lower, upper);
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
                        NanTags.DecodePointer(code, out var original, out _);
                        var final = mapping[original];
                        WriteCode(output, NanTags.EncodePointer(final, DataType.PtrString));
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
            _stringTable.Add(s);
            _opcodes.Add(NanTags.EncodePointer(_stringTable.Count - 1, DataType.PtrDiagnosticString));
        }

        /// <summary>
        /// Reference a value name
        /// </summary>
        public void VariableReference(string valueName)
        {
            _opcodes.Add(NanTags.EncodeVariableRef(valueName, out _));
        }

        public void Memory(char action)
        {
            _opcodes.Add(NanTags.EncodeOpcode('m', action, 0, 0));
        }

        public void FunctionCall(string functionName, int parameterCount)
        {
            _opcodes.Add(NanTags.EncodeVariableRef(functionName, out _));
            _opcodes.Add(NanTags.EncodeOpcode('f','c', (ushort)parameterCount, 0));
        }

        public int OpCodeCount()
        {
            return _opcodes.Count;
        }

        /// <summary>
        /// Add a define-and-skip set of opcodes *before* merging in the compiled function opcodes.
        /// </summary>
        public void FunctionDefine(string functionName, int argCount, int tokenCount)
        {
            _opcodes.Add(NanTags.EncodeVariableRef(functionName, out _));
            _opcodes.Add(NanTags.EncodeOpcode('f','d', (ushort)argCount, (ushort)tokenCount));
        }

        /// <summary>
        /// Function return that should not happen
        /// </summary>
        public void InvalidReturn()
        {
            _opcodes.Add(NanTags.EncodeOpcode('c', 't', 0, 0));
        }

        public void Return()
        {
            _opcodes.Add(NanTags.EncodeOpcode('c', 'r', 0, 0));
        }

        private void Split16(int longVal, out ushort p1, out ushort p2) {
            unchecked{
                p1 =(ushort)(longVal & 0xFFFF);
                p2 =(ushort)(longVal >> 16);
            }
        }

        /// <summary>
        /// Jump relative down if top of value-stack is false
        /// </summary>
        public void CompareJump(int opCodeCount)
        {
            Split16(opCodeCount, out var lower, out var upper);
            _opcodes.Add(NanTags.EncodeOpcode('c', 'c', lower, upper));
        }

        /// <summary>
        /// Jump relative up
        /// </summary>
        public void UnconditionalJump(int opCodeCount)
        {
            Split16(opCodeCount, out var lower, out var upper);
            _opcodes.Add(NanTags.EncodeOpcode('c', 'j', lower, upper));
        }

        public void LiteralNumber(double d)
        {
            _opcodes.Add(d);
        }

        public void LiteralString(string s)
        {
            // a string is [NanTag(UInt32): byte length] [string bytes, padded to 8 byte chunks]
            _stringTable.Add(s);
            _opcodes.Add(NanTags.EncodePointer(_stringTable.Count - 1, DataType.PtrString));
        }

        public void LiteralInt32(int i)
        {
            _opcodes.Add(NanTags.EncodeInt32(i));
        }

        public void RawToken(double value)
        {
            _opcodes.Add(value);
        }
    }
}