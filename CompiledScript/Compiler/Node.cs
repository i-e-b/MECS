using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace CompiledScript.Compiler
{
    class Node
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

        public Node(bool isLeaf)
        {
            IsLeaf = isLeaf;
        }

        public string Show()
        {
            return Show(0);
        }

        public virtual string Show(int i)
        {
            StringBuilder builder = new StringBuilder();
            for (int j = 0; j < i; j++)
            {
                builder.Append("    ");
            }
            builder.Append(Text).Append('\n');

            if (_children != null)
            {
                foreach (var node in _children)
                {
                    builder.Append(node.Show(i + 1));
                }
            }
            return builder.ToString();
        }
    }
}
