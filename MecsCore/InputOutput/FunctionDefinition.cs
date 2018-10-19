namespace MecsCore.InputOutput
{
    public struct FunctionDefinition
    {
        public FuncDef Kind;
        public int ParamCount;
        public int StartPosition;
    }

    public enum FuncDef
    {
        /// <summary>
        /// A custom function (defined by the script)
        /// Param count and start position are used
        /// </summary>
        Custom = 0,

        // The rest are the built-in functions

        Equal,
        GreaterThan,
        LessThan,
        NotEqual,
        Assert,
        Random,
        Eval,
        Call,
        LogicNot,
        LogicOr,
        LogicAnd,
        ReadKey,
        ReadLine,
        Print,
        Substring,
        Length,
        Replace,
        Concat,
        MathAdd,
        MathSub,
        MathProd,
        MathDiv,
        MathMod,
        UnitEmpty
    }
}