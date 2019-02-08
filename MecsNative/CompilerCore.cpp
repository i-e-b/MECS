#include "CompilerCore.h"
#include "SourceCodeTokeniser.h"

#include "TimingSys.h"


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
    /*
    // An unwrapped variable name?
    if (IsUnwrappedIdentifier(valueName, rootNode, compileContext)) {
        if (debug) {
            wr.Comment("// treating '" + valueName + "' as an implicit get()");
        }
        if (substitute) wr.Memory('g', leafValue);
        else wr.Memory('g', valueName, 0);

        return;
    }

    if (debug) {
        wr.Comment("// Value : \"" + root + "\"\r\n");
        if (substitute)
        {
            wr.Comment("// Parameter reference redefined as '" + valueName + "'\r\n");
        }
    }

    switch (root->NodeType) {
    case NodeType::Numeric:
        wr.LiteralNumber(double.Parse(valueName.Replace("_", "")));
        break;

    case NodeType::StringLiteral:
        wr.LiteralString(valueName);
        break;

    case NodeType::Atom:
        if (valueName == "true") wr.LiteralInt32(-1);
        else if (valueName == "false") wr.LiteralInt32(0);
        else if (substitute) wr.RawToken(leafValue);
        else wr.VariableReference(valueName);
        break;

    default:
        throw new Exception("Unexpected compiler state");
    }
    */
}

void CompileMemoryFunction(int level, bool debug, TreeNode* node, TreeNode* container, TagCodeCache* wr, Scope* parameterNames) {

}
void CompileExternalFile(int level, bool debug, TreeNode* node, TagCodeCache* wr, Scope* parameterNames, HashMap* includedFiles) {

}
bool CompileConditionOrLoop(int level, bool debug, TreeNode* container, TreeNode* node, TagCodeCache* wr, Scope* parameterNames) {
    // return true if we output a value
    return false;
}
void CompileFunctionDefinition(int level, bool debug, TreeNode* node, TagCodeCache* wr, Scope* parameterNames) {

}
bool CompileFunctionCall(int level, bool debug, TagCodeCache* wr, TreeNode* node, Scope* parameterNames) {
    // TODO
    return false;
}

// Function/Program compiler. This is called recursively when subroutines are found
TagCodeCache* Compile(TreeNode* root, int indent, bool debug, Scope* parameterNames, HashMap* includedFiles, Context compileContext) {
    if (root == NULL) return NULL;

    auto wr = TCW_Allocate();

    // end of syntax line
    if (TreeIsLeaf(root))
    {
        EmitLeafNode(root, debug, parameterNames, compileContext, wr);
        return wr;
    }

    // otherwise, recurse down (depth first)
    auto rootContainer = root;
    auto chain = TreeChild(root);
    while (chain != NULL) {
        auto node = chain;
        chain = TreeSibling(chain);
        if (TreeIsLeaf(node)) {
            TCW_Merge(wr, Compile(node, indent + 1, debug, parameterNames, includedFiles, compileContext));
            continue;
        }

        auto container = node;
        if (IsMemoryFunction(node)) {
            CompileMemoryFunction(indent, debug, node, container, wr, parameterNames);
        } else if (IsInclude(node)) {
            CompileExternalFile(indent, debug, node, wr, parameterNames, includedFiles);
        } else if (IsFlowControl(node)) {
            if (CompileConditionOrLoop(indent, debug, container, node, wr, parameterNames)) TCW_SetReturnsValues(wr);
        } else if (IsFunctionDefinition(node)) {
            CompileFunctionDefinition(indent, debug, node, wr, parameterNames);
        } else {
            if (CompileFunctionCall(indent, debug, wr, node, parameterNames)) TCW_SetReturnsValues(wr);
        }
    }

}