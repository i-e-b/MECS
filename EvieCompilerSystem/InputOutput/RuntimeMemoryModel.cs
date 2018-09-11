using System;
using System.Collections.Generic;
using System.IO;

namespace EvieCompilerSystem.InputOutput
{
    public class RuntimeMemoryModel
    {
        private readonly List<double> encodedTokens;

        public RuntimeMemoryModel(NanCodeWriter writer)
        {
            var ms = new MemoryStream(writer.OpCodeCount() * 16);
            writer.WriteToStream(ms);
            ms.Seek(0, SeekOrigin.Begin);

            encodedTokens = new List<double>((int)(ms.Length / 8));
            var raw = ms.ToArray();

            for (int i = 0; i < raw.Length; i+=8)
            {
                encodedTokens.Add(BitConverter.ToDouble(raw, i));
            }
        }

        public RuntimeMemoryModel(MemoryStream stream)
        {
            encodedTokens = new List<double>((int)(stream.Length / 8));
            var raw = stream.ToArray();

            for (int i = 0; i < raw.Length; i+=8)
            {
                encodedTokens.Add(BitConverter.ToDouble(raw, i));
            }
        }

        public List<double> Tokens()
        {
            return encodedTokens;
        }


        public string DereferenceString(long position) {
            // a string is [NanTag(UInt32): byte length] [string bytes, padded to 8 byte chunks]
            // The plan:
            //  1) read the byte length
            //  2) calculate chunk count and read the chunks
            //  3) chunks to bytes
            //  4) make string (ascii for now, then utf-8?)
        }
        
        public bool CastBoolean(double encoded)
        {
            var type = NanTags.TypeOf(encoded);

            switch (type) {
                case DataType.Number:
                    return Math.Abs(encoded) <= double.Epsilon;
               
                case DataType.ValInt32:
                    return NanTags.DecodeInt32(encoded) != 0;

                case DataType.ValUInt32:
                    return NanTags.DecodeUInt32(encoded) != 0;

                case DataType.PtrString:
                    { // null, empty, "false" or "0" are false. All other strings are true.
                        NanTags.DecodePointer(encoded, out var ptr, out _);
                        var strVal = DereferenceString(ptr);
                        if (string.IsNullOrEmpty(strVal)) return false;
                        if (string.Equals(strVal, "false", StringComparison.OrdinalIgnoreCase)) return false;
                        if (string.Equals(strVal, "0", StringComparison.OrdinalIgnoreCase)) return false;
                        return true;
                    }

                // All the things that can't be meaningfully cast are 'false'
                default: return false;
            }
        }

        public double CastDouble(double encoded)
        {
            var type = NanTags.TypeOf(encoded);

            switch (type) {
                case DataType.Number:
                    return encoded;
               
                case DataType.ValInt32:
                    return NanTags.DecodeInt32(encoded);

                case DataType.ValUInt32:
                    return NanTags.DecodeUInt32(encoded);

                // All the things that can't be meaningfully cast
                default: return double.NaN;
            }
        }

        /// <summary>
        /// Get a resonable string representation from a value.
        /// This should include stringifying non-string types (numbers, structures etc)
        /// </summary>
        public string CastString(double encoded)
        {
            return TODO_IMPLEMENT_ME;
        }

        /// <summary>
        /// Cast a value to int. If not applicable, returns zero
        /// </summary>
        public int CastInt(double encoded)
        {
            return TODO_IMPLEMENT_ME;
        }

        /// <summary>
        /// Store a new string at the end of memory, and return a string pointer token for it
        /// </summary>
        public double StoreStringAndGetReference(string toString)
        {
            return TODO_IMPLEMENT_ME;
        }
    }
}