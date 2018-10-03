using System;
using EvieCompilerSystem.InputOutput;

namespace EvieCompilerSystem.Compiler
{
    /// <summary>
    /// Patterns in the code that can be reduced or compounded for better performance
    /// </summary>
    public static class Optimisations {
        
        /// <summary>
        /// Is a node an add or subtract that can be encoded as an increment
        /// </summary>
        public static bool IsSmallIncrement(Node node, out sbyte incr, out string varName)
        {
            incr = 0;
            varName = null;

            // These cases are handled:
            // 1)  set(x +(x n))
            // 2)  set(x +(get(x) n))
            // 3)  set(x +(n x))
            // 4)  set(x +(n get(x))
            // 5)  set(x -(x n))
            // 6)  set(x -(get(x) n)
            //
            // Where `x` is any reference name, and n is a literal integer between -100 and 100

            if (node.Text != "set") return false;
            if (node.Children.Count != 2) return false;
            var target = node.Children.First.Value.Text;
            var operation = node.Children.Last.Value;
            if (operation.Children.Count != 2) return false;
            
            if (operation.Text != "+" && operation.Text != "-") return false;

            if (operation.Text == "-") { // the operands can only be one way around
                if (!IsGet(operation.Children.First.Value, target)) return false;
                var incrNode = operation.Children.Last.Value;
                if (incrNode.NodeType != NodeType.Numeric) return false;
                if (! int.TryParse(incrNode.Text, out var incrV)) return false;
                if (incrV < -100 || incrV > 100 || incrV == 0) return false;
                unchecked{
                    incr = (sbyte)(-incrV); // negate to account for subtraction
                    varName = target;
                }
                return true;
            }

            if (operation.Text == "+") { // +- operands can be either way
                Node incrNode;
                if (IsGet(operation.Children.First.Value, target)) {
                    incrNode = operation.Children.Last.Value;
                } else if (IsGet(operation.Children.Last.Value, target)) {
                    incrNode = operation.Children.First.Value;
                } else return false;
                
                if (incrNode.NodeType != NodeType.Numeric) return false;
                if (! int.TryParse(incrNode.Text, out var incrV)) return false;
                if (incrV < -100 || incrV > 100 || incrV == 0) return false;
                unchecked{
                    incr = (sbyte)incrV;
                    varName = target;
                }
                return true;
            }

            return false;
        }

        private static bool IsGet(Node node, string target)
        {
            if (node.NodeType == NodeType.Atom && node.Text == target) return true;
            if (node.Text == "get" && node.Children.Count == 1 && node.Children.First.Value.Text == target) return true;

            return false;
        }

        private static bool IsGet(Node node)
        {
            if (node.NodeType == NodeType.Atom && !string.IsNullOrWhiteSpace(node.Text)) return true;
            if (node.Text == "get" && node.Children.Count == 1 && !string.IsNullOrWhiteSpace(node.Children.First.Value.Text)) return true;

            return false;
        }

        /// <summary>
        /// Is this a single comparison between two values?
        /// </summary>
        public static bool IsSimpleComparion(Node condition, uint opCodeCount)
        {
            if (opCodeCount >= ushort.MaxValue) return false; // can't be encoded

            var target = condition.Children.First.Value;
            if (target.Children.Count != 2) return false;

            var left = target.Children.First.Value;
            var right = target.Children.Last.Value;

            var leftIsSimple = IsGet(left) || left.IsSimpleType();
            var rightIsSimple = IsGet(right) || right.IsSimpleType();

            if ( ! leftIsSimple || ! rightIsSimple) return false;

            switch (target.Text) {
                case "=":
                case "equals":
                    return true;

                case "not-equals":
                case "<>":
                    return true;

                case "<":
                case ">":
                    return true;

                default:
                    return false;
            }
        }

        public static Node ReadSimpleComparison(Node condition, out CmpOp cmpOp, out ushort argCount)
        {
            var target = condition.Children.First.Value;
            argCount = (ushort) target.Children.Count;
            
            switch (target.Text) {
                case "=":
                case "equals":
                    cmpOp = CmpOp.Equal;
                    break;

                case "not-equals":
                case "<>":
                    cmpOp = CmpOp.NotEqual;
                    break;

                case "<":
                    cmpOp = CmpOp.Less;
                    break;
                case ">":
                    cmpOp = CmpOp.Greater;
                    break;

                default: throw new Exception("Invalid comparator. Investigate optimisation pre-check.");
            }
            
            return Node.Repack(target.Children);
        }
    }
}