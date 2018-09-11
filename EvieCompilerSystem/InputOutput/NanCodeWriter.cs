using System.Collections.Generic;
using System.IO;

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
            TODO_IMPLEMENT_ME;
        }

        /// <summary>
        /// Write opcodes to stream, then data section.
        /// References to string constants will be recalculated
        /// </summary>
        public void WriteToStream(Stream output) {
            TODO_IMPLEMENT_ME;
            // a string is [NanTag(UInt32): byte length] [string bytes, padded to 8 byte chunks]
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

        /// <summary>
        /// Jump relative down if top of value-stack is false
        /// </summary>
        public void CompareJump(int opCodeCount)
        {
            _opcodes.Add(NanTags.EncodeOpcode('c', 'c', (ushort)opCodeCount, 0));
        }

        /// <summary>
        /// Jump relative up
        /// </summary>
        public void UnconditionalJump(int opCodeCount)
        {
            _opcodes.Add(NanTags.EncodeOpcode('c','j', (ushort)opCodeCount, 0));
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