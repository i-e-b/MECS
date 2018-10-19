using System;

namespace MecsCore.InputOutput
{
    /// <summary>
    /// Comparison Operations available for compound compare
    /// </summary>
    public enum CmpOp : byte
    {
        Equal = (byte)'=',
        NotEqual = (byte)'!',
        Less = (byte)'<',
        Greater = (byte)'>'
    }

    public static class CmpOpExtensions {
        public static FuncDef ToFunction(this CmpOp cmpOp) {
            switch (cmpOp){
                case CmpOp.Equal:
                    return FuncDef.Equal;
                case CmpOp.NotEqual:
                    return FuncDef.NotEqual;
                case CmpOp.Less:
                    return FuncDef.LessThan;
                case CmpOp.Greater:
                    return FuncDef.GreaterThan;
                default:
                    throw new ArgumentOutOfRangeException(nameof(cmpOp), cmpOp, null);
            }
        }
    }
}