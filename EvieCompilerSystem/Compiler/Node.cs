using System.Collections.Generic;
using System.Text;

namespace EvieCompilerSystem.Compiler
{
    public class Node
    {
        private LinkedList<Node> _children { get; set; }
        public LinkedList<Node> Children
        {
            get
            {
                if (_children == null)
                {
                    _children = new LinkedList<Node>();
                }
                return _children;
            }
        }

        public Node Parent { get; set; }

        /// <summary>
        /// Text value of the node. For strings, this is after escape codes are processed
        /// </summary>
        public string Text { get; set; }

        public bool IsLeaf { get; set; }
        public NodeType NodeType { get; set; }

        /// <summary>
        /// Raw source text of the node, if applicable. For strings, this is before escape codes are processed.
        /// </summary>
        public string Unescaped { get; set; }

        /// <summary>
        /// Location in the source file that this node was found
        /// </summary>
        public int SourceLocation { get; set; }


        public Node(bool isLeaf, int sourceLoc)
        {
            NodeType = NodeType.Default;
            IsLeaf = isLeaf;
            SourceLocation = sourceLoc;
        }

        // ReSharper disable once UnusedMember.Global
        /// <summary>
        /// Render the node tree, specifically for diagnostics
        /// </summary>
        public string Show()
        {
            return Show(0);
        }

        /// <summary>
        /// Render the node tree with a specific starting indent, specifically for diagnostics
        /// </summary>
        public string Show(int i)
        {
            var builder = new StringBuilder();
            for (int j = 0; j < i; j++)
            {
               builder.Append("    ");
            }
            builder.Append(Text).Append("\r\n");

            if (_children == null) return builder.ToString();

            foreach (var node in _children)
            {
                builder.Append(node.Show(i + 1));
            }
            return builder.ToString();
        }

        
        /// <summary>
        /// Render the node tree with a specific starting indent, as close to original input as possible
        /// </summary>
        public void Reformat(int indent, StringBuilder builder, ref int caret) {
            int originalCaret = caret;
            ReformatRec(indent, builder, originalCaret, ref caret);
        }

        private void ReformatRec(int indent, StringBuilder builder, int origCaret, ref int caret)
        {
            //builder.Append("["+SourceLocation+"]");

            // Update caret
            if (SourceLocation < origCaret) caret = builder.Length + (origCaret - SourceLocation);

            // Write the node contents
            builder.Append(Unescaped ?? Text);

            if (_children == null) return;

            // Recursively write child nodes
            bool leadingWhite = false;
            foreach (var node in _children)
            {
                // Handle re-indenting
                if (leadingWhite) {
                    if (node.NodeType == NodeType.Whitespace) continue;
                    leadingWhite = false;
                    if (node.NodeType == NodeType.Delimiter) builder.Append(' ', (indent - 1) * 4);
                    else builder.Append(' ', indent * 4);
                }
                if (node.NodeType == NodeType.Newline) leadingWhite = true;
                
                // otherwise write node text
                node.ReformatRec(indent + 1, builder, origCaret, ref caret);
            }
        }

        public static Node RootNode()
        {
            return new Node(false, 0) {Text = "", NodeType = NodeType.Root};
        }

        public static Node OpenCall(Node parent, int loc)
        {
            return new Node(false, loc)
            {
                Text = "()",
                Unescaped = "(",
                Parent = parent
            };
        }

        public static Node CloseCall(int loc)
        {
            return new Node(true, loc) {Text = ")", NodeType = NodeType.Delimiter};
        }

        public static Node Whitespace(string s, int loc)
        {
            return new Node(true, loc) {Text = s, NodeType = NodeType.Whitespace};
        }

        public static Node Delimiter(char c, int loc)
        {
            return new Node(true, loc)
            {
                Text = c.ToString(),
                NodeType = NodeType.Delimiter
            };
        }
    }
}
