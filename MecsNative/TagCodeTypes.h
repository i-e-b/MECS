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
};

typedef struct FunctionDefinition
{
    FuncDef Kind;
    int ParamCount;
    int StartPosition;
} FunctionDefinition;

FuncDef CmpOpToFunction(CmpOp cmpOp) {
    switch (cmpOp) {
    case CmpOp::Equal:
        return FuncDef::Equal;
    case CmpOp::NotEqual:
        return FuncDef::NotEqual;
    case CmpOp::Less:
        return FuncDef::LessThan;
    case CmpOp::Greater:
        return FuncDef::GreaterThan;
    default:
        return FuncDef::Invalid;
    }
}

#endif
