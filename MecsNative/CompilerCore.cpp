#include "CompilerCore.h"
#include "SourceCodeTokeniser.h"
#include "CompilerOptimisations.h"
#include "Desugar.h"

#include "Vector.h"
#include "FileSys.h"
#include "TimingSys.h"

// Up this if you have files over 1MB. Or write better code.
#define MAX_IMPORT_SIZE 0xFFFFF


typedef String* StringPtr;
bool CC_StringKeyCompare(void* key_A, void* key_B) {
    auto A = *((StringPtr*)key_A);
    auto B = *((StringPtr*)key_B);
    return StringAreEqual(A, B);
}
unsigned int CC_StringKeyHash(void* key) {
    auto A = *((StringPtr*)key);
    return StringHash(A);
}

RegisterHashMapStatics(Map)
RegisterHashMapFor(StringPtr, bool, CC_StringKeyHash, CC_StringKeyCompare, Map)

RegisterTreeFor(SourceNode, Tree)

RegisterVectorFor(char, Vec)
RegisterVectorFor(StringPtr, Vec)

// Compile source code from a syntax tree into a tag code cache
TagCodeCache* CompileRoot(TreeNode* root, bool debug) {
    if (root == NULL) return NULL;
    auto wr = TCW_Allocate();

    if (debug) {
        auto timeStamp = SystemTime();
        auto header = StringNew("/*\r\n MECS - TagCode Intermediate Language\r\n");
        StringAppend(header, "Date    : ");
        StringAppendInt64Hex(header, timeStamp);
        StringAppend(header, "Version : 1.0\r\n*/\r\n\r\n");
        TCW_Comment(wr, header);
    }

    auto parameterNames = ScopeAllocate(); // renaming for local parameters
    auto includedFiles = MapAllocate_StringPtr_bool(32); // used to prevent multiple includes

    // The implementation of `Compile` is way down at the bottom
    TCW_Merge(wr, Compile(root, 0, debug, parameterNames, includedFiles, Context::Default));

    return wr;
}


bool IsUnwrappedIdentifier(String* valueName, TreeNode* rootNode, Context compileContext) {
    auto root = TreeReadBody_SourceNode(rootNode);
    if (root->NodeType != NodeType::Atom || compileContext == Context::MemoryAccess) return false;

    if (   StringAreEqual(valueName, "false") // reserved identifiers
        || StringAreEqual(valueName, "true")) return false;

    return true;
}
bool IsMemoryFunction(TreeNode* node) {
    if (node == NULL) return false;
    auto source = TreeReadBody_SourceNode(node);
    auto str = source->Text;

    if (   StringAreEqual(str, "get")
        || StringAreEqual(str, "set")
        || StringAreEqual(str, "isset")
        || StringAreEqual(str, "unset")) return true;

    return false;
}
bool IsInclude(TreeNode* node) {
    if (node == NULL) return false;
    auto source = TreeReadBody_SourceNode(node);
    auto str = source->Text;

    if (StringAreEqual(str, "import")) return true;

    return false;
}
bool IsFlowControl(TreeNode* node) {
    if (node == NULL) return false;
    auto source = TreeReadBody_SourceNode(node);
    auto str = source->Text;

    if (   StringAreEqual(str, "if")
        || StringAreEqual(str, "while")) return true;

    return false;
}
bool IsFunctionDefinition(TreeNode* node) {
    if (node == NULL) return false;
    auto source = TreeReadBody_SourceNode(node);
    auto str = source->Text;

    if (StringAreEqual(str, "def")) return true;

    return false;
}

// Map parameter names to positional names. This must match the expectations of the interpreter
void ParameterPositions(Scope* parameterNames, TreeNode* paramDef, TagCodeCache* wr) {
    ScopePush(parameterNames, NULL);
    int i = 0;

    auto chain = TreeChild(paramDef);
    while (chain != NULL) {
        auto data = TreeReadBody_SourceNode(chain);
        if (InScope(parameterNames, GetCrushedName(data->Text))) {
            TCW_AddError(wr, StringNewFormat("Duplicate parameter '\x01'.\r\nAll parameter names must be unique in a single function definition", data->Text));
            return;
        }

        auto originalReference = GetCrushedName(data->Text);
        auto parameterReference = ScopeNameForPosition(i);
        auto parameterByteCode = EncodeVariableRef(parameterReference);

        ScopeSetValue(parameterNames, originalReference, parameterByteCode);

        TCW_AddSymbol(wr, originalReference, data->Text);
        TCW_AddSymbol(wr, parameterReference, StringNewFormat("param[\x02]", i));

        chain = TreeSibling(chain);
        i++;
    }
}

void EmitLeafNode(TreeNode* rootNode, bool debug, Scope* parameterNames, Context compileContext, TagCodeCache* wr) {
    auto root = TreeReadBody_SourceNode(rootNode);
    DataTag leafValue = {};
    bool substitute = false;
    auto valueName = root->Text;
    auto nameHash = GetCrushedName(valueName);
    if (ScopeCanResolve(parameterNames, nameHash)) {
        substitute = true;
        leafValue = ScopeResolve(parameterNames, nameHash);
    }

    // An unwrapped variable name?
    if (IsUnwrappedIdentifier(valueName, rootNode, compileContext)) {
        if (debug) {
            TCW_Comment(wr, StringNewFormat("// treating '\x01' as an implicit get()", valueName));
        }
        if (substitute) {
            TCW_Memory(wr, 'g', leafValue.data);
        } else {
            TCW_Memory(wr, 'g', valueName, 0);
        }

        return;
    }

    if (debug) {
        TCW_Comment(wr, StringNewFormat("// Value : '\x01'\r\n", DescribeSourceNode(root)));
        if (substitute) {
            TCW_Comment(wr, StringNewFormat("// Parameter reference redefined as '\x01'\r\n", valueName));
        }
    }

    switch (root->NodeType) {
    case NodeType::Numeric:
        // TODO: extend from integers
        int32_t numVal;
        if (!StringTryParse_int32(valueName, &numVal)) {
            TCW_AddError(wr, StringNewFormat("Failed to decode '\x01' as a number [#\03]", valueName, root->SourceLocation));
            return;
        }
        TCW_LiteralNumber(wr, numVal);
        break;

    case NodeType::StringLiteral:
        TCW_LiteralString(wr, valueName);
        break;

    case NodeType::Atom:
        if (StringAreEqual(valueName, "true")) TCW_LiteralNumber(wr,-1);
        else if (StringAreEqual(valueName, "false")) TCW_LiteralNumber(wr, 0);
        else if (substitute) TCW_RawToken(wr, leafValue);
        else TCW_VariableReference(wr, valueName);
        break;

    case NodeType::ScopeDelimiter:
    {
        // This is getting fired for the unit definition AND for empty parameter lists.
        // That's not happening in the C# side -- so probably something different in the parser
        // and/or the desugar transforms
        TCW_VariableReference(wr, valueName); // this is `()`, the unit definition
        auto desc = DescribeSourceNode(root);
        TCW_AddError(wr, StringNewFormat("Unexpected compiler state [#\02] near '\x01'", root->SourceLocation, desc));
        break;
    }
    default:
    {
        auto desc = DescribeSourceNode(root);
        TCW_AddError(wr, StringNewFormat("Unexpected compiler state [#\02] near '\x01'", root->SourceLocation, desc));
        break;
    }
    }
}

void CompileMemoryFunction(int level, bool debug, TreeNode* node, TagCodeCache* wr, Scope* parameterNames) {
    auto nodeData = TreeReadBody_SourceNode(node);
    // Check for special increment mode
    int8_t incr;
    String* target = NULL;
    if (StringAreEqual(nodeData->Text, "set") && CO_IsSmallIncrement(node, &incr, &target)) {
        TCW_Increment(wr, incr, target);
        return;
    }

    // build a sub-tree to compile in memory-access context
    // we slightly mutate the tree, so can't just take the child directly
    auto child = TreePivot(node);
    int paramCount = TreeCountChildren(child);
    auto childData = TreeReadBody_SourceNode(child);

    TCW_Merge(wr, Compile(child, level + 1, debug, parameterNames, NULL, Context::MemoryAccess));

    if (debug) { TCW_Comment(wr, StringNewFormat("// Memory function : '\x01'", nodeData->Text)); }

    char act = StringCharAtIndex(nodeData->Text, 0);
    TCW_Memory(wr, act, childData->Text, paramCount);
}

void CompileExternalFile(int level, bool debug, TreeNode* node, TagCodeCache* wr, Scope* parameterNames, HashMap* includedFiles) {
    //     1) Check against import list. If already done, warn and skip.
    //     2) Read file. Fail = terminate with error
    //     3) Compile to opcodes
    //     4) Inject opcodes in `wr`
    auto root = TreeReadBody_SourceNode(node);

    if (includedFiles == NULL) {
        TCW_AddError(wr, StringNewFormat("Files can only be included at the root level [#\03]", root->SourceLocation));
        return;
    }

    // check against import list
    auto firstChild = TreeChild(node);
    auto firstChildData = TreeReadBody_SourceNode(firstChild);
    auto targetFile = firstChildData->Text;

    if (MapGet_StringPtr_bool(includedFiles, targetFile, NULL)) {
        TCW_Comment(wr, StringNewFormat("// Ignored import: '\x01'", targetFile));
        return;
    }

    // read into buffer
    auto inclCode = StringEmpty();
    auto buffer = StringGetByteVector(inclCode);
    uint64_t read = 0;
    bool ok = FileLoadChunk(targetFile, buffer, 0, MAX_IMPORT_SIZE, &read);
    if (!ok || read >= MAX_IMPORT_SIZE) {
        TCW_AddError(wr, StringNewFormat("Import failed. Can't read file '\x01'", targetFile));
        return;
    }

    // Prevent double include
    MapPut_StringPtr_bool(includedFiles, targetFile, true, true);

    // Parse and compile
    TreeNode* parsed = ParseSourceCode(inclCode, false);
    auto programFragment = Compile(parsed, level, debug, parameterNames, includedFiles, Context::External);

    if (debug) { TCW_Comment(wr, StringNewFormat("// File import: '\x01'", targetFile)); }

    TCW_Merge(wr, programFragment);

    if (debug) { TCW_Comment(wr, StringNewFormat("// <-- End of file import: '\x01'", targetFile)); }
}

bool CompileConditionOrLoop(int level, bool debug, TreeNode* node, TagCodeCache* wr, Scope* parameterNames) {
    // return true if we output a value
    bool returns = false;

    auto nodeData = TreeReadBody_SourceNode(node);
    int nodeChildCount = TreeCountChildren(node);

    if (nodeChildCount < 1) {
        TCW_AddError(wr, StringNewFormat("\x01 requires parameter(s)", nodeData->Text));
        return false;
    }

    bool isLoop = StringAreEqual(nodeData->Text, "while");
    auto context = isLoop ? Context::Loop : Context::Condition;

    // Split the condition and action

    // build a tree for the if/while condition
    auto condition = TreeAllocate_SourceNode();
    auto condData = TreeReadBody_SourceNode(condition);
    condData->Text = StringNew("()");
    condData->SourceLocation = nodeData->SourceLocation;
    TreeAppendNode(condition, TreeChild(node));

    // build a tree for the if/while body
    int topOfBlock = TCW_Position(wr) - 1;
    auto body = TreeAllocate_SourceNode();
    auto bodyData = TreeReadBody_SourceNode(body);
    bodyData->Text = StringNew("()");
    bodyData->SourceLocation = nodeData->SourceLocation;

    auto chain = TreeSibling(TreeChild(node));
    TreeAppendNode(body, chain);

    auto compiledBody = Compile(body, level + 1, debug, parameterNames, NULL, context);
    returns |= TCW_ReturnsValues(compiledBody);
    int opCodeCount = TCW_OpCodeCount(compiledBody); // how far to jump over the body
    if (isLoop) opCodeCount++; // also skip the end unconditional jump

    if (debug) {
        TCW_Comment(wr, StringNewFormat( "// Compare condition for : '\x01', If false, skip \x02 element(s)", nodeData->Text, opCodeCount ));
    }

    if (CO_IsSimpleComparion(condition, opCodeCount)) {
        // output just the arguments
        CmpOp cmpOp;
        uint16_t argCount;
        auto argNodes = CO_ReadSimpleComparison(condition, &cmpOp, &argCount);
        auto conditionArgs = Compile(argNodes, level + 1, debug, parameterNames, NULL, context);
        TCW_Merge(wr, conditionArgs);
        TCW_CompoundCompareJump(wr, cmpOp, argCount, opCodeCount);
    } else {
        auto conditionCode = Compile(condition, level + 1, debug, parameterNames, NULL, context);

        if (debug) { TCW_Comment(wr, StringNewFormat("// Condition for : '\x01'", nodeData->Text)); }

        TCW_Merge(wr, conditionCode);
        TCW_CompareJump(wr, opCodeCount);
    }

    TCW_Merge(wr, compiledBody);

    if (debug) { TCW_Comment(wr, StringNewFormat("// End : \x01", nodeData->Text)); }
    if (isLoop) {
        int distance = TCW_Position(wr) - topOfBlock;
        TCW_UnconditionalJump(wr, distance);
    }
    return returns;
}

bool AllChildrenAreLeaves(TreeNode* node) {
    auto chain = TreeChild(node);
    while (chain != NULL) {
        if (TreeCountChildren(chain) > 0) return false;
        chain = TreeSibling(chain);
    }
    return true;
}

void CompileFunctionDefinition(int level, bool debug, TreeNode* node, TagCodeCache* wr, Scope* parameterNames) {
    // 1) Compile the func to a temporary string
    // 2) Inject a new 'def' op-code, that names the function and does an unconditional jump over it.
    // 3) Inject the compiled func
    // 4) Inject a new 'return' op-code

    auto nodeData = TreeReadBody_SourceNode(node);
    int nodeChildCount = TreeCountChildren(node);


    if (nodeChildCount != 2) {
        TCW_AddError(wr, StringNew("Function definition must have 3 parts: the name, the parameter list, and the definition.\r\nCall like `def (   myFunc ( param1 param2 ) ( ... statements ... )   )`"));
    }

    auto definitionNode = TreeChild(node); // parameters in parens
    auto bodyNode = TreeSibling(definitionNode); //expressions defining the function
    auto bodyData = TreeReadBody_SourceNode(bodyNode);

    if (!AllChildrenAreLeaves(definitionNode)) {
        auto str = StringNew("Function parameters must be simple names.\r\n");
        StringAppend(str, "`def ( myFunc (  param1  ) ( ... ) )` is OK,\r\n");
        StringAppend(str, "`def ( myFunc ( (param1) ) ( ... ) )` is not OK");
        TCW_AddError(wr, str);
        return;
    }
    if (!StringAreEqual(bodyData->Text, "()")) {
        TCW_AddError(wr, StringNew("Bare functions not supported. Wrap your function body in (parenthesis)"));
        return;
    }

    auto definitionData = TreeReadBody_SourceNode(definitionNode);
    auto functionName = definitionData->Text;
    auto argCount = TreeCountChildren(definitionNode);

    ParameterPositions(parameterNames, definitionNode, wr);

    auto subroutine = Compile(bodyNode, level, debug, parameterNames, NULL, Context::Default);
    int tokenCount = TCW_OpCodeCount(subroutine);

    if (debug) {
        TCW_Comment(wr, StringNewFormat("// Function definition : '\x01' with \x02 parameter(s)", functionName, argCount));
    }

    TCW_FunctionDefine(wr, functionName, argCount, tokenCount);
    TCW_Merge(wr, subroutine);

    if (TCW_ReturnsValues(subroutine)) {
        // Add an invalid return opcode. This will show an error message.
        TCW_InvalidReturn(wr);
    } else {
        // Add the 'return' call
        TCW_Return(wr, 0);
    }

    // Then the runner will need to interpret both the new op-codes
    // This would include a return-stack-push for calling functions,
    //   a jump-to-absolute-position by name
    //   a jump-to-absolute-position by return stack & pop
}

bool CompileFunctionCall(int level, bool debug, TagCodeCache* wr, TreeNode* node, Scope* parameterNames) {
    auto nodeData = TreeReadBody_SourceNode(node);
    auto funcName = nodeData->Text;

    if (NeedsDesugaring(funcName)) {
        node = DesugarProcessNode(funcName, parameterNames, node, wr);
        auto frag = Compile(node, level + 1, debug, parameterNames, NULL, Context::Default);
        TCW_Merge(wr, frag);
        return  TCW_ReturnsValues(frag);
    }

    TCW_Merge(wr, Compile(node, level + 1, debug, parameterNames, NULL, Context::Default));

    int nodeChildCount = TreeCountChildren(node);
    if (debug) { TCW_Comment(wr, StringNewFormat("// Function : '\x01' with \x02 parameter(s)", funcName, nodeChildCount)); }

    if (StringAreEqual(funcName, "return")) { TCW_Return(wr, nodeChildCount); } else { TCW_FunctionCall(wr, funcName, nodeChildCount); }

    return (StringAreEqual(funcName, "return")) && (nodeChildCount > 0); // is there a value return?
}

// Function/Program compiler. This is called recursively when subroutines are found
TagCodeCache* Compile(TreeNode* root, int indent, bool debug, Scope* parameterNames, HashMap* includedFiles, Context compileContext) {
    if (root == NULL) return NULL;

    auto wr = TCW_Allocate();

    // end of syntax line
    if (TreeIsLeaf(root)) {
        EmitLeafNode(root, debug, parameterNames, compileContext, wr);
        return wr;
    }

    // otherwise, recurse down (depth first)
    auto rootContainer = root;
    auto chain = TreeChild(root);
    while (chain != NULL) {
        if (TCW_HasErrors(wr)) return wr;

        auto node = chain;
        chain = TreeSibling(chain);
        if (TreeIsLeaf(node)) {
            TCW_Merge(wr, Compile(node, indent + 1, debug, parameterNames, includedFiles, compileContext));
            continue;
        }

        if (IsMemoryFunction(node)) {
            CompileMemoryFunction(indent, debug, node, wr, parameterNames);
        } else if (IsInclude(node)) {
            CompileExternalFile(indent, debug, node, wr, parameterNames, includedFiles);
        } else if (IsFlowControl(node)) {
            if (CompileConditionOrLoop(indent, debug, node, wr, parameterNames)) TCW_SetReturnsValues(wr);
        } else if (IsFunctionDefinition(node)) {
            CompileFunctionDefinition(indent, debug, node, wr, parameterNames);
        } else {
            if (CompileFunctionCall(indent, debug, wr, node, parameterNames)) TCW_SetReturnsValues(wr);
        }
    }
    return wr;
}