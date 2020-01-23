#include "SourceCodeTokeniser.h"
#include "MemoryManager.h"

typedef SourceNode Node; // 'SourceNode'. Typedef just for brevity

//RegisterTreeStatics(T)
//RegisterTreeFor(Node, T)

RegisterDTreeStatics(DT)
RegisterDTreeFor(Node, DT)

Node newNode(int sourceLoc, String* text, NodeType type) {
    auto n = Node{};
    n.NodeType = type;
    n.IsValid = true;
    n.Text = text;
    n.Unescaped = NULL;
    n.SourceLocation = sourceLoc;
    n.ErrorMessage = NULL;
    return n;
}
Node newNodeError(int sourceLoc, String* text) {
    auto n = Node{};
    n.NodeType = NodeType::InvalidNode;
    n.IsValid = false;
    n.Text = NULL;
    n.Unescaped = NULL;
    n.SourceLocation = sourceLoc;
    n.ErrorMessage = text;
    return n;
}
Node newNodeInvalid() {
    auto n = Node{};
    n.NodeType = NodeType::InvalidNode;
    n.IsValid = false;
    n.Text = NULL;
    n.Unescaped = NULL;
    n.SourceLocation = -1;
    return n;
}
Node newNodeOpenCall(int sourceLoc) {
    auto n = Node{};
    n.NodeType = NodeType::ScopeDelimiter;
    n.IsValid = true;
    n.Text = StringNew("()");
    n.Unescaped = StringNew("(");
    n.SourceLocation = sourceLoc;
    return n;
}
Node newNodeCloseCall(int sourceLoc) {
    auto n = Node{};
    n.NodeType = NodeType::ScopeDelimiter;
    n.IsValid = true;
    n.Text = StringNew(")");
    n.Unescaped = NULL;
    n.SourceLocation = sourceLoc;
    return n;
}
Node newNodeWhitespace(int sourceLoc, const char* str) {
    auto n = Node{};
    n.NodeType = NodeType::Whitespace;
    n.IsValid = true;
    n.Text = StringNew(str);
    n.Unescaped = NULL;
    n.SourceLocation = sourceLoc;
    return n;
}
Node newNodeDelimiter(int sourceLoc, char c) {
    auto n = Node{};
    n.NodeType = NodeType::Whitespace;
    n.IsValid = true;
    n.Text = StringNew(c);
    n.Unescaped = NULL;
    n.SourceLocation = sourceLoc;
    return n;
}
Node newNodeString(int sourceLoc, String* str) {
    auto n = Node{};
    n.NodeType = NodeType::StringLiteral;
    n.IsValid = true;
    n.Text = str;
    n.Unescaped = NULL;
    n.SourceLocation = sourceLoc;
    return n;
}
Node newNodeAtom(int sourceLoc, String* str) {
    auto n = Node{};
    n.NodeType = NodeType::Atom;
    n.IsValid = true;
    n.Text = str;
    n.Unescaped = NULL;
    n.SourceLocation = sourceLoc;
    return n;
}
Node newNodeDirective(int sourceLoc, String* str) {
    auto n = Node{};
    n.NodeType = NodeType::Directive;
    n.IsValid = true;
    n.Text = str;
    n.Unescaped = NULL;
    n.SourceLocation = sourceLoc;
    return n;
}

inline bool IsQuote(char c) {
    return (c == '"' || c == '\'' || c == '`');
}

// skip any whitespace (any of ',', ' ', '\t', '\r', '\n'). Most of the complexity is to capture metadata for auto-format
int SkipWhitespace(String* exp, int position, DTreeNode* mdParent) {
    int lastcap = position;
    int i = position;
    int length = StringLength(exp);

    bool capWS = false;
    bool capNL = false;
    auto tmp = Node{};

    while (i < length)
    {
        char c = StringCharAtIndex(exp, i);
        bool found = false;

        switch (c)
        {
        case ' ':
        case ',':
        case '\t':
            found = true;
            if (capNL) {
                // switch from newlines to regular space
                // output NL so far
                if (mdParent != NULL) DTAddChild_Node(*mdParent, newNode(lastcap, StringSlice(exp, lastcap, i - lastcap), NodeType::Newline));
                lastcap = i;
            }
            capNL = false;
            capWS = true;
            i++;
            break;

        case '\r':
        case '\n':
            found = true;
            if (capWS) {
                // switch from regular space to newlines
                // output WS so far
                if (mdParent != NULL) DTAddChild_Node(*mdParent, newNode(lastcap, StringSlice(exp, lastcap, i - lastcap), NodeType::Whitespace));
                lastcap = i;
            }
            i++;
            capNL = true;
            capWS = false;
            break;
        }
        if (!found)
        {
            break;
        }
    }


    if (i != lastcap) {
        if (capNL) {
            if (mdParent != NULL) {
                DTAddChild_Node(*mdParent, newNode(lastcap, StringSlice(exp, lastcap, i - lastcap), NodeType::Newline));
            }
        }
        if (capWS) {
            if (mdParent != NULL) {
                DTAddChild_Node(*mdParent, newNode(lastcap, StringSlice(exp, lastcap, i - lastcap), NodeType::Whitespace));
            }
        }
    }

    return i;
}

// convert escape sequences to their real values
char Unescape(char car) {
    switch (car) {
    case '\\': return '\\';
    case 't': return '\t';
    case 'r': return '\r';
    case 'n': return '\n';
    case 'e': return '\x1B'; // ASCII escape
    case '0': return '\0';

    default: return car;
    }
}

// read a string literal from the source code
String* ReadString (String* exp, int* inOutPosition, char end, bool* outEndedCorrectly) {
    int i = *inOutPosition;
    int length = StringLength(exp);
    auto sb = StringEmpty();
    *outEndedCorrectly = false;
    
    char end2 = end;
    if (end == '`') { end2 = '\'';} // allow `quote' as a string

    while (i < length) {
        char car = StringCharAtIndex(exp, i);

        if (car == '\\') { // escape sequences
            int nb = 0;
            i++;
            while (i < length) {
                car = StringCharAtIndex(exp, i);
                if (car == '\\') {
                    if (nb % 2 == 0) {
                        StringAppendChar(sb, car);
                    }
                } else if (car == end && nb % 2 == 1) {
                    i--;
                    break;
                } else {
                    StringAppendChar(sb, Unescape(car));
                    break;
                }

                nb++;
                i++;
            }
        } else if (car == end || car == end2) { // end of string
            *outEndedCorrectly = true;
            break;
        } else { // normal characters
            StringAppendChar(sb, car);
        }

        i++;
    }

    *inOutPosition = i;
    return sb;
}

int NextNewline(String* source, int i) {
    unsigned int npos;
    unsigned int rpos;
    bool nfound = StringFind(source, '\n', i, &npos);
    bool rfound = StringFind(source, '\r', i, &rpos);

    if (nfound && rfound) {
        return (npos < rpos) ? npos : rpos;
    } else if (nfound) {
        return npos;
    } else if (rfound) {
        return rpos;
    } else {
        return StringLength(source);
    }
}

// Read an identifier
String* ReadWord(String* expression, int position) {
    int i = position;
    int length = StringLength(expression);
    auto sb = StringEmpty();

    while (i < length)
    {
        char c = StringCharAtIndex(expression, i);

        if (c == ' ' || c == '\n' || c == '\r' || c == '\t' // whitespace
            || c == ')' || c == '('                         // parens
            || c == ',' || c == ':')                        // special
        {
            break;
        }

        StringAppendChar(sb, c);

        i++;
    }
    return sb;
}

// Try to read a line comment. Returns false if it's not a line comment
bool TryCaptureComment(String* source, int* inOutPosition, bool preserveMetadata, DTreeNode* mdParent)
{
    unsigned int end;
    int i = *inOutPosition;
    if (i >= StringLength(source)) return false;
    char k = StringCharAtIndex(source, i + 1);
    Node tmp = newNodeInvalid();

    switch (k)
    {
    case '/': // line comment
        end = NextNewline(source, i);
        if (preserveMetadata) {
            tmp = newNode(i, StringSlice(source, i, end - i), NodeType::Comment);
            DTAddChild_Node(*mdParent, tmp);
        }
        *inOutPosition = end - 1;
        return true;
    case '*': // block comment
    {
        bool found = StringFind(source, "*/", i + 2, &end);
        if (!found) end = StringLength(source);
        if (preserveMetadata) {
            tmp = newNode(i, StringSlice(source, i, end - i + 2), NodeType::Comment);
            DTAddChild_Node(*mdParent, tmp);
        }
        *inOutPosition = end + 1;
        return true;
    }
    default: return false;
    }
}

bool IsNumeric(String* word) {
    return StringTryParse_double(word, NULL);
}

void MaybeIncludeWhitespace(bool y, DTreeNode* wsNode, DTreeNode target) {
    if (wsNode == NULL || wsNode == NULL) return;
    if (y) {
        int id = DTGetChildId(*wsNode);
        DTAppendSubtree(target, DTNode(wsNode->Tree, id)); // this does a copy
    }
    DTClear(wsNode->Tree);
}
void PrepareWhitespaceContainer(DTreeNode* wsNode) {
    if (wsNode == NULL) return;
    
    DTClear(wsNode->Tree);
    wsNode->NodeId = DTRootId(wsNode->Tree);
}

bool ParseSource(String* source, DTreePtr tree, int position, bool preserveMetadata) {
    int i = position;
    int length = StringLength(source);
    auto current = DTNode(tree, DTRootId(tree));
    //Node tmp = newNodeInvalid();
    DTreeNode parent = current;

    DTreePtr wsTree = DTAllocate_Node(DTArena(tree));
    DTreeNode wsNode = DTNode(wsTree, DTRootId(wsTree));

    while (i < length)
    {
        PrepareWhitespaceContainer(&wsNode);
        i = SkipWhitespace(source, i, &wsNode);
        MaybeIncludeWhitespace(preserveMetadata, &wsNode, current);

        if (i >= length) { break; }

        char car = StringCharAtIndex(source, i);


        switch (car)
        {
        case '(': // start of call
            parent = current;
            current.NodeId = DTAddChild_Node(parent, newNodeOpenCall(i));
            break;
        case ')': // end of call
        {
            if (preserveMetadata) {
                DTAddChild_Node(current, newNodeCloseCall(i));
            }

            current.NodeId = DTGetParentId(current);
            if (!DTValidNode(current)) {
				DTAddChild_Node(current, newNodeError(i, StringNew("###PARSER ERROR: ROOT CRASH###")));
                return false;
            }
            break;
        }

        case '"': // start of strings
        case '\'':
        case '`':
        {
            if (preserveMetadata) {
                DTAddChild_Node(current, newNodeDelimiter(i, car));
            }

            i++;
            auto old_i = i;
            bool endedCorrectly;
            auto words = ReadString(source, &i, car, &endedCorrectly);

            auto tmp = newNodeString(old_i, words);
            if (preserveMetadata) {
                tmp.Unescaped = StringSlice(source, old_i, i - old_i);
            }

            DTAddChild_Node(current, tmp);

            if (preserveMetadata && endedCorrectly) {
                car = StringCharAtIndex(source, i);
                DTAddChild_Node(current, newNodeDelimiter(i, car));
            }
            break;
        }

        case '/': //maybe a comment?
            if (TryCaptureComment(source, &i, preserveMetadata, &current))
            {
                break;
            }
            // else, fall through to default

        default:
        {
            auto word = ReadWord(source, i);
            int wordLength = StringLength(word);
            if (wordLength == 0) {
                StringDeallocate(word);
            } else {
                int startLoc = i;
                i += wordLength;

                PrepareWhitespaceContainer(&wsNode);
                i = SkipWhitespace(source, i, &wsNode);
                if (i > length) {
                    // Unexpected end of input
                    // To help formatting and diagnosis, write the last bits.
                    DTAddSibling_Node(current, newNodeAtom(startLoc, word));
                    MaybeIncludeWhitespace(preserveMetadata, &wsNode, current);
                    return false;
                }
                car = StringCharAtIndex(source, i);
                if (car == '(')
                {
                    // we need the atom we create to NOT be a leaf!
                    if (IsNumeric(word)) {
                        DTAddChild_Node(current, 
                            newNodeError(i, StringNewFormat("Error: '\x01' used like a function name, but looks like a number", word ))
                        );
                        return false;
                    } else {
						parent = current;
						auto tmp = newNodeAtom(startLoc, word);
						tmp.functionLike = true;
						current.NodeId = DTAddChild_Node(parent, tmp);

						MaybeIncludeWhitespace(preserveMetadata, &wsNode, current);
						if (preserveMetadata) {
							DTAddChild_Node(current, newNodeOpenCall(i));
						}
                    }
                }
                else if (car == ':') {
                    
                    // Scheduler directive. We expect a string here.
                    
					StringAppendChar(word, ':'); // the ':' is part of the directive name, so it doesn't clash with variables.
                    i++;
					i = SkipWhitespace(source, i, &wsNode);
                    MaybeIncludeWhitespace(preserveMetadata, &wsNode, current);
					car = StringCharAtIndex(source, i);

                    if (IsQuote(car)) {
                        // OK, a directive string
						i++;
						auto old_i = i;
						bool endedCorrectly;
						auto words = ReadString(source, &i, car, &endedCorrectly);
                        i++;

                        if (!endedCorrectly) {
							DTAddChild_Node(current, 
                                newNodeError(i, StringNewFormat("\r\nError: '\x01' system directive argument was not ended correctly", word))
                            );
							return false;
                        }

                        // node for directive
						auto tmp = newNodeDirective(startLoc, word);
						tmp.functionLike = true;

                        // add to tree
						//parent = current;
						auto dirPos = DTAddChild_Node(current, tmp);
                        
						if (preserveMetadata) {
							DTAddChild_Node(current, newNodeDelimiter(i, car));
						}

                        // add directive argument to directive node
						int id = DTAddChild_Node(current.Tree, dirPos, newNodeString(old_i, words));
                        

                        // This isn't working well
						if (preserveMetadata && endedCorrectly) {
							car = StringCharAtIndex(source, i);
							DTAddChild_Node(current, newNodeDelimiter(i, car));
						}
					}
					else {
						DTAddChild_Node(current, 
                            newNodeError(i, StringNewFormat("\r\nError: '\x01' looks like a system directive, but you didn't give a string", word))
                        );
						return false;
					}
                }
                else
                {
                    i--;
                    auto tmp = newNodeAtom(startLoc, word);
                    if (IsNumeric(word)) tmp.NodeType = NodeType::Numeric;

                    DTAddChild_Node(current, tmp);

                    MaybeIncludeWhitespace(preserveMetadata, &wsNode, current);
                }
            }
        }

        break; // end of 'default'
        }

        i++;
    }
    DTDeallocate(wsNode);
    return true;
}

DTreePtr ParseSourceCode(ArenaPtr arena, String* source, bool preserveMetadata) {
    auto root = Node();
    root.NodeType = NodeType::Root;
    root.SourceLocation = 0;
    root.IsValid = true;
    root.Text = NULL;
    root.Unescaped = NULL;
    root.FormattedLocation = 0;

    auto tree = DTAllocate_Node(arena);
    DTSetValue_Node(tree, 0, root);

    bool valid = ParseSource(source, tree, 0, preserveMetadata);

    if (!valid) {
        root.IsValid = false;
		DTSetValue_Node(tree, 0, root);
    }

    return tree;
}

void DeallocateAST(DTreePtr ast) {
    if (ast == NULL) return;

    auto vec = DTAllData(ast, NULL); // vector of SourceNode

    Node n = Node{};
    while (VectorPop(vec, &n)) {
        StringDeallocate(n.ErrorMessage);
        StringDeallocate(n.Text);
        StringDeallocate(n.Unescaped);
    }

    VectorDeallocate(vec);
    DTreeDeallocate(&ast);
}


String* DescribeNodeType(NodeType nt) {
    switch (nt) {
    case NodeType::Atom: return StringNew("Atom");
    case NodeType::Comment: return StringNew("[comment]");
    case NodeType::Default: return StringNew("Default");
    case NodeType::Delimiter: return StringNew("Delimiter");
    case NodeType::InvalidNode: return StringNew("<INVALID>");
    case NodeType::Newline: return StringNew("[newline]");
    case NodeType::Numeric: return StringNew("Numeric");
    case NodeType::Root: return StringNew("<AST ROOT>");
    case NodeType::ScopeDelimiter: return StringNew("[scope delimiter]");
    case NodeType::StringLiteral: return StringNew("String");
    case NodeType::Whitespace: return StringNew("[whitespace]");
    case NodeType::Directive: return StringNew("Directive");
    default: return StringNew("<UNKNOWN>");
    }
}

String* DescribeSourceNode(SourceNode *n) {
    if (n == NULL) return NULL;
    auto str = DescribeNodeType(n->NodeType);
    StringAppendChar(str, ' ');
    StringAppend(str, (n->Unescaped == NULL) ? n->Text : n->Unescaped);
    return str;
}

// Recursively descend into the nodes.
// TODO: handle highlighting, cursor position
void Render_Rec(DTreePtr tree, int nodeId, int indent, String* outp) {

    Node* n = DTReadBody_Node(tree, nodeId);

    // Write the node contents
    if (n->IsValid) {
        StringAppend(outp, (n->Unescaped == NULL) ? n->Text : n->Unescaped);
    } else {
        StringAppend(outp, n->ErrorMessage);
	}

    // Recursively write child nodes
    bool leadingWhite = false;
    auto childId = DTGetChildId(tree, nodeId);
    while (childId >= 0) {
        n = DTReadBody_Node(tree, childId);

        // Handle re-indenting
        if (leadingWhite) {
            if (n->NodeType == NodeType::Whitespace) { // skip leading whitespace
                childId = DTGetSiblingId(tree, childId);
                continue;
            }

            leadingWhite = false;
            if (n->NodeType == NodeType::ScopeDelimiter) {
                StringAppendChar(outp, ' ', (indent - 1) * 4);
            }
            else {
                StringAppendChar(outp, ' ', (indent) * 4);
            }
        }
        if (n->NodeType == NodeType::Newline) { // TODO: newlines aren't rendering correctly
            leadingWhite = true;
        }

        // write node text
        Render_Rec(tree, childId, indent + 1, outp);
        childId = DTGetSiblingId(tree, childId); // next node at this level
    }
}

String* RenderAstToSource(DTreePtr ast) {
    if (ast == NULL) return NULL;

    auto outp = StringEmpty();

    Render_Rec(ast, DTRootId(ast), 0, outp);
    
    return outp;
}
