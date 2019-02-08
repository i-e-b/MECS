#include "TagCodeTypes.h"

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
