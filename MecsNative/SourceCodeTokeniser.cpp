#include "SourceCodeTokeniser.h"
#include "MemoryManager.h"

typedef SourceNode Node; // 'SourceNode'. Typedef just for brevity

RegisterTreeStatics(T)
RegisterTreeFor(Node, T)

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

// skip any whitespace (any of ',', ' ', '\t', '\r', '\n'). Most of the complexity is to capture metadata for auto-format
int SkipWhitespace(String* exp, int position, TreeNode* mdParent) {
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
                if (mdParent != NULL) TAddChild_Node(mdParent, &newNode(lastcap, StringSlice(exp, lastcap, i - lastcap), NodeType::Newline));
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
                if (mdParent != NULL) TAddChild_Node(mdParent, &newNode(lastcap, StringSlice(exp, lastcap, i - lastcap), NodeType::Whitespace));
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
                TAddChild_Node(mdParent, &newNode(lastcap, StringSlice(exp, lastcap, i - lastcap), NodeType::Newline));
            }
        }
        if (capWS) {
            if (mdParent != NULL) {
                TAddChild_Node(mdParent, &newNode(lastcap, StringSlice(exp, lastcap, i - lastcap), NodeType::Whitespace));
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
        } else if (car == end) { // end of string
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

        if (c == ' ' || c == '\n' || c == '\t' || c == ')'
            || c == '(' || c == ',' || c == '\r')
        {
            break;
        }

        StringAppendChar(sb, c);

        i++;
    }
    return sb;
}

// Try to read a line comment. Returns false if it's not a line comment
bool TryCaptureComment(String* source, int* inOutPosition, bool preserveMetadata, TreeNode* mdParent)
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
            TAddChild_Node(mdParent, &tmp);
        }
        *inOutPosition = end - 1;
        return true;
    case '*': // block comment
    {
        bool found = StringFind(source, "*/", i + 2, &end);
        if (!found) end = StringLength(source);
        if (preserveMetadata) {
            tmp = newNode(i, StringSlice(source, i, end - i + 2), NodeType::Comment);
            TAddChild_Node(mdParent, &tmp);
        }
        *inOutPosition = end + 1;
        return true;
    }
    default: return false;
    }
}

bool IsNumeric(String* word) {
    return StringTryParse_f16(word, NULL);
}

void MaybeIncludeWhitespace(bool y, TreeNode** wsNode, TreeNode* target) {
    if (wsNode == NULL || *wsNode == NULL) return;
    if (y) {
        TAppendNode(target, TChild(*wsNode)); // include any childen, but not the top node
        mfree(*wsNode); // specifically NOT dealloc the whole chain -- it's added to 'target' now
        *wsNode = NULL; // kill the whitespace node ref
    } else {
        TDeallocate(*wsNode); // Not using the whitespace. Clean it all up
        *wsNode = NULL; // kill the whitespace node ref
    }
}
void PrepareWhitespaceContainer(TreeNode** wsNode) {
    if (wsNode == NULL) return;
    if (*wsNode != NULL) TDeallocate(*wsNode); // didn't get used. Clean it up.
    *wsNode = TBareNode_Node();
}

bool ParseSource(String* source, TreeNode* root, int position, bool preserveMetadata) {
    int i = position;
    int length = StringLength(source);
    auto current = root;
    Node tmp = newNodeInvalid();
    TreeNode* parent = current;
    TreeNode* wsNode = NULL;

    while (i < length)
    {
        PrepareWhitespaceContainer(&wsNode);
        i = SkipWhitespace(source, i, wsNode);
        MaybeIncludeWhitespace(preserveMetadata, &wsNode, current);

        if (i >= length) { break; }

        char car = StringCharAtIndex(source, i);


        switch (car)
        {
        case '(': // start of call
            parent = current;
            tmp = newNodeOpenCall(i);
            current = TAddChild_Node(parent, &tmp);
            break;
        case ')': // end of call
        {
            if (preserveMetadata) {
                tmp = newNodeCloseCall(i);
                TAddChild_Node(current, &tmp);
            }

            current = TParent(current);
            if (current == NULL) {
                tmp = newNodeError(i, StringNew("###PARSER ERROR: ROOT CRASH###"));
                TAddChild_Node(current, &tmp);
                return false;
            }
            break;
        }

        case '"': // start of strings
        case '\'':
        {
            if (preserveMetadata) {
                tmp = newNodeDelimiter(i, car);
                TAddChild_Node(current, &tmp);
            }

            i++;
            auto old_i = i;
            bool endedCorrectly;
            auto words = ReadString(source, &i, car, &endedCorrectly);

            tmp = newNodeString(old_i, words);
            if (preserveMetadata) {
                tmp.Unescaped = StringSlice(source, old_i, i - old_i);
            }

            TAddChild_Node(current, &tmp);

            if (preserveMetadata && endedCorrectly) {
                tmp = newNodeDelimiter(i, car);
                TAddChild_Node(current, &tmp);
            }
            break;
        }

        case '/': //maybe a comment?
            if (TryCaptureComment(source, &i, preserveMetadata, current))
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
                i = SkipWhitespace(source, i, wsNode);
                if (i >= length) {
                    // Unexpected end of input
                    // To help formatting and diagnosis, write the last bits.
                    TAddSibling_Node(current, &newNodeAtom(startLoc, word));
                    MaybeIncludeWhitespace(preserveMetadata, &wsNode, current);
                    return false;
                }
                car = StringCharAtIndex(source, i);
                if (car == '(')
                {
                    if (IsNumeric(word)) {
                        tmp = newNodeError(i, StringNew("Error: '"));
                        StringAppend(tmp.ErrorMessage, word);
                        StringAppend(tmp.ErrorMessage, "' used like a function name, but looks like a number");

                        TAddChild_Node(current, &tmp);
                        return false;
                    }

                    parent = current;
                    tmp = newNodeAtom(startLoc, word);
                    current = TAddChild_Node(parent, &tmp);

                    MaybeIncludeWhitespace(preserveMetadata, &wsNode, current);
                    if (preserveMetadata) {
                        TAddChild_Node(current, &newNodeOpenCall(i));
                    }
                }
                else
                {
                    i--;
                    tmp = newNodeAtom(startLoc, word);
                    if (IsNumeric(word)) tmp.NodeType = NodeType::Numeric;

                    TAddChild_Node(current, &tmp);

                    MaybeIncludeWhitespace(preserveMetadata, &wsNode, current);
                }
            }
        }

        break; // end of 'default'
        }

        i++;
    }
    if (wsNode != NULL) TDeallocate(wsNode);
    return true;
}

TreeNode* ParseSourceCode(String* source, bool preserveMetadata) {
    auto root = Node();
    root.NodeType = NodeType::Root;
    root.SourceLocation = 0;
    root.IsValid = true;
    root.Text = NULL;
    root.Unescaped = NULL;
    root.FormattedLocation = 0;

    auto tree = TAllocate_Node();
    TSetValue_Node(tree, &root);

    bool valid = ParseSource(source, tree, 0, preserveMetadata);

    if (!valid) {
        root.IsValid = false;
        TSetValue_Node(tree, &root);
    }

    return tree;
}

void DeallocateAST(TreeNode* ast) {
    if (ast == NULL) return;

    auto vec = TAllData(ast); // vector of SourceNode

    Node n = Node{};
    while (VectorPop(vec, &n)) {
        StringDeallocate(n.ErrorMessage);
        StringDeallocate(n.Text);
        StringDeallocate(n.Unescaped);
    }

    VectorDeallocate(vec);
    TreeDeallocate(ast);
}


String* DescribeSourceNode(SourceNode *n) {
    return (n->Unescaped == NULL) ? n->Text : n->Unescaped;
}

// Recursively descend into the nodes.
// TODO: handle highlighting, cursor position
void Render_Rec(TreeNode* node, int indent, String* outp) {

    Node* n = TReadBody_Node(node);

    // Write the node contents
    StringAppend(outp, (n->Unescaped == NULL) ? n->Text : n->Unescaped);

    // Recursively write child nodes
    bool leadingWhite = false;
    auto child = TChild(node);
    while (child != NULL) {
        n = TReadBody_Node(child);

        // Handle re-indenting
        if (leadingWhite) {
            if (n->NodeType == NodeType::Whitespace) { // skip leading whitespace
                child = TSibling(child);
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
        if (n->NodeType == NodeType::Newline) {
            leadingWhite = true;
        }

        // write node text
        Render_Rec(child, indent + 1, outp);
        child = TSibling(child); // next node at this level
    }
}

String* RenderAstToSource(TreeNode* ast) {
    if (ast == NULL) return NULL;

    auto outp = StringEmpty();

    Render_Rec(ast, 0, outp);
    
    return outp;
}
