using System;
using EvieCompilerSystem.Runtime;

namespace EvieCompilerSystem.Compiler
{
    public static class Desugar{
        /// <summary>
        /// List of special function names that the compiler plays tricks with
        /// </summary>
        public static bool NeedsDesugaring(string funcName)
        {
            switch (funcName) {
                case "pick": return true; // switch / if-else chain

                default: return false;
            }
        }

        public static Node ProcessNode(string funcName, Scope parameterNames, Node node) {
            
            switch (funcName) {
                case "pick": return ConvertToPickList(funcName, node); // switch / if-else chain

                default: throw new Exception("Desugar for '" + funcName + "' is declared but not implemented");
            }
        }

        private static Node ConvertToPickList(string funcName, Node sourceNode)
        {
            if (sourceNode.IsLeaf) throw new Exception("Empty pick list");

            // Each `if` gets a return at the end.
            // Any immediate child that is not an `if` is an error

            sourceNode.Text = "()"; // make the contents into an anonymous block

            foreach (var nub in sourceNode.Children)
            {
                if (nub.Text != "if") throw new Exception("'pick' must contain a list of 'if' calls, and nothing else." +
                                                          "If you want something that runs in every-other-case, use\r\n" +
                                                          "if ( true ...");

                nub.Children.AddLast(MakeReturnNode());
            }

            // The entire lot then gets wrapped in a function def and immediately called
            var newFuncName = SugarName(funcName);

            var defineBlock = new Node(false, -1) { Text = "def" };
            defineBlock.Children.AddLast(new Node(true, -1) { Text = newFuncName }); // name, empty param list
            defineBlock.Children.AddLast(sourceNode); // modified pick block

            var wrapper = new Node(false, -1);
            wrapper.Children.AddFirst(defineBlock); // function def
            wrapper.Children.AddLast(new Node(false, -1){ Text = newFuncName  }); // call the function

            return wrapper;
        }

        private static int sugarCallCount = 0;
        private static string SugarName(string original) {
            sugarCallCount++;
            return "__" + original + "_s" + sugarCallCount;
        }

        private static Node MakeReturnNode()
        {
            return new Node(false, -1) { Text = "return" };
        }
    }
}