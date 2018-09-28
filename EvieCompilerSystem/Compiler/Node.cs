using System.Collections.Generic;
using System.Text;

namespace EvieCompilerSystem.Compiler
{
    public class Node
    {
        private LinkedList<Node> _children { get; set; }

        /// <summary>
        /// Return collection of child nodes, creating the collection if needed
        /// </summary>
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

        /// <summary>
        /// Parent node (not always populated)
        /// </summary>
        public Node Parent;

        /// <summary>
        /// Text value of the node. For strings, this is after escape codes are processed
        /// </summary>
        public string Text;

        /// <summary>
        /// If false, it is a container of other nodes
        /// </summary>
        public bool IsLeaf;

        /// <summary>
        /// Semantic class of the node
        /// </summary>
        public NodeType NodeType;

        /// <summary>
        /// Raw source text of the node, if applicable. For strings, this is before escape codes are processed.
        /// </summary>
        public string Unescaped;

        /// <summary>
        /// Location in the source file that this node was found
        /// </summary>
        public int SourceLocation;

        /// <summary>
        /// If false, the parse tree was not successful
        /// </summary>
        public bool IsValid;

        /// <summary>
        /// Start location in the source file from last time the node was formatted
        /// </summary>
        public int FormattedLocation;

        /// <summary>
        /// Length of contents (in characters) from the last time the node was formatted
        /// </summary>
        public int FormattedLength;

        public Node(bool isLeaf, int sourceLoc, Node parent)
        {
            NodeType = NodeType.Default;
            IsLeaf = isLeaf;
            IsValid = true;
            SourceLocation = sourceLoc;
            Parent = parent;
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

        public override string ToString()
        {
            var ccount = _children == null ? "0" : _children.Count.ToString();
            return $"[{SourceLocation}, {NodeType}]{Text} ({ccount} children)";
        }

        /// <summary>
        /// Render the node tree with a specific starting indent, as close to original input as possible
        /// Returns the node that contains the Caret
        /// </summary>
        public Node Reformat(int indent, StringBuilder builder, ref int caret) {
            int originalCaret = caret;
            Node focus = null;
            ReformatRec(indent, builder, originalCaret, ref caret, ref focus);
            return focus;
        }

        private void ReformatRec(int indent, StringBuilder builder, int origCaret, ref int caret, ref Node focus)
        {
            // Update caret
            if (SourceLocation < origCaret){
                focus = Parent;
                caret = builder.Length + (origCaret - SourceLocation);
            }
            FormattedLocation = builder.Length;

            // Write the node contents
            builder.Append(Unescaped ?? Text);

            // Recursively write child nodes
            bool leadingWhite = false;
            if (_children != null) foreach (var node in _children)
            {
                // Handle re-indenting
                if (leadingWhite) {
                    if (node.NodeType == NodeType.Whitespace) continue;
                    leadingWhite = false;
                    if (node.NodeType == NodeType.Paren) builder.Append(' ', (indent - 1) * 4);
                    else builder.Append(' ', indent * 4);
                }
                if (node.NodeType == NodeType.Newline) leadingWhite = true;
                
                // write node text
                node.ReformatRec(indent + 1, builder, origCaret, ref caret, ref focus);
            }

            //if (leftEdge && caret >= FormattedLocation && caret <= builder.Length) focus = this; // do this as we come out of recursion to get widest scope

            FormattedLength = builder.Length - FormattedLocation;
        }

        public static Node RootNode()
        {
            return new Node(false, 0, null) {Text = "", NodeType = NodeType.Root};
        }

        public static Node OpenCall(Node parent, int loc)
        {
            return new Node(false, loc, parent)
            {
                Text = "()",
                Unescaped = "(",
                Parent = parent,
                NodeType = NodeType.Paren
            };
        }

        public static Node CloseCall(Node parent, int loc)
        {
            return new Node(true, loc, parent) {Text = ")", NodeType = NodeType.Paren};
        }

        public static Node Whitespace(Node parent, string s, int loc)
        {
            return new Node(true, loc, parent) {Text = s, NodeType = NodeType.Whitespace};
        }

        public static Node Delimiter(Node parent, char c, int loc)
        {
            return new Node(true, loc, parent)
            {
                Text = c.ToString(),
                NodeType = NodeType.Delimiter
            };
        }
    }
}
