#include "Desugar.h"
#include "SourceCodeTokeniser.h"
#include "MemoryManager.h"

#include <cassert>

RegisterDTreeStatics(DT)
RegisterDTreeFor(SourceNode, DT)

String* SugarName(String* original, int position) {
    return StringNewFormat("__\x01_s\x02", original, position);
}

bool NeedsDesugaring(String* funcName) {
    if (StringAreEqual(funcName, "pick")) return true; // switch / if-else chain

    return false;
}

static void AddReturnNode(DTreePtr tree, int parentId) {
    auto data = SourceNode{};
    data.Text = StringNew("return");
    data.NodeType = NodeType::Atom;
    data.functionLike = true;

    DTAddChild_SourceNode(tree, parentId, data);
}

DTreeNode ConvertToPickList(String* funcName, DTreeNode sourceNode, TagCodeCache* tcc) {
    if (DTCountChildren(sourceNode) < 1) {
        TCW_AddError(tcc, StringNew("Empty pick list"));
        return sourceNode;
    }

    // Each `if` gets a return at the end.
    // Any immediate child that is not an `if` is an error

    auto sourceData = DTReadBody_SourceNode(sourceNode);
    auto tree = sourceNode.Tree;

    // For each `if` statement, inject a `return` at the end of it.
    auto chain = DTGetChildId(sourceNode);
    while (chain >= 0) {
        auto nub = DTReadBody_SourceNode(tree, chain);
        if (!StringAreEqual(nub->Text, "if")) {
            auto msg = StringNew("'pick' must contain a list of 'if' calls, and nothing else.");
            StringAppend(msg, "If you want something that runs in every-other-case, use\r\n");
            StringAppend(msg, "if ( true ...");
            TCW_AddError(tcc, msg);
            return sourceNode;
        }

        AddReturnNode(tree, chain);
        chain = DTGetSiblingId(tree, chain);
    }

    // The entire lot then gets wrapped in a function def and immediately called
    auto newFuncName = SugarName(funcName, sourceData->SourceLocation);

    StringClear(sourceData->Text);

    auto defData = SourceNode{}; defData.Text = StringNew("def");
    auto nameData = SourceNode{}; nameData.Text = newFuncName;
    auto callData = SourceNode{}; callData.Text = newFuncName; callData.functionLike = true;
    auto parenData = SourceNode{}; parenData.Text = StringNew("()"); parenData.NodeType = NodeType::Atom;

    // TODO: rather than making a new subtree, we should use our insertion.
    // inject a def block between `sourceNode` and its children


    /*
    BEFORE:

    pick
        ()
            if
                ()
                ...

    AFTER:

    def
        <synth name>
			()
				if
					()
					...
					return

    */

    // get the parent node
    int parent = DTGetParentId(sourceNode);

    // clip out the source node (we will re-add later)
    int blockContent = DTGetChildId(tree, sourceNode.NodeId);
    int addIdx = DTRemoveChildById(tree, parent, sourceNode.NodeId);

    // add a new child to hold the function definition
    int defId = DTInsertChild_SourceNode(tree, parent, addIdx, defData);

    int funcNameBlock = DTAddChild_SourceNode(tree, defId, nameData); // name and empty param list
    int inter = DTAddChild_SourceNode(tree, defId, parenData); // function definition node
    DTAppendSubtree(tree, inter, tree, blockContent); // re-add our modified pick block contents

    int callBlock = DTInsertChild_SourceNode(tree, parent, addIdx+1, callData); // call the function we just defined.

    /*
    auto wrapper = TreeAllocate_SourceNode(TreeArena(sourceNode)); // new tree node
    auto defineBlock = TreeAddChild_SourceNode(wrapper, &defData); // function def
    auto funcNameBlock = TreeAddChild_SourceNode(defineBlock, &nameData); // name, empty param list


    auto inter = TreeAddChild_SourceNode(defineBlock, &parenData); // function def
    TreeAppendNode(inter, TreeChild(sourceNode)); // modified pick block

    auto callBlock = TreeAddChild_SourceNode(wrapper, &callData); // call the function
    
    return wrapper;
    */
    return DTNode(tree, defId);
}

DTreeNode DesugarProcessNode(String* funcName, Scope* parameterNames, DTreeNode node, TagCodeCache* tcc) {
    if (StringAreEqual(funcName, "pick")) return ConvertToPickList(funcName, node, tcc); // switch / if-else chain

    TCW_AddError(tcc, StringNewFormat("Desugar for '\x01' is declared but not implemented", funcName));
    return DTNode(NULL, -1);
}
