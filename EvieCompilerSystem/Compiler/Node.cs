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
        public string Text { get; set; }
        public bool IsLeaf { get; set; }
        public NodeType NodeType { get; set; }
        public string Unescaped { get; set; }

        public Node(bool isLeaf)
        {
            NodeType = NodeType.Default;
            IsLeaf = isLeaf;
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
        public virtual string Show(int i)
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
        public virtual void Reformat(int i, StringBuilder builder)
        {
            builder.Append(Unescaped ?? Text);

            if (_children == null) return;// builder.ToString().Replace("\n", "\r\n");

            bool leadingWhite = false;
            foreach (var node in _children)
            {
                // Handle re-indenting
                if (leadingWhite) {
                    if (node.NodeType == NodeType.Whitespace) continue;
                    leadingWhite = false;
                    if (node.NodeType == NodeType.Delimiter) builder.Append(' ', (i - 1) * 4);
                    else builder.Append(' ', i * 4);
                }
                if (node.NodeType == NodeType.Newline) leadingWhite = true;
                
                // otherwise write node text
                node.Reformat(i + 1, builder);
            }
        }
    }
}
