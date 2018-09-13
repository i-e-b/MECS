using System;

namespace EvieCompilerSystem.InputOutput
{
    public enum DataType
    {
        Invalid,
        Number,
        VariableRef, Opcode,
        PtrString, PtrHashtable, PtrGrid,
        PtrArray_Int32, PtrArray_UInt32, PtrArray_String, PtrArray_Double,
        PtrSet_String, PtrSet_Int32,
        PtrLinkedList,
        PtrDiagnosticString,
        NoValue,
        ValInt32, ValUInt32
    }

    public enum NonValueType
    {
        /// <summary>
        /// Nothing returned
        /// </summary>
        Void = 0,

        /// <summary>
        /// No result, but as part of a return
        /// </summary>
        Unit = 1
    }

    public static class NanTags
    {
        const ulong NAN_FLAG = 0x7FF8000000000000;      // Bits to make a quiet NaN
        const ulong ALL_DATA = 0x8007FFFFFFFFFFFF;      // 51 bits (all non NaN flags)
        const ulong UPPER_FOUR = 0x8007000000000000;    // mask for top 4 available bits
        const ulong LOWER_32 = 0x00000000FFFFFFFF;      // low 32 bits
        const ulong LOWER_48 = 0x0000FFFFFFFFFFFF;      // 48 bits for pointers, all non TAG data

        // Mask with "UPPER_FOUR" then check against these:    // Possible assignments:
        const ulong TAG_VAR_REF        = 0x8000000000000000;    // Variable ref (2 char + 32 bit hash?)
        const ulong TAG_OPCODE         = 0x8001000000000000;    // Opcode (3 x 16bit: code, first param, second param)

        const ulong TAG_PTR_STR        = 0x8002000000000000;    // Memory pointer to STRING header
        const ulong TAG_PTR_TABLE      = 0x8003000000000000;    // Memory pointer to HASHTABLE
        const ulong TAG_PTR_GRID       = 0x8004000000000000;    // Memory pointer to GRID (hash table keyed by ints) -- maybe this can be inferred from usage?

        const ulong TAG_PTR_ARR_INT32  = 0x8005000000000000;    // Memory pointer to ARRAY of int32
        const ulong TAG_PTR_ARR_UINT32 = 0x8006000000000000;    // Memory pointer to ARRAY of uint32
        const ulong TAG_ARR_STR        = 0x8007000000000000;    // Memory pointer to ARRAY of string
        const ulong TAG_ARR_DOUBLE     = 0x0000000000000000;    // Memory pointer to ARRAY of double

        const ulong TAG_PTR_SET_STR    = 0x0001000000000000;    // Memory pointer to SET of string header
        const ulong TAG_PTR_SET_INT32  = 0x0002000000000000;    // Memory pointer to SET of 32 signed integer

        const ulong TAG_PTR_LINKLIST   = 0x0003000000000000;    // Memory pointer to double-linked list node

        const ulong TAG_PTR_DEBUG      = 0x0004000000000000;    // Memory pointer to Diagnostic string
        const ulong TAG_NAR            = 0x0005000000000000;    // NoValue - specifically not-a-value / not-a-result. Holds general runtime flags.

        const ulong TAG_INT32_VAL      = 0x0006000000000000;    // Signed 32 bit integer / single boolean
        const ulong TAG_UINT32_VAL     = 0x0007000000000000;    // Unsigned integer 32

        /// <summary>
        /// Read tagged type
        /// </summary>
        public static DataType TypeOf(double unknown)
        {
            if (!double.IsNaN(unknown)) return DataType.Number;

            var tag = (ulong)BitConverter.DoubleToInt64Bits(unknown) & UPPER_FOUR;

            switch (tag)
            {
                case TAG_VAR_REF: return DataType.VariableRef;
                case TAG_OPCODE: return DataType.Opcode;
                case TAG_PTR_STR: return DataType.PtrString;
                case TAG_PTR_TABLE: return DataType.PtrHashtable;
                case TAG_PTR_GRID: return DataType.PtrGrid;
                case TAG_PTR_ARR_INT32: return DataType.PtrArray_Int32;
                case TAG_PTR_ARR_UINT32: return DataType.PtrArray_UInt32;
                case TAG_ARR_STR: return DataType.PtrArray_String;
                case TAG_ARR_DOUBLE: return DataType.PtrArray_Double;
                case TAG_PTR_SET_STR: return DataType.PtrSet_String;
                case TAG_PTR_SET_INT32: return DataType.PtrSet_Int32;
                case TAG_PTR_LINKLIST: return DataType.PtrLinkedList;
                case TAG_PTR_DEBUG: return DataType.PtrDiagnosticString;
                case TAG_NAR: return DataType.NoValue;
                case TAG_INT32_VAL: return DataType.ValInt32;
                case TAG_UINT32_VAL: return DataType.ValUInt32;

                default:
                    return DataType.Invalid;
            }
        }

        /// <summary>
        /// Return the tag bits for a DataType
        /// </summary>
        public static ulong TagFor(DataType type)
        {
            switch (type)
            {
                case DataType.VariableRef: return TAG_VAR_REF;
                case DataType.Opcode: return TAG_OPCODE;
                case DataType.PtrString: return TAG_PTR_STR;
                case DataType.PtrHashtable: return TAG_PTR_TABLE;
                case DataType.PtrGrid: return TAG_PTR_GRID;
                case DataType.PtrArray_Int32: return TAG_PTR_ARR_INT32;
                case DataType.PtrArray_UInt32: return TAG_PTR_ARR_UINT32;
                case DataType.PtrArray_String: return TAG_ARR_STR;
                case DataType.PtrArray_Double: return TAG_ARR_DOUBLE;
                case DataType.PtrSet_String: return TAG_PTR_SET_STR;
                case DataType.PtrSet_Int32: return TAG_PTR_SET_INT32;
                case DataType.PtrLinkedList: return TAG_PTR_LINKLIST;
                case DataType.PtrDiagnosticString: return TAG_PTR_DEBUG;
                case DataType.NoValue: return TAG_NAR;
                case DataType.ValInt32: return TAG_INT32_VAL;
                case DataType.ValUInt32: return TAG_UINT32_VAL;

                default:
                    throw new Exception("Invalid data type");
            }
        }

        /// <summary>
        /// Value tagged as an non return type
        /// </summary>
        public static double VoidReturn()
        {
            unchecked
            {
                ulong encoded = NAN_FLAG | TAG_NAR | (ulong)NonValueType.Void;
                return BitConverter.Int64BitsToDouble((long)encoded);
            }
        }

        /// <summary>
        /// Encode a tagged non-value tag
        /// </summary>
        public static double EncodeNonValue(NonValueType type)
        {
            unchecked
            {
                ulong encoded = NAN_FLAG | TAG_NAR | ((ulong)type);
                return BitConverter.Int64BitsToDouble((long)encoded);
            }
        }

        /// <summary>
        /// Read the tag from a non value tag
        /// </summary>
        public static NonValueType DecodeNonValue(double encoded) {
            
            unchecked
            {
                int tag = (int)((ulong)BitConverter.DoubleToInt64Bits(encoded) & LOWER_32);
                return (NonValueType)tag;
            }
        }

        /// <summary>
        /// Encode an op-code with up to 2x16 bit params
        /// </summary>
        /// <param name="codeClass">Kind of op code</param>
        /// <param name="codeAction">The action to perform in the class</param>
        /// <param name="p1">First parameter, if used</param>
        /// <param name="p2">Second parameter if used</param>
        public static double EncodeOpcode(char codeClass, char codeAction, ushort p1, ushort p2)
        {
            unchecked
            {
                byte cc = (byte)codeClass;
                byte ca = (byte)codeAction;
                ulong encoded =
                        NAN_FLAG
                      | TAG_OPCODE
                      | ((ulong)cc << 40)
                      | ((ulong)ca << 32)
                      | ((ulong)p1 << 16)
                      | ((ulong)p2)
                    ;
                return BitConverter.Int64BitsToDouble((long)encoded);
            }
        }

        
        /// <summary>
        /// Encode an op-code with one 32 bit param
        /// </summary>
        /// <param name="codeClass">Kind of op code</param>
        /// <param name="codeAction">The action to perform in the class</param>
        /// <param name="p1">First parameter, if used</param>
        public static double EncodeLongOpcode(char codeClass, char codeAction, int p1)
        {
            unchecked
            {
                byte cc = (byte)codeClass;
                byte ca = (byte)codeAction;
                ulong encoded =
                        NAN_FLAG
                        | TAG_OPCODE
                        | ((ulong)cc << 40)
                        | ((ulong)ca << 32)
                        | ((ulong)p1)
                    ;
                return BitConverter.Int64BitsToDouble((long)encoded);
            }
        }

        /// <summary>
        /// Read an opcode out of a tagged nan
        /// </summary>
        /// <param name="encoded">The tagged value</param>
        /// <param name="codeClass">class of op code</param>
        /// <param name="codeAction">action of op code</param>
        /// <param name="p1">first param</param>
        /// <param name="p2">second param</param>
        public static void DecodeOpCode(double encoded, out char codeClass, out char codeAction, out ushort p1, out ushort p2)
        {
            unchecked
            {
                var enc = LOWER_48 & (ulong)BitConverter.DoubleToInt64Bits(encoded);
                codeClass = (char)(enc >> 40);
                codeAction = (char)(0xFF & (enc >> 32));
                p1 = (ushort)(enc >> 16);
                p2 = (ushort)enc;
            }
        }

        /// <summary>
        /// Read an opcode out of a tagged nan, combining the two parameters into a single longer one
        /// </summary>
        /// <param name="encoded">The tagged value</param>
        /// <param name="codeClass">class of op code</param>
        /// <param name="codeAction">action of op code</param>
        /// <param name="p1">first param and second param combined</param>
        public static void DecodeLongOpCode(double encoded, out char codeClass, out char codeAction, out uint p1)
        {
            unchecked
            {
                var enc = LOWER_48 & (ulong)BitConverter.DoubleToInt64Bits(encoded);
                codeClass = (char)(enc >> 40);
                codeAction = (char)(0xFF & (enc >> 32));
                p1 = (uint)enc;
            }
        }

        /// <summary>
        /// Crush and encode a name (such as a function or variable name) as a variable ref
        /// </summary>
        /// <param name="fullName">Full name of the identifier</param>
        /// <param name="crushedName">Output crushed name</param>
        /// <returns>Encoded data</returns>
        public static double EncodeVariableRef(string fullName, out ulong crushedName)
        {
            unchecked
            {
                ulong hash = prospector32s(fullName.ToCharArray(), (uint)fullName.Length);
                byte f = (byte)fullName[fullName.Length - 1];
                byte l = (byte)fullName.Length;

                crushedName = ((ulong)f << 40) | ((ulong)l << 32) | hash;
                ulong raw = NAN_FLAG | TAG_VAR_REF | crushedName;

                return BitConverter.Int64BitsToDouble((long)raw);
            }
        }
        
        /// <summary>
        /// Encode an already crushed name as a variable ref
        /// </summary>
        public static double EncodeVariableRef(ulong crushedName)
        {
            unchecked
            {
                ulong raw = NAN_FLAG | TAG_VAR_REF | crushedName;

                return BitConverter.Int64BitsToDouble((long)raw);
            }
        }

        /// <summary>
        /// Get hash code of names, as created by variable reference op codes
        /// </summary>
        public static ulong GetCrushedName(string fullName) {
            unchecked
            {
                ulong hash = prospector32s(fullName.ToCharArray(), (uint)fullName.Length);
                byte f = (byte)fullName[fullName.Length - 1];
                byte l = (byte)fullName.Length;

                return ((ulong)f << 40) | ((ulong)l << 32) | hash;
            }
        }

        /// <summary>
        /// Extract an encoded reference name from a double
        /// </summary>
        public static ulong DecodeVariableRef(double enc)
        {
            unchecked
            {
                return LOWER_48 & (ulong)BitConverter.DoubleToInt64Bits(enc);
            }
        }

        /// <summary>
        /// Encode a pointer with a type
        /// </summary>
        public static double EncodePointer(long target, DataType type)
        {
            unchecked
            {
                return BitConverter.Int64BitsToDouble((long)(NAN_FLAG | TagFor(type) | ((ulong)target & LOWER_48)));
            }
        }

        /// <summary>
        /// Decode a pointer and type
        /// </summary>
        public static void DecodePointer(double encoded, out long target, out DataType type)
        {
            unchecked
            {
                type = TypeOf(encoded);
                target = (long)((ulong)BitConverter.DoubleToInt64Bits(encoded) & LOWER_48);
            }
        }

        /// <summary>
        /// Encode an int32
        /// </summary>
        public static double EncodeInt32(int original)
        {
            unchecked
            {
                return BitConverter.Int64BitsToDouble((long)(NAN_FLAG | TAG_INT32_VAL | ((ulong)original & LOWER_32)));
            }
        }

        /// <summary>
        /// Decode an int32
        /// </summary>
        public static int DecodeInt32(double enc)
        {
            unchecked
            {
                return (int)((ulong)BitConverter.DoubleToInt64Bits(enc) & LOWER_32);
            }
        }

        /// <summary>
        /// Encode an unsigned int32
        /// </summary>
        public static double EncodeUInt32(uint original)
        {
            unchecked
            {
                return BitConverter.Int64BitsToDouble((long)(NAN_FLAG | TAG_UINT32_VAL | ((ulong)original & LOWER_32)));
            }
        }

        /// <summary>
        /// Decode an unsigned int32
        /// </summary>
        public static uint DecodeUInt32(double enc)
        {
            unchecked
            {
                return (uint)((ulong)BitConverter.DoubleToInt64Bits(enc) & LOWER_32);
            }
        }

        /// <summary>
        /// Test if all data except Exponent and first bit of mantissa are equal
        /// </summary>
        public static bool AreEqual(double a, double b)
        {
            var da = ALL_DATA & (ulong)BitConverter.DoubleToInt64Bits(a);
            var db = ALL_DATA & (ulong)BitConverter.DoubleToInt64Bits(b);
            return da == db;
        }

        /// <summary>
        /// Low bias 32 bit hash
        /// </summary>
        static uint prospector32s(char[] buf, uint key)
        {
            unchecked
            {
                uint hash = key;
                for (int i = 0; i < buf.Length; i++)
                {
                    hash += buf[i];
                    hash ^= hash >> 16;
                    hash *= 0x7feb352d;
                    hash ^= hash >> 15;
                    hash *= 0x846ca68b;
                    hash ^= hash >> 16;
                }
                hash ^= (uint)buf.Length;
                hash ^= hash >> 16;
                hash *= 0x7feb352d;
                hash ^= hash >> 15;
                hash *= 0x846ca68b;
                hash ^= hash >> 16;
                return hash + key;
            }
        }

        /// <summary>
        /// Encode a boolean. We use Int32, 0 = false
        /// </summary>
        public static double EncodeBool(bool b)
        {
            return EncodeInt32(b ? -1 : 0);
        }

    }

}