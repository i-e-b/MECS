using System;
using System.Linq;
using System.Text;

namespace EvieCompilerSystem.Compiler
{
    public class SourceCodeTokeniser
    {
        /// <summary>
        /// Read an source string and output node tree
        /// </summary>
        /// <param name="source">Input text</param>
        /// <param name="preserveMetadata">if true, comments and spacing will be included</param>
        public Node Read(string source, bool preserveMetadata)
        {
            var root = Node.RootNode();
            var valid = Read(source, root, 0, preserveMetadata);
            if (!valid) {
                root.IsValid = false;
            }
            return root;
        }

        private bool Read(string source, Node root, int position, bool preserveMetadata)
        {
            int i = position;
            int length = source.Length;
            var current = root;

            while (i < length)
            {
                i = SkipWhitespace(source, i, preserveMetadata, current);

                if (i >= length)
                {
                    break;
                }

                var car = source.ElementAt(i);

                Node tmp;
                Node parent;
                switch (car)
                {
                    case '(': // start of call
                        parent = current;
                        current = Node.OpenCall(parent, i);
                        parent.Children.AddLast(current);
                        break;
                    case ')': // end of call
                        
                        if (preserveMetadata) {
                            tmp = Node.CloseCall(i);
                            current.Children.AddLast(tmp);
                        }

                        if (current.Parent != null)
                        {
                            current = current.Parent;
                        }

                        break;
                    case ',': // optional separator
                        // Ignore.
                        if (preserveMetadata) {
                            tmp = Node.Whitespace(",", i);
                            current.Children.AddLast(tmp);
                        }
                        break;

                    case '"': // start of strings
                    case '\'':
                        {
                            if (preserveMetadata) current.Children.AddLast(Node.Delimiter(car, i));

                            i++;
                            var old_i = i;
                            var words = ReadString(source, ref i, car, out var endedCorrectly);

                            tmp = new Node(true, old_i) {
                                Text = words,
                                NodeType = NodeType.StringLiteral
                            };
                            if (preserveMetadata) {
                                tmp.Unescaped = source.Substring(old_i, i - old_i);
                            }
                            current.Children.AddLast(tmp);
                            
                            if (preserveMetadata && endedCorrectly) current.Children.AddLast(Node.Delimiter(car, i));
                            break;
                        }

                    case '/': //maybe a comment?
                        {
                            if (!TryCaptureComment(source, ref i, preserveMetadata, current))
                            {
                                goto default;
                            }
                            break;
                        }

                    default:

                        {
                            var word = ReadWord(source, i);
                            if (word.Length != 0)
                            {
                                var startLoc = i;
                                i += word.Length;

                                var wsNode = Node.Whitespace(null, i);
                                i = SkipWhitespace(source, i, preserveMetadata, wsNode);
                                if (i >= length)
                                {
                                    // Unexpected end of input
                                    // To help formatting and diagnosis, write the last bits.
                                    current.Children.AddLast(new Node(false, startLoc)
                                    {
                                        Text = word,
                                        NodeType = NodeType.Atom
                                    });
                                    if (preserveMetadata){
                                        foreach (var ws in wsNode.Children) { current.Children.AddLast(ws); }
                                    }
                                    return false;
                                }
                                car = source.ElementAt(i);
                                if (car == '(')
                                {
                                    if (IsNumeric(word))
                                        throw new Exception("Parser error: '" + word + "' used like a function name, but looks like a number");
                                    parent = current;
                                    current = new Node(false, startLoc)
                                    {
                                        Text = word,
                                        Parent = parent,
                                        NodeType = NodeType.Atom
                                    };

                                    if (preserveMetadata){
                                        foreach (var ws in wsNode.Children) { current.Children.AddLast(ws); }
                                        current.Children.AddLast(Node.OpenCall(parent, i));
                                    }
                                    parent.Children.AddLast(current);
                                }
                                else
                                {
                                    i--;
                                    tmp = new Node(true, startLoc) {
                                        Text = word,
                                        NodeType = NodeType.Atom
                                    };
                                    if (IsNumeric(word)) tmp.NodeType = NodeType.Numeric;

                                    current.Children.AddLast(tmp);
                                    if (preserveMetadata) {
                                        foreach (var ws in wsNode.Children) { current.Children.AddLast(ws); }
                                    }
                                }
                            }
                        }

                        break;
                }

                i++;
            }
            return true;
        }

        private bool TryCaptureComment(string source, ref int i, bool preserveMetadata, Node mdParent)
        {
            int end;
            if (i >= source.Length) return false;
            var k = source[i + 1];
            Node tmp;
            switch (k)
            {
                case '/': // line comment
                    end = NextNewline(source, i);
                    if (preserveMetadata) {
                        tmp = new Node(true, i) {
                            Text = source.Substring(i, end - i),
                            NodeType = NodeType.Comment
                        };
                        mdParent.Children.AddLast(tmp);
                    }
                    i = end - 1;
                    return true;
                case '*': // block comment
                    end = source.IndexOf("*/", i+2, StringComparison.Ordinal);
                    if (preserveMetadata) {
                        tmp = new Node(true, i) {
                            Text = source.Substring(i, end - i + 2),
                            NodeType = NodeType.Comment
                        };
                        mdParent.Children.AddLast(tmp);
                    }
                    i = end + 1;
                    return true;
                default: return false;
            }
        }

        private static readonly char[] newLines = {'\r','\n' };
        private int NextNewline(string source, int i)
        {
            var f = source.IndexOfAny(newLines, i);
            if (f < 0) return source.Length;
            return f;
        }

        private bool IsNumeric(string word)
        {
            return double.TryParse(word.Replace("_",""), out _);
        }

        public int SkipWhitespace(string exp, int position, bool preserveMetadata, Node mdParent)
        {
            int lastcap = position;
            int i = position;
            int length = exp.Length;

            bool capWS = false;
            bool capNL = false;

            while (i < length)
            {
                var c = exp.ElementAt(i);
                var found = false;

                switch (c)
                {
                    case ' ':
                    case '\t':
                        found = true;
                        if (preserveMetadata)
                        {
                            if (capNL)
                            { // switch from newlines to regular space
                              // output NL so far
                                var tmp = new Node(true, lastcap) {
                                    Text = exp.Substring(lastcap, i - lastcap),
                                    NodeType = NodeType.Newline
                                };
                                mdParent.Children.AddLast(tmp);
                                lastcap = i ;
                            }
                        }
                        capNL = false;
                        capWS = true;
                        i++;
                        break;

                    case '\r':
                    case '\n':
                        found = true;
                        if (preserveMetadata)
                        {
                            if (capWS)
                            { // switch from regular space to newlines
                                // output WS so far
                                var tmp = new Node(true, lastcap) {
                                    Text = exp.Substring(lastcap, i - lastcap),
                                    NodeType = NodeType.Whitespace
                                };
                                mdParent.Children.AddLast(tmp);
                                lastcap = i;
                            }
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

            
            if (preserveMetadata && i != lastcap)
            {
                if (capNL)
                {
                    var tmp = new Node(true, lastcap) {
                        Text = exp.Substring(lastcap, i - lastcap),
                        NodeType = NodeType.Newline
                    };
                    mdParent.Children.AddLast(tmp);
                }
                if (capWS)
                {
                    var tmp = new Node(true, lastcap) {
                        Text = exp.Substring(lastcap, i - lastcap),
                        NodeType = NodeType.Whitespace
                    };
                    mdParent.Children.AddLast(tmp);
                }
            }

            return i;
        }

        public static string ReadString(string exp, ref int position, char end, out bool endedCorrectly)
        {
            int i = position;
            int length = exp.Length;
            var sb = new StringBuilder();
            endedCorrectly = false;

            while (i < length)
            {
                var car = exp.ElementAt(i);

                if (car == '\\')
                {
                    var nb = 0;
                    i++;
                    while (i < length)
                    {
                        car = exp.ElementAt(i);
                        if (car == '\\')
                        {
                            if (nb % 2 == 0)
                            {
                                sb.Append(car);
                            }
                        }
                        else if (car == end && nb % 2 == 1)
                        {
                            i--;
                            break;
                        }
                        else
                        {
                            sb.Append(Unescape(car));
                            break;
                        }

                        nb++;
                        i++;
                    }
                }
                else if (car == end)
                {
                    endedCorrectly = true;
                    break;
                }
                else
                {
                    sb.Append(car);
                }

                i++;
            }

            position = i;
            return sb.ToString();
        }

        private static char Unescape(char car)
        {
            switch (car) {
                case '\\': return '\\';
                case 't' : return '\t';
                case 'r' : return '\r';
                case 'n' : return '\n';
                case 'e' : return '\x1B'; // ASCII escape
                case '0' : return '\0';
                
                // TODO: unicode escapes

                default: return car;
            }
        }

        public static string ReadWord(string expression, int position)
        {
            int i = position;
            int length = expression.Length;
            var sb = new StringBuilder();

            while (i < length)
            {
                var c = expression[i];

                if (c == ' ' || c == '\n' || c == '\t' || c == ')'
                        || c == '(' || c == ',' || c == '\r')
                {
                    break;
                }

                sb.Append(c);

                i++;
            }
            return sb.ToString();
        }
    }
}
