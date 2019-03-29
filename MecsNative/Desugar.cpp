#include "Desugar.h"
#include "SourceCodeTokeniser.h"

#include <cassert>

RegisterTreeFor(SourceNode, Tree)

String* SugarName(String* original, int position) {
    return StringNewFormat("__\x01_s\x02", original, position);
}

bool NeedsDesugaring(String* funcName) {
    if (StringAreEqual(funcName, "pick")) return true; // switch / if-else chain

    return false;
}

TreeNode* MakeReturnNode() {
    auto node = TreeAllocate_SourceNode();
    auto data = TreeReadBody_SourceNode(node);

    data->Text = StringNew("return");
    data->NodeType = NodeType::Atom;
    data->functionLike = true;

    return node;
}

TreeNode* ConvertToPickList(String* funcName, TreeNode* sourceNode, TagCodeCache* tcc) {
    if (TreeChild(sourceNode) == NULL) {
        TCW_AddError(tcc, StringNew("Empty pick list"));
        return NULL;
    }

    // Each `if` gets a return at the end.
    // Any immediate child that is not an `if` is an error

    auto sourceData = TreeReadBody_SourceNode(sourceNode);

    auto chain = TreeChild(sourceNode);
    while (chain != NULL) {
        auto nub = TreeReadBody_SourceNode(chain);
        if (!StringAreEqual(nub->Text, "if")) {
            auto msg = StringNew("'pick' must contain a list of 'if' calls, and nothing else.");
            StringAppend(msg, "If you want something that runs in every-other-case, use\r\n");
            StringAppend(msg, "if ( true ...");
            TCW_AddError(tcc, msg);
            return NULL;
        }

        TreeAppendNode(chain, MakeReturnNode());
        chain = TreeSibling(chain);
    }

    // The entire lot then gets wrapped in a function def and immediately called
    auto newFuncName = SugarName(funcName, sourceData->SourceLocation);

    StringClear(sourceData->Text);

    auto defData = SourceNode{}; defData.Text = StringNew("def");
    auto nameData = SourceNode{}; nameData.Text = newFuncName;
    auto callData = SourceNode{}; callData.Text = newFuncName; callData.functionLike = true;
    auto parenData = SourceNode{}; parenData.Text = StringNew("()"); parenData.NodeType = NodeType::Atom;

    auto wrapper = TreeAllocate_SourceNode();
    auto defineBlock = TreeAddChild_SourceNode(wrapper, &defData); // function def
    auto funcNameBlock = TreeAddChild_SourceNode(defineBlock, &nameData); // name, empty param list

    int count = TreeCountChildren(sourceNode);

    auto inter = TreeAddChild_SourceNode(defineBlock, &parenData); // function def
    TreeAppendNode(inter, TreeChild(sourceNode)); // modified pick block

    auto callBlock = TreeAddChild_SourceNode(wrapper, &callData); // call the function

    return wrapper;
}

TreeNode* DesugarProcessNode(String* funcName, Scope* parameterNames, TreeNode* node, TagCodeCache* tcc) {
    if (StringAreEqual(funcName, "pick")) return ConvertToPickList(funcName, node, tcc); // switch / if-else chain

    TCW_AddError(tcc, StringNewFormat("Desugar for '\x01' is declared but not implemented", funcName));
    return NULL;
}
