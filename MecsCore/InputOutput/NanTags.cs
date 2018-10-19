﻿using System;
using System.Runtime.CompilerServices;
using System.Text;

namespace MecsCore.InputOutput
{
    /// <summary>
    /// Functions to encode and decode values behind NaN tags in 64-bit blocks
    /// </summary>
    public static class NanTags
    {
        const ulong NAN_FLAG   = 0x7FF8000000000000;      // Bits to make a quiet NaN
        const ulong UPPER_FOUR = 0x8007000000000000;      // mask for top 4 available bits
        const ulong LOWER_32   = 0x00000000FFFFFFFF;      // low 32 bits
        const ulong LOWER_48   = 0x0000FFFFFFFFFFFF;      // 48 bits for pointers, all non TAG data
        const ulong ALLOC_FLAG = 0x8000000000000000;      // Tags are arranged so GC types have this bit set

        // Mask with "UPPER_FOUR" then check against these:
        const ulong TAG_NAR            = 0;

        const ulong TAG_VAR_REF        = (ulong)DataType.VariableRef << 32;
        const ulong TAG_OPCODE         = (ulong)DataType.Opcode << 32;

        const ulong TAG_INT32_VAL      = (ulong)DataType.ValInt32 << 32;
        const ulong TAG_UINT32_VAL     = (ulong)DataType.ValUInt32 << 32;
        const ulong TAG_SMALL_STR      = (ulong)DataType.ValSmallString << 32;
        const ulong TAG_PTR_DEBUG      = (ulong)DataType.PtrStaticString << 32; // it is a pointer, but it's not allocated

        const ulong TAG_UNUSED_VAL_1   = (ulong)DataType.UNUSED_VAL_1 << 32;


        const ulong TAG_PTR_SET        = (ulong)DataType.PtrSet << 32;
        const ulong TAG_PTR_VECTOR     = (ulong)DataType.PtrVector << 32;
        const ulong TAG_PTR_STR        = (ulong)DataType.PtrString << 32;
        const ulong TAG_PTR_TABLE      = (ulong)DataType.PtrHashtable << 32;
        const ulong TAG_PTR_GRID       = (ulong)DataType.PtrGrid << 32;

        const ulong TAG_UNUSED_PTR_1   = (ulong)DataType.UNUSED_PTR_1 << 32;
        const ulong TAG_UNUSED_PTR_2   = (ulong)DataType.UNUSED_PTR_2 << 32;
        const ulong TAG_UNUSED_PTR_3   = (ulong)DataType.UNUSED_PTR_3 << 32;



        /// <summary>
        /// Read tagged type
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static unsafe DataType TypeOf(double unknown)
        {
            var bits = *(ulong*)&unknown;
            if ((bits & NAN_FLAG) != NAN_FLAG) return DataType.Number;
            var tag = (bits & UPPER_FOUR) >> 32;
            return (DataType)tag;
        }


        /// <summary>
        /// Returns true if this token is a pointer to allocated data.
        /// Returns false if this token is a direct value
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static unsafe bool IsAllocated(double token)
        {
            var bits = *(ulong*)&token;
            return (ALLOC_FLAG & bits) > 0;
        }

        /// <summary>
        /// Return the tag bits for a DataType
        /// </summary>
        public static ulong TagFor(DataType type)
        {
            switch (type)
            {
                case DataType.NoValue: return TAG_NAR;
                case DataType.VariableRef: return TAG_VAR_REF;
                case DataType.Opcode: return TAG_OPCODE;

                case DataType.ValInt32: return TAG_INT32_VAL;
                case DataType.ValUInt32: return TAG_UINT32_VAL;
                case DataType.ValSmallString: return TAG_SMALL_STR;

                case DataType.PtrStaticString: return TAG_PTR_DEBUG;

                case DataType.PtrString: return TAG_PTR_STR;
                case DataType.PtrHashtable: return TAG_PTR_TABLE;
                case DataType.PtrGrid: return TAG_PTR_GRID;
                case DataType.PtrSet: return TAG_PTR_SET;
                case DataType.PtrVector: return TAG_PTR_VECTOR;


                case DataType.UNUSED_PTR_1: return TAG_UNUSED_PTR_1;
                case DataType.UNUSED_PTR_2: return TAG_UNUSED_PTR_2;
                case DataType.UNUSED_PTR_3: return TAG_UNUSED_PTR_3;
                case DataType.UNUSED_VAL_1: return TAG_UNUSED_VAL_1;

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
        /// Encode an op-code with up to 2x16 bit params
        /// </summary>
        /// <param name="codeClass">Kind of op code</param>
        /// <param name="codeAction">The action to perform in the class</param>
        /// <param name="p1">First parameter, if used</param>
        /// <param name="p2">Second parameter if used</param>
        public static unsafe double EncodeOpcode(char codeClass, byte codeAction, ushort p1, ushort p2)
        {
            unchecked
            {
                byte cc = (byte)codeClass;
                byte ca = codeAction;
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
        /// Decode a short string
        /// </summary>
        public static unsafe string DecodeShortStr(double token)
        {
            var sb = new StringBuilder(6);

            var bits = *(ulong*)&token;

            for (int i = 5; i >= 0; i--)
            {
                var cv = (bits >> (i * 8)) & 0xFF;
                if (cv == 0) break;
                sb.Append((char)cv);
            }

            return sb.ToString();
        }

        /// <summary>
        /// Encode a string of 6 characters or less
        /// </summary>
        public static unsafe double EncodeShortStr(string str)
        {
            unchecked{
                var len = str.Length;
                if (len > 6) throw new Exception();

                var tag = NAN_FLAG | TAG_SMALL_STR;

                for (int i = 0; i < len; i++)
                {
                    var bv = (byte)str[i];
                    var ulv = (ulong)bv;
                    tag |= ulv << ((5 - i) * 8);
                }
                return *(double*)&tag;
            }
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
                case DataType.PtrStaticString: return "Pointer: Static Str ["+DecodePointer(token)+"]"; 
                case DataType.PtrHashtable: return "Pointer: Hashtable ["+DecodePointer(token)+"]";
                case DataType.PtrGrid: return "Pointer: Grid ["+DecodePointer(token)+"]";
                case DataType.PtrVector: return "Pointer: Linked List ["+DecodePointer(token)+"]";

                case DataType.UNUSED_PTR_2: return "Pointer: String Array ["+DecodePointer(token)+"]";
                case DataType.UNUSED_PTR_3: return "Pointer: Double Array ["+DecodePointer(token)+"]";

                case DataType.PtrSet: return "Pointer: String Set ["+DecodePointer(token)+"]";
                case DataType.UNUSED_PTR_1: return "Pointer: Int32 Set ["+DecodePointer(token)+"]";

                case DataType.Number: return "Double ["+token+"]";
                case DataType.ValInt32: return "Int32 ["+DecodeInt32(token)+"]";
                case DataType.ValUInt32: return "UInt32 [" + DecodeUInt32(token) + "]";
                case DataType.ValSmallString: return "Short String [" + DecodeShortStr(token) + "]";

                case DataType.UNUSED_VAL_1: return "UNUSED TOKEN";

                default:
                    return "Invalid token";
            }
        }
    }

}