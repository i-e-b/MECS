#pragma once

#ifndef sourcecodetokeniser_h
#define sourcecodetokeniser_h

#include "Tree.h"
#include "String.h"

enum NodeType {
    // Unspecified node. Determined by context
    Default = 0,
    // A string literal in the source code
    StringLiteral = 1,
    // A number literal in the source code
    Numeric = 2,
    // An 'atom' -- variable name, function name, etc
    Atom = 3,
    // The root of a parsed document
    Root = 4,

    // A node that has not been correctly configured
    InvalidNode = 255,

    // Only included if meta-data is retained:

    // A comment block (including delimiters)
    Comment = 101,
    // Whitespace excluding newlines. The *does* include ignored characters like ',' and ';'
    Whitespace = 102,
    // A single newline
    Newline = 103,
    // Opening or closing parenthesis
    ScopeDelimiter = 104,
    // String delimiter or similar
    Delimiter = 105,
    // Cursor start position
    CaretLeft = 106,
    // Cursor end position
    CaretRight = 107
};

typedef struct SourceNode {
    // Semantic class of the node
    NodeType NodeType;

    // If true, this atom is used like a function call.
    bool functionLike;

    // Text value of the node. For strings, this is after escape codes are processed
    String* Text;

    // Raw source text of the node, if applicable. For strings, this is before escape codes are processed.
    String* Unescaped;

    // Location in the source file that this node was found
    int SourceLocation;

    // If false, the parse tree was not successful
    bool IsValid;

    // If not valid, a reason or explanation. Can be NULL.
    String* ErrorMessage;

    // Start location in the source file from last time the node was formatted
    int FormattedLocation;

    // Length of contents (in characters) from the last time the node was formatted
    int FormattedLength;
} SourceNode;

/// <summary>
/// Read an source string and output tree of `SourceNode`
/// </summary>
/// <param name="source">Input text</param>
/// <param name="preserveMetadata">if true, comments and spacing will be included</param>
TreeNode* ParseSourceCode(String* source, bool preserveMetadata);

// Write the abstract syntax tree out as a source code string. This does auto-formatting
String* RenderAstToSource(TreeNode* ast);

// Create a human-readable description of a SourceNode
String* DescribeSourceNode(SourceNode *n);

// Create a human-readable description of a node type
String* DescribeNodeType(NodeType nt);

// Clean up tree, deallocating anything stored during the `Read` process
void DeallocateAST(TreeNode* ast);

#endif