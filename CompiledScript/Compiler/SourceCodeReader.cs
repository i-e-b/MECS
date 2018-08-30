using System;
using System.Linq;
using System.Text;

namespace CompiledScript.Compiler
{
    class SourceCodeReader
    {
        public Node Read(string source)
        {
            var root = new Node(false) {Text = "root"};
            Read(source, root, 0);
            return root;
        }

        private void Read(string source, Node root, int position)
        {
            int i = position;
            int length = source.Length;
            var current = root;

            while (i < length)
            {
                i = Skip(source, i, new[] { ' ', '\t', '\n', ';', '\r' });

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

                    default:

                        {
                            var word = ReadWord(source, i);
                            if (word.Length != 0)
                            {
                                i += word.Length;

                                i = Skip(source, i, new[] { ' ', '\t', '\n', '\r' });
                                if (i >= length)
                                {
                                    //break;
                                    throw new Exception("Unexpected end of input");
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

        private bool IsNumeric(string word)
        {
            return char.IsDigit(word, 0);
        }

        public static int Skip(string exp, int position, char[] cars)
        {
            int i = position;
            int length = exp.Length;

            while (i < length)
            {
                var car = exp.ElementAt(i);
                var found = false;

                if (cars.Any(c => c == car))
                {
                    i++;
                    found = true;
                }
                if (!found)
                {
                    break;
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
