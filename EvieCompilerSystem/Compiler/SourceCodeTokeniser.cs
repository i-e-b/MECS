using System;
using System.Linq;
using System.Text;

namespace EvieCompilerSystem.Compiler
{
    class SourceCodeTokeniser
    {
        /// <summary>
        /// Read an source string and output node tree
        /// </summary>
        /// <param name="source">Input text</param>
        /// <param name="preserveMetadata">if true, comments and spacing will be included</param>
        public Node Read(string source, bool preserveMetadata)
        {
            var root = new Node(false) {Text = "root"};
            Read(source, root, 0, preserveMetadata);
            return root;
        }

        private void Read(string source, Node root, int position, bool preserveMetadata)
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

                Node parent;
                Node tmp;
                switch (car)
                {
                    case '(': // start of call
                        parent = current;
                        current = new Node(false)
                        {
                            Text = "()",
                            Parent = parent
                        };
                        parent.Children.AddLast(current);
                        break;
                    case ')': // end of call
                        if (current.Parent != null)
                        {
                            current = current.Parent;
                        }

                        break;
                    case ',': // optional separator
                        // Ignore.
                        if (preserveMetadata) {
                            tmp = new Node(true) {Text = ",", NodeType = NodeType.Whitespace};
                            current.Children.AddLast(tmp);
                        }
                        break;

                    case '"': // start of strings
                    case '\'':
                        {
                            i++;
                            var words = ReadString(source, ref i, car);
                            tmp = new Node(true) {Text = words, NodeType = NodeType.StringLiteral};
                            current.Children.AddLast(tmp);
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
                                i += word.Length;

                                i = SkipWhitespace(source, i, preserveMetadata, current);
                                if (i >= length)
                                {
                                    throw new Exception("Unexpected end of input. Check syntax.");
                                }
                                car = source.ElementAt(i);
                                if (car == '(')
                                {
                                    if (IsNumeric(word)) throw new Exception("Parser error: '" + word + "' looks like a function name, but starts with a number");
                                    parent = current;
                                    current = new Node(false)
                                    {
                                        Text = word,
                                        Parent = parent,
                                        NodeType = NodeType.Atom
                                    };

                                    parent.Children.AddLast(current);
                                }
                                else
                                {
                                    i--;
                                    tmp = new Node(true) { Text = word, NodeType = NodeType.Atom};
                                    if (IsNumeric(word)) tmp.NodeType = NodeType.Numeric;
                                    current.Children.AddLast(tmp);
                                }
                            }
                        }

                        break;
                }

                i++;
            }
        }

        private static readonly char[] newLines = {'\r','\n' };
        private bool TryCaptureComment(string source, ref int i, bool preserveMetadata, Node mdParent)
        {
            int end;
            if (i >= source.Length) return false;
            var k = source[i + 1];
            Node tmp;
            switch (k)
            {
                case '/': // line comment
                    end = source.IndexOfAny(newLines, i);
                    if (preserveMetadata) {
                        tmp = new Node(true) { Text = source.Substring(i, (end - i)), NodeType = NodeType.Comment};
                        mdParent.Children.AddLast(tmp);
                    }
                    i = end;
                    return true;
                case '*': // block comment
                    end = source.IndexOf("*/", i+2, StringComparison.Ordinal);
                    if (preserveMetadata) {
                        tmp = new Node(true) { Text = source.Substring(i, (end - i) + 2), NodeType = NodeType.Comment };
                        mdParent.Children.AddLast(tmp);
                    }
                    i = end + 1;
                    return true;
                default: return false;
            }
        }

        private bool IsNumeric(string word)
        {
            return double.TryParse(word.Replace("_",""), out _);
        }

        public static int SkipWhitespace(string exp, int position, bool preserveMetadata, Node mdParent)
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
                                var tmp = new Node(true) { Text = exp.Substring(lastcap, i - lastcap), NodeType = NodeType.Newline };
                                mdParent.Children.AddLast(tmp);
                                lastcap = i + 1;
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
                                var tmp = new Node(true) { Text = exp.Substring(lastcap, i - lastcap), NodeType = NodeType.Whitespace };
                                mdParent.Children.AddLast(tmp);
                                lastcap = i + 1;
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

            
            if (preserveMetadata)
            {
                if (capNL)
                {
                    var tmp = new Node(true) { Text = exp.Substring(lastcap, i - lastcap), NodeType = NodeType.Newline };
                    mdParent.Children.AddLast(tmp);
                }
                if (capWS)
                {
                    var tmp = new Node(true) { Text = exp.Substring(lastcap, i - lastcap), NodeType = NodeType.Whitespace };
                    mdParent.Children.AddLast(tmp);
                }
            }

            return i;
        }

        public static string ReadString(string exp, ref int position, char end)
        {
            int i = position;
            int length = exp.Length;
            var sb = new StringBuilder();

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
