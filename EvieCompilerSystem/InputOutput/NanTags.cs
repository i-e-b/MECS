using System;
using System.Runtime.CompilerServices;

namespace EvieCompilerSystem.InputOutput
{
    public enum NonValueType
    {
        /// <summary>Nothing returned</summary>
        Void = 0,

        /// <summary>No result, but as part of a return</summary>
        Unit = 1
    }

    public enum DataType : uint
    {
        Invalid               = 1,
        Number                = 2,

        NoValue               = 0x00000000,
        VariableRef           = 0x80000000,
        Opcode                = 0x80010000,
        PtrString             = 0x80020000,
        PtrHashtable          = 0x80030000,
        PtrGrid               = 0x80040000,
        PtrArray_Int32        = 0x80050000,
        PtrArray_UInt32       = 0x80060000,
        PtrArray_String       = 0x80070000,
        PtrArray_Double       = 0x00050000,
        PtrSet_String         = 0x00010000,
        PtrSet_Int32          = 0x00020000,
        PtrLinkedList         = 0x00030000,
        PtrDiagnosticString   = 0x00040000,
        ValInt32              = 0x00060000,
        ValUInt32             = 0x00070000
    }

    public static class NanTags
    {
        const ulong NAN_FLAG = 0x7FF8000000000000;      // Bits to make a quiet NaN
        //const ulong ALL_DATA = 0x8007FFFFFFFFFFFF;      // 51 bits (all non NaN flags)
        const ulong UPPER_FOUR = 0x8007000000000000;    // mask for top 4 available bits
        const ulong LOWER_32 = 0x00000000FFFFFFFF;      // low 32 bits
        const ulong LOWER_48 = 0x0000FFFFFFFFFFFF;      // 48 bits for pointers, all non TAG data

        // Mask with "UPPER_FOUR" then check against these:    // Possible assignments:
        const ulong TAG_NAR            = 0x0000000000000000;    // NoValue - specifically not-a-value / not-a-result. Holds general runtime flags.

        const ulong TAG_PTR_SET_STR    = 0x0001000000000000;    // Memory pointer to SET of string header
        const ulong TAG_PTR_SET_INT32  = 0x0002000000000000;    // Memory pointer to SET of 32 signed integer
        const ulong TAG_PTR_LINKLIST   = 0x0003000000000000;    // Memory pointer to double-linked list node
        const ulong TAG_PTR_DEBUG      = 0x0004000000000000;    // Memory pointer to Diagnostic string
        const ulong TAG_INT32_VAL      = 0x0006000000000000;    // Signed 32 bit integer / single boolean
        const ulong TAG_UINT32_VAL     = 0x0007000000000000;    // Unsigned integer 32

        const ulong TAG_VAR_REF        = 0x8000000000000000;    // Variable ref (32 bit hash)
        const ulong TAG_OPCODE         = 0x8001000000000000;    // Opcode (3 x 16bit: code, first param, second param)

        const ulong TAG_PTR_STR        = 0x8002000000000000;    // Memory pointer to STRING header
        const ulong TAG_PTR_TABLE      = 0x8003000000000000;    // Memory pointer to HASHTABLE
        const ulong TAG_PTR_GRID       = 0x8004000000000000;    // Memory pointer to GRID (hash table keyed by ints) -- maybe this can be inferred from usage?

        const ulong TAG_PTR_ARR_INT32  = 0x8005000000000000;    // Memory pointer to ARRAY of int32
        const ulong TAG_PTR_ARR_UINT32 = 0x8006000000000000;    // Memory pointer to ARRAY of uint32
        const ulong TAG_ARR_STR        = 0x8007000000000000;    // Memory pointer to ARRAY of string
        const ulong TAG_ARR_DOUBLE     = 0x0005000000000000;    // Memory pointer to ARRAY of double


        /// <summary>
        /// Read tagged type
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static unsafe DataType TypeOf(double unknown)
        {
            unchecked
            {
                var bits = *(ulong*) &unknown;
                if ((bits & NAN_FLAG) != NAN_FLAG) return DataType.Number;
                var tag = (bits & UPPER_FOUR) >> 32;
                return (DataType)tag;
            }
        }

        
        /// <summary>
        /// Returns true if this token is a pointer to allocated data.
        /// Returns false if this token is a direct value
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool IsAllocated(double token)
        {
            // Nice-to-have -- check a specific bit pattern rather than having another switch table
            var type = TypeOf(token);
            switch (type){
                case DataType.Invalid:
                case DataType.Number:
                case DataType.NoValue:
                case DataType.VariableRef:
                case DataType.Opcode:
                case DataType.ValInt32:
                case DataType.ValUInt32:
                    return false;

                case DataType.PtrString:
                case DataType.PtrHashtable:
                case DataType.PtrGrid:
                case DataType.PtrArray_Int32:
                case DataType.PtrArray_UInt32:
                case DataType.PtrArray_String:
                case DataType.PtrArray_Double:
                case DataType.PtrSet_String:
                case DataType.PtrSet_Int32:
                case DataType.PtrLinkedList:
                case DataType.PtrDiagnosticString:
                    return true;

                default:
                    throw new ArgumentOutOfRangeException();
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
        public static unsafe double VoidReturn()
        {
            unchecked
            {
                ulong encoded = NAN_FLAG | TAG_NAR | (ulong)NonValueType.Void;
                return *(double*)&encoded;//BitConverter.Int64BitsToDouble((long)encoded);
            }
        }

        /// <summary>
        /// Encode a tagged non-value tag
        /// </summary>
        public static unsafe double EncodeNonValue(NonValueType type)
        {
            unchecked
            {
                ulong encoded = NAN_FLAG | TAG_NAR | ((ulong)type);
                return *(double*)&encoded;
            }
        }

        /// <summary>
        /// Read the tag from a non value tag
        /// </summary>
        public static unsafe NonValueType DecodeNonValue(double encoded) {
            
            unchecked
            {
                int tag = (int)((*(ulong*)&encoded) & LOWER_32);
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
        public static unsafe double EncodeOpcode(char codeClass, char codeAction, ushort p1, ushort p2)
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
                      | p2
                    ;
                return *(double*)&encoded;
            }
        }

        /// <summary>
        /// Encode an op-code with one 32 bit param
        /// </summary>
        /// <param name="codeClass">Kind of op code</param>
        /// <param name="codeAction">The action to perform in the class</param>
        /// <param name="p1">First parameter, if used</param>
        public static double EncodeLongOpcode(char codeClass, char codeAction, uint p1)
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
                        | p1
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
        public static unsafe void DecodeOpCode(double encoded, out char codeClass, out char codeAction, out ushort p1, out ushort p2)
        {
            unchecked
            {
                var enc = LOWER_48 & (*(ulong*)&encoded);
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
        public static unsafe void DecodeLongOpCode(double encoded, out char codeClass, out char codeAction, out uint p1)
        {
            unchecked
            {
                var enc = LOWER_48 & (*(ulong*)&encoded);
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
        public static unsafe double EncodeVariableRef(string fullName, out uint crushedName)
        {
            unchecked
            {
                crushedName = prospector32s(fullName.ToCharArray(), (uint)fullName.Length);
                ulong raw = NAN_FLAG | TAG_VAR_REF | crushedName;
                
                return *(double*)&raw;
            }
        }
        
        /// <summary>
        /// Encode an already crushed name as a variable ref
        /// </summary>
        public static unsafe double EncodeVariableRef(uint crushedName)
        {
            uint nse = crushedName;
            ulong raw = NAN_FLAG | TAG_VAR_REF | nse;

            return *(double*)&raw;
        }

        /// <summary>
        /// Get hash code of names, as created by variable reference op codes
        /// </summary>
        public static uint GetCrushedName(string fullName) {
            unchecked
            {
                return prospector32s(fullName.ToCharArray(), (uint)fullName.Length);
            }
        }

        /// <summary>
        /// Extract an encoded reference name from a double
        /// </summary>
        public static unsafe uint DecodeVariableRef(double encoded)
        {
            unchecked{
                var raw = LOWER_32 & (*(ulong*)&encoded);
                return (uint)raw;
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
        public static unsafe void DecodePointer(double encoded, out long target, out DataType type)
        {
            unchecked
            {
                type = TypeOf(encoded);
                target = (long)((*(ulong*)&encoded) & LOWER_48);
            }
        }
        
        /// <summary>
        /// Decode a pointer
        /// </summary>
        public static unsafe long DecodePointer(double encoded)
        {
            unchecked
            {
                return (long)((*(ulong*)&encoded) & LOWER_48);
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
        public static unsafe int DecodeInt32(double encoded)
        {
            unchecked
            {
                return (int)((*(ulong*)&encoded) & LOWER_32);
            }
        }

        /// <summary>
        /// Encode an unsigned int32
        /// </summary>
        public static double EncodeUInt32(uint original)
        {
            unchecked
            {
                return BitConverter.Int64BitsToDouble((long)(NAN_FLAG | TAG_UINT32_VAL | (original & LOWER_32)));
            }
        }

        /// <summary>
        /// Decode an unsigned int32
        /// </summary>
        public static unsafe uint DecodeUInt32(double encoded)
        {
            unchecked
            {
                return (uint)((*(ulong*)&encoded) & LOWER_32);
            }
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

        /// <summary>
        /// Get raw data of tags
        /// </summary>
        public static unsafe ulong DecodeRaw(double d)
        {
            return *(ulong*)&d;
        }

        /// <summary>
        /// Diagnostic description of a token
        /// </summary>
        public static string Describe(double token)
        {
            var type = TypeOf(token);
            switch (type){
                case DataType.NoValue: return "Non value";
                case DataType.Opcode: return "Opcode";
                case DataType.VariableRef:  return "VariableNameRef ["+DecodeVariableRef(token)+"]";

                case DataType.PtrString: return "Pointer: String ["+DecodePointer(token)+"]"; 
                case DataType.PtrDiagnosticString: return "Pointer: Diag Str ["+DecodePointer(token)+"]"; 
                case DataType.PtrHashtable: return "Pointer: Hashtable ["+DecodePointer(token)+"]";
                case DataType.PtrGrid: return "Pointer: Grid ["+DecodePointer(token)+"]";
                case DataType.PtrLinkedList: return "Pointer: Linked List ["+DecodePointer(token)+"]";

                case DataType.PtrArray_Int32:  return "Pointer: Int32 Array ["+DecodePointer(token)+"]";
                case DataType.PtrArray_UInt32: return "Pointer: UInt32 Array ["+DecodePointer(token)+"]";
                case DataType.PtrArray_String: return "Pointer: String Array ["+DecodePointer(token)+"]";
                case DataType.PtrArray_Double: return "Pointer: Double Array ["+DecodePointer(token)+"]";

                case DataType.PtrSet_String: return "Pointer: String Set ["+DecodePointer(token)+"]";
                case DataType.PtrSet_Int32: return "Pointer: Int32 Set ["+DecodePointer(token)+"]";

                case DataType.Number: return "Double ["+token+"]";
                case DataType.ValInt32: return "Int32 ["+DecodeInt32(token)+"]";
                case DataType.ValUInt32: return "UInt32 [" + DecodeUInt32(token) + "]";

                default:
                    return "Invalid token";
            }
        }
    }

}