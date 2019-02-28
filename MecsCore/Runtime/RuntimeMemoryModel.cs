using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using MecsCore.InputOutput;

namespace MecsCore.Runtime
{
    public class RuntimeMemoryModel
    {
        /// <summary>
        /// Scope for parameters and variables
        /// </summary>
        public readonly Scope Variables;

        // nancode and static strings
        private readonly List<double> encodedTokens;

        public RuntimeMemoryModel(NanCodeWriter writer, Scope parentScope)
        {
            Variables = new Scope(parentScope);
            var ms = new MemoryStream((int)writer.OpCodeCount() * 16);
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
            Variables = new Scope();
            encodedTokens = new List<double>((int)(stream.Length / 8));
            var raw = stream.ToArray();

            for (int i = 0; i < raw.Length; i+=8)
            {
                encodedTokens.Add(BitConverter.ToDouble(raw, i));
            }
        }

        /// <summary>
        /// Produce a diagnostic description of the memory layout
        /// </summary>
        public string ToString(HashLookup<string> debugSymbols)
        {
            int index = 0;
            var sb = new StringBuilder();

            // Try to display static strings meaningfully
            if (NanTags.TypeOf(encodedTokens[0]) == DataType.Opcode) {
                index = 1;
                NanTags.DecodeLongOpCode(encodedTokens[0], out var c1, out var c2, out var count);
                if (c1 == 'c' && c2 == 's') {
                    sb.AppendLine("Data table: "+count+" tokens ("+(count*8)+" bytes)");
                    while (index < count)
                    {
                        var length = NanTags.DecodeUInt32(encodedTokens[index]);
                        var chunkCount = (int)Math.Ceiling(length / 8.0d);
                        sb.Append("    " + index + ": ("+length+") [");

                        index++;
                        for (var ch = 0; ch < chunkCount; ch++)
                        {
                            var raw = BitConverter.GetBytes(encodedTokens[index++]);
                            sb.Append(MakeSafe(Encoding.ASCII.GetString(raw)));
                        }
                        sb.AppendLine("]");
                    }
                    sb.AppendLine("End of data table");
                }
            }

            // output remaining bytecodes
            for (; index < encodedTokens.Count; index++)
            {
                var token = encodedTokens[index];
                var type = NanTags.TypeOf(token);
                sb.Append(index.ToString());
                sb.Append(" ");
                sb.Append(type.ToString());
                sb.Append(": ");
                sb.AppendLine(Stringify(token, type, debugSymbols));
            }

            return sb.ToString();
        }

        /// <summary>
        /// Reduce a .Net string to ASCII
        /// </summary>
        private string MakeSafe(string raw)
        {
            return string.Join("", raw.ToCharArray().Select(c => 
                (c >= ' ' && c <= '~') ? c : '\u001F'
                ));
        }

        public override string ToString()
        {
            return ToString(null);
        }

        private string Stringify(double token, DataType type, HashLookup<string> debugSymbols)
        {
            switch (type){
                case DataType.Invalid: return "";
                case DataType.NoValue: return "";

                case DataType.VariableRef:
                    var rref = NanTags.DecodeVariableRef(token);
                    if (debugSymbols?.ContainsKey(rref) == true)
                    {
                        return "'" + debugSymbols[rref] + "' (" + rref.ToString("X") + ")";
                    }
                    return rref.ToString("X");

                case DataType.Opcode:
                    NanTags.DecodeLongOpCode(token, out var ccls, out var cact, out var refr);
                    if (ccls == 'm') {
                        if (debugSymbols?.ContainsKey(refr) == true)
                        {
                            return ccls+""+cact+" '" + debugSymbols[refr] + "' (" + refr.ToString("X") + ")";
                        }
                    }
                    if (ccls == 'i')
                    {
                        if (debugSymbols?.ContainsKey(refr) == true)
                        {
                            return "Incr " + ((sbyte)cact) + " '" + debugSymbols[refr] + "' (" + refr.ToString("X") + ")";
                        }
                    }
                    NanTags.DecodeOpCode(token, out _, out _, out var p1, out var p2);
                    return ccls+""+cact+" ("+p1+", "+p2+")";

                case DataType.ValSmallString:
                    return "["+NanTags.DecodeShortStr(token)+"]";

                case DataType.PtrHashtable:
                case DataType.PtrGrid:
                case DataType.PtrSet:
                case DataType.PtrVector:
                case DataType.PtrString:
                case DataType.PtrStaticString:
                    NanTags.DecodePointer(token, out var targ, out _);
                    return " -> " + targ;

                case DataType.ValInt32: return NanTags.DecodeInt32(token).ToString();
                case DataType.ValUInt32: return NanTags.DecodeUInt32(token).ToString();
                case DataType.Number:  return token.ToString(CultureInfo.InvariantCulture);

                default:
                    throw new ArgumentOutOfRangeException(nameof(type), type, null);
            }
        }

        public List<double> Tokens()
        {
            return encodedTokens;
        }

        public string DereferenceString(long position) {
            // a string is [NanTag(UInt32): byte length] [string bytes, padded to 8 byte chunks]
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
        
        /// <summary>
        /// Interpret or cast value as a boolean
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool CastBoolean(double encoded)
        {
            var type = NanTags.TypeOf(encoded);

            switch (type) {
                case DataType.Number:
                    return Math.Abs(encoded) > double.Epsilon;
               
                case DataType.ValInt32:
                    return NanTags.DecodeInt32(encoded) != 0;

                case DataType.ValUInt32:
                    return NanTags.DecodeUInt32(encoded) != 0;

                case DataType.ValSmallString:
                    {
                        var strVal = NanTags.DecodeShortStr(encoded);
                        return StringTruthyness(strVal);
                    }

                case DataType.PtrString:
                case DataType.PtrStaticString:
                    {
                        NanTags.DecodePointer(encoded, out var ptr, out _);
                        var strVal = DereferenceString(ptr);
                        return StringTruthyness(strVal);
                    }

                case DataType.VariableRef:
                    // Follow scope
                    var next = Variables.Resolve(NanTags.DecodeVariableRef(encoded));
                    return CastBoolean(next);


                // All the things that can't be meaningfully cast are 'false'
                default: return false;
            }
        }

        /// <summary>
        /// null, empty, "false" or "0" are false. All other strings are true.
        /// </summary>
        private static bool StringTruthyness(string strVal)
        {
            if (string.IsNullOrEmpty(strVal)) return false;
            if (string.Equals(strVal, "false", StringComparison.OrdinalIgnoreCase)) return false;
            if (string.Equals(strVal, "0", StringComparison.OrdinalIgnoreCase)) return false;
            return true;
        }

        /// <summary>
        /// Interpret, or cast value as double
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public double CastDouble(double encoded)
        {
            var type = NanTags.TypeOf(encoded);
            double result;
            switch (type) {
                case DataType.Number:
                    return encoded;
               
                case DataType.ValInt32:
                    return NanTags.DecodeInt32(encoded);

                case DataType.ValUInt32:
                    return NanTags.DecodeUInt32(encoded);
                    
                case DataType.VariableRef:
                    // Follow scope
                    var next = Variables.Resolve(NanTags.DecodeVariableRef(encoded));
                    return CastDouble(next);
                    
                case DataType.ValSmallString:
                    double.TryParse(NanTags.DecodeShortStr(encoded), out result);
                    return result;

                case DataType.PtrStaticString:
                case DataType.PtrString:
                    NanTags.DecodePointer(encoded, out var target, out _);
                    double.TryParse(DereferenceString(target), out result);
                    return result;

                // All the things that can't be meaningfully cast
                default: return 0.0d;
            }
        }

        /// <summary>
        /// Get a resonable string representation from a value.
        /// This should include stringifying non-string types (numbers, structures etc)
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public string CastString(double encoded)
        {
            var type = NanTags.TypeOf(encoded);
            switch (type){
                case DataType.Invalid: return "<invalid value>";
                case DataType.Number: return encoded.ToString(CultureInfo.InvariantCulture);
                case DataType.Opcode: return "<Op Code>";
                case DataType.NoValue: return "";

                case DataType.VariableRef:
                    // Follow scope
                    var next = Variables.Resolve(NanTags.DecodeVariableRef(encoded));
                    return CastString(next);

                case DataType.ValSmallString:
                    return NanTags.DecodeShortStr(encoded);

                case DataType.PtrStaticString:
                case DataType.PtrString:
                    NanTags.DecodePointer(encoded, out var target, out _);
                    return DereferenceString(target);

                case DataType.PtrHashtable:
                case DataType.PtrGrid:
                case DataType.PtrSet:
                case DataType.PtrVector:
                    return "<complex type>";

                case DataType.ValInt32: return NanTags.DecodeInt32(encoded).ToString();
                case DataType.ValUInt32: return NanTags.DecodeUInt32(encoded).ToString();

                default:
                    throw new ArgumentOutOfRangeException();
            }
        }

        /// <summary>
        /// Cast a value to int. If not applicable, returns zero
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public int CastInt(double encoded)
        {
            int result;
            var type = NanTags.TypeOf(encoded);
            switch (type){
                case DataType.Invalid:
                case DataType.Opcode:
                case DataType.NoValue:
                    return 0;

                case DataType.VariableRef:
                    // Follow scope
                    var next = Variables.Resolve(NanTags.DecodeVariableRef(encoded));
                    return CastInt(next);

                case DataType.ValSmallString:
                    int.TryParse(NanTags.DecodeShortStr(encoded), out result);
                    return result;

                case DataType.PtrStaticString:
                case DataType.PtrString:
                    NanTags.DecodePointer(encoded, out var target, out _);
                    int.TryParse(DereferenceString(target), out result);
                    return result;

                case DataType.PtrHashtable:
                case DataType.PtrGrid:
                case DataType.PtrSet:
                case DataType.PtrVector:
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
            // short strings are stack/scope values
            if (str.Length <= 6) {
                return NanTags.EncodeShortStr(str);
            }

            // Longer strings need to be allocated
            var location = encodedTokens.Count;

            var bytes = Encoding.ASCII.GetBytes(str);
            var headerOpCode = NanTags.EncodeUInt32((uint)bytes.Length);

            encodedTokens.Add(headerOpCode);

            unchecked
            {
                ulong pack = 0;
                int rem = 0;
                for (int i = 0; i < bytes.Length; i++)
                {
                    pack += ((ulong)bytes[i]) << rem;

                    rem += 8;
                    if (rem > 56)
                    {
                        encodedTokens.Add(BitConverter.Int64BitsToDouble((long)pack));
                        rem = 0;
                        pack = 0;
                    }
                }

                if (rem != 0) {
                    for (; rem < 64; rem += 8)
                    {
                        pack += ((ulong)'_') << rem;
                    }
                    encodedTokens.Add(BitConverter.Int64BitsToDouble((long)pack));
                }
            }
            var token = NanTags.EncodePointer(location, DataType.PtrString);

            Variables.PotentialGarbage.Add(token);

            return token;
        }

        public string DiagnosticString(double token, HashLookup<string> symbols = null)
        {
            return Stringify(token, NanTags.TypeOf(token), symbols);
        }
    }
}