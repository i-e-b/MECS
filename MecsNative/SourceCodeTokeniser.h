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

    // Negative values are only included if meta-data is retained

    // A comment block (including delimiters)
    Comment = -1,
    // Whitespace excluding newlines. The *does* include ignored characters like ',' and ';'
    Whitespace = -2,
    // A single newline
    Newline = -3,
    // Opening or closing parenthesis
    ScopeDelimiter = -4,
    // String delimiter or similar
    Delimiter = -5
};

typedef struct SourceNode {
    // Semantic class of the node
    NodeType NodeType;

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
Tree* Read(String* source, bool preserveMetadata);

// Clean up tree
void DeallocateAST(Tree* ast);

#endif