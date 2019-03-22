#pragma once

#ifndef tagcodetypes_h
#define tagcodetypes_h


// Comparison Operations available for compound compare
enum class CmpOp {
    Equal = '=',
    NotEqual = '!',
    Less = '<',
    Greater = '>'
};

// Enum mapping for all in-built functions from the runtime
// These are used primarily in TagCodeInterpreter.cpp -> AddBuiltInFunctionSymbols()
enum class FuncDef {
    // A bad function mapping
    Invalid = -200,

    // A custom function (defined by the script). Param count and start position are needed.
    Custom = 0,

    // The rest are the built-in functions

    Equal,
    GreaterThan,
    LessThan,
    NotEqual,
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

    Assert,
    Random,
    Eval,
    Call,

    MathAdd,
    MathSub,
    MathProd,
    MathDiv,
    MathMod,

    NewList,
    Push,
    Pop,
    Dequeue,

    UnitEmpty
};

typedef struct FunctionDefinition
{
    FuncDef Kind;
    int ParamCount;
    int StartPosition;
} FunctionDefinition;

FuncDef CmpOpToFunction(CmpOp cmpOp);

#endif
