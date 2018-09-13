using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

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
            var header = encodedTokens[(int)position];
            if (NanTags.TypeOf(header) != DataType.ValUInt32) throw new Exception("String header was not a UInt32");
            var length = NanTags.DecodeUInt32(header);

            //  2) calculate chunk count and read the chunks
            var chunkCount = (int)Math.Ceiling(length / 8.0d);
            var rawBytes = encodedTokens.GetRange((int)position+1, chunkCount).SelectMany(BitConverter.GetBytes).ToArray();

            //  4) make string (ascii for now, then utf-8?)
            return Encoding.ASCII.GetString(rawBytes, 0, (int)length);
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
            var type = NanTags.TypeOf(encoded);
            switch (type){
                case DataType.Invalid: return "<invalid value>";
                case DataType.Number: return encoded.ToString(CultureInfo.InvariantCulture);
                case DataType.VariableRef: return "<Reference>";
                case DataType.Opcode: return "<Op Code>";
                case DataType.NoValue: return "<NAR>";

                case DataType.PtrDiagnosticString:
                case DataType.PtrString:
                    NanTags.DecodePointer(encoded, out var target, out _);
                    return DereferenceString(target);

                case DataType.PtrHashtable:
                case DataType.PtrGrid:
                case DataType.PtrArray_Int32:
                case DataType.PtrArray_UInt32:
                case DataType.PtrArray_String:
                case DataType.PtrArray_Double:
                case DataType.PtrSet_String:
                case DataType.PtrSet_Int32:
                case DataType.PtrLinkedList:
                    return "<complex type>"; // TODO: add strigification later

                case DataType.ValInt32: return NanTags.DecodeInt32(encoded).ToString();
                case DataType.ValUInt32: return NanTags.DecodeUInt32(encoded).ToString();

                default:
                    throw new ArgumentOutOfRangeException();
            }
        }

        /// <summary>
        /// Cast a value to int. If not applicable, returns zero
        /// </summary>
        public int CastInt(double encoded)
        {
            var type = NanTags.TypeOf(encoded);
            switch (type){
                case DataType.Invalid:
                case DataType.VariableRef:
                case DataType.Opcode:
                case DataType.NoValue:
                    return 0;

                case DataType.PtrDiagnosticString:
                case DataType.PtrString:
                    NanTags.DecodePointer(encoded, out var target, out _);
                    int.TryParse(DereferenceString(target), out var result);
                    return result;

                case DataType.PtrHashtable:
                case DataType.PtrGrid:
                case DataType.PtrArray_Int32:
                case DataType.PtrArray_UInt32:
                case DataType.PtrArray_String:
                case DataType.PtrArray_Double:
                case DataType.PtrSet_String:
                case DataType.PtrSet_Int32:
                case DataType.PtrLinkedList:
                    return 0;

                case DataType.Number: return (int)encoded;
                case DataType.ValInt32: return NanTags.DecodeInt32(encoded);
                case DataType.ValUInt32: return (int)NanTags.DecodeUInt32(encoded);

                default:
                    throw new ArgumentOutOfRangeException();
            }
        }

        /// <summary>
        /// Store a new string at the end of memory, and return a string pointer token for it
        /// </summary>
        public double StoreStringAndGetReference(string str)
        {
            var location = encodedTokens.Count;

            var bytes = Encoding.ASCII.GetBytes(str);
            var headerOpCode = NanTags.EncodeUInt32((uint)bytes.Length);

            encodedTokens.Add(headerOpCode);

            long pack = 0;
            int rem = 56;
            for (int i = 0; i < bytes.Length; i++)
            {
                pack |= ((long)bytes[i]) << rem;

                if (rem <= 0) {
                    rem = 64;
                    encodedTokens.Add(BitConverter.Int64BitsToDouble(pack));
                    pack = 0;
                }

                rem -= 8;
            }
            
            encodedTokens.Add(BitConverter.Int64BitsToDouble(pack)); // might be a bit wasteful if alignment happened to be perfect, but meh.


            return location;
        }
    }
}