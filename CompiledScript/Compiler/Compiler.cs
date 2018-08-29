using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using CompiledScript.Runner;
using CompiledScript.Utils;

namespace CompiledScript.Compiler
{
    class Compiler
    {
        public static string CompileRoot(Node root, bool debug)
        {
            var sb = new StringBuilder();

            if (debug)
            {
                sb.Append("/*\r\n");
                sb.Append("CompiledScript - Intermediate Language\r\n");
                sb.Append("Date    : " + DateTime.Now + "\r\n");
                sb.Append("Version : 1.1\r\n");
                sb.Append("*/\r\n\r\n");
            }

            var parameterNames = new Scope(); // renaming for local parameters
            var includedFiles = new HashSet<string>();

            sb.Append(Compile(root, 0, debug, parameterNames, includedFiles));

            return sb.ToString();
        }

        /// <summary>
        /// Function/Program compiler. This is called recursively when subroutines are found
        /// </summary>
        public static ProgramFragment Compile(Node root, int indent, bool debug, Scope parameterNames, HashSet<string> includedFiles)
        {
		    var sb = new StringBuilder();
            var result = new ProgramFragment { ReturnsValues = false };

            if (root.IsLeaf)
            {
                var valueName = root.Text;
                if (parameterNames.CanResolve(valueName)) valueName = parameterNames.Resolve(valueName);


                if (debug)
                {
                    sb.Append(Fill(indent - 1, ' '));
                    sb.Append("// Value : \"");
                    sb.Append(EscapeEncodedTextForDebug(root));
                    sb.Append("\"\r\n");
                    if (parameterNames.CanResolve(root.Text))
                    {
                        sb.Append("// Parameter reference redefined as '" + valueName + "'\r\n");
                    }
                    sb.Append(Fill(indent - 1, ' '));
                }


			    sb.Append("v");
                sb.Append(valueName);
                sb.Append("\r\n");
		    }
            else
            {
                Node rootContainer = root;
                foreach (Node node in rootContainer.Children.ToList())
                {
				    node.Text = StringEncoding.Encode(node.Text);
				    if (!node.IsLeaf)
                    {
					    var container = node;
					    if (IsMemoryFunction(node) )
					    {
					        CompileMemoryFunction(indent, debug, node, container, sb, parameterNames);
					    }
                        else if (IsInclude(node))
                        {
                            CompileExternalFile(indent, debug, node, sb, parameterNames, includedFiles);
                        }
                        else if (IsFlowControl(node))
					    {
					        result.ReturnsValues |= CompileConditionOrLoop(indent, debug, container, node, sb, parameterNames);
					    }
                        else if (IsFunctionDefinition(node))
                        {
                            CompileFunctionDefinition(indent, debug, node, sb, parameterNames);
                        }
                        else
					    {
					        result.ReturnsValues |= CompileFunctionCall(indent, debug, sb, node, parameterNames);
					    }
				    }
                    else
                    {
                        sb.Append(Compile(node, indent + 1, debug, parameterNames, includedFiles));
				    }
			    }
		    }

            result.ByteCode = sb.ToString();
		    return result;
	    }

        private static bool IsInclude(Node node)
        {
            switch (node.Text)
            {
                case "import":
                    return true;

                default:
                    return false;
            }
        }

        private static bool IsFunctionDefinition(Node node)
        {
            switch (node.Text)
            {
                case "def":
                    return true;

                default:
                    return false;
            }
        }

        private static bool IsFlowControl(Node node)
        {
            switch (node.Text)
            {
                case "if":
                case "while":
                    return true;

                default:
                    return false;
            }
        }

        private static bool IsMemoryFunction(Node node)
        {
            switch (node.Text)
            {
                case "get":
                case "set":
                case "isset":
                case "unset":
                    return true;

                default:
                    return false;
            }
        }

        private static void CompileMemoryFunction(int level, bool debug, Node node, Node container, StringBuilder sb, Scope parameterNames)
        {
            switch (node.Text)
            {
                case "set":
                    if (container.Children.Count != 2)
                    {
                        throw new Exception(node.Text + " required 2 parameters");
                    }

                    break;
                default:
                    if (container.Children.Count != 1)
                    {
                        throw new Exception(node.Text + " required 1 parameter");
                    }

                    break;
            }

            int nb = node.Text == "set" ? 1 : 0;
            var child = new Node(false);
            child.Text = container.Children.ElementAt(0).Text;
            for (int i = nb; i >= 0; i--)
            {
                child.Children.AddLast(container.Children.ElementAt(i));
            }

            sb.Append(Compile(child, level + 1, debug, parameterNames, null));

            if (debug)
            {
                sb.Append(Fill(level, ' '));
                sb.Append("// Memory function : " + node.Text);
                sb.Append("\r\n");
                sb.Append(Fill(level, ' '));
            }

            sb.Append("m" + node.Text[0]);
            sb.Append("\r\n");
        }

        
        private static void CompileExternalFile(int level, bool debug, Node node, StringBuilder sb, Scope parameterNames, HashSet<string> includedFiles)
        {
            //     1) Check against import list. If already done, warn and skip.
            //     2) Read file. Fail = terminate with error
            //     3) Compile to opcodes
            //     4) Inject opcodes in `sb`

            if (includedFiles == null) throw new Exception("Files can only be included at the root level");

            var targetFile = node.Children.First.Value.Text;

            if (includedFiles.Contains(targetFile)) {
                sb.Append(Fill(level, ' '));
                sb.Append("// Ignored import: " + targetFile);
                return;
            }

            // TODO: replace with a file resolver?
            if (!File.Exists(targetFile)) throw new Exception("Import failed. Can't read file '" + targetFile + "'");

            var text = File.ReadAllText(targetFile);
            includedFiles.Add(targetFile);
            
            var reader = new SourceCodeReader();
            var parsed = reader.Read(FileReader.ReadContent(text, true));
            var programFragment = Compile(parsed, level, debug, parameterNames, includedFiles);


            if (debug)
            {
                sb.Append(Fill(level, ' '));
                sb.Append("// File import: \"" + targetFile);
                sb.Append("\r\n");
                sb.Append(Fill(level, ' '));
            }

            sb.Append(programFragment.ByteCode);

            if (debug)
            {
                sb.Append(Fill(level, ' '));
                sb.Append("// <-- End of file import: \"" + targetFile);
                sb.Append("\r\n");
                sb.Append(Fill(level, ' '));
            }
        }

        private static bool CompileConditionOrLoop(int level, bool debug, Node container, Node node, StringBuilder sb, Scope parameterNames)
        {
            bool returns = false;
            if (container.Children.Count < 1)
            {
                throw new ArgumentException(node.Text + " required parameter(s)");
            }

            bool isLoop = node.Text == "while";
            Node condition = new Node(false);
            condition.Children.AddLast(container.Children.ElementAt(0));
            condition.Text = "()";

            var compilerOutput = Compile(condition, level + 1, debug, parameterNames, null);
            returns |= compilerOutput.ReturnsValues;
            string compiledCondition = compilerOutput.ByteCode;

            if (debug)
            {
                sb.Append(Fill(level, ' '));
                sb.Append("// Condition for : " + node.Text);
                sb.Append("\r\n");
            }

            sb.Append(compiledCondition);

            Node body = new Node(false);
            for (int i = 1; i < container.Children.Count; i++)
            {
                body.Children.AddLast(container.Children.ElementAt(i));
            }

            body.Text = "()";
            var compiledBody = Compile(body, level + 1, debug, parameterNames, null);
            returns |= compiledBody.ReturnsValues;

            string clean = FileReader.RemoveInlineComment(compiledBody.ByteCode);


            List<string> elementsBody = new List<string>();
            foreach (string s in clean.Replace("\t", " ").Replace("\r", " ").Replace("\n", " ").Split(' '))
            {
                elementsBody.Add(s);
            }

            int nbElementBody = 0;
            foreach (string s in elementsBody)
            {
                if (s.Length != 0)
                {
                    nbElementBody++;
                }
            }

            int nbElementBack = nbElementBody;
            if (isLoop)
            {
                clean = FileReader.RemoveInlineComment(compiledCondition);

                elementsBody = new List<string>();
                foreach (string s in clean.Replace("\t", " ").Replace("\r", " ").Replace("\n", " ").Split(' '))
                {
                    elementsBody.Add(s);
                }

                foreach (string s in elementsBody)
                {
                    if (s.Length != 0)
                    {
                        nbElementBack++;
                    }
                }
                
                nbElementBack += 2; // CMP + len body
                nbElementBody += 2;
            }

            if (debug)
            {
                sb.Append("\r\n");
                sb.Append(Fill(level, ' '));
                sb.Append("// Compare condition for : " + node.Text);
                sb.Append("\r\n");
                sb.Append(Fill(level, ' '));
            }

            sb.Append("ccmp");
            sb.Append(" ").Append(nbElementBody);

            if (debug)
            {
                sb.Append(" // If false, skip " + nbElementBody + " element(s)");
            }

            //builder.Append(" ");
            sb.Append("\r\n");

            sb.Append(compiledBody.ByteCode);

            if (isLoop)
            {
                if (debug)
                {
                    sb.Append(Fill(level, ' '));
                    sb.Append("// End : " + node.Text);
                    sb.Append("\r\n");
                    sb.Append(Fill(level, ' '));
                }

                sb.Append("cjmp");
                sb.Append(" ").Append(nbElementBack);
                //builder.Append(" ");
                sb.Append("\r\n");
                if (debug)
                {
                    sb.Append("\r\n");
                }
            }
            else
            {
                if (debug)
                {
                    sb.Append(Fill(level, ' '));
                    sb.Append("// End : " + node.Text);
                    sb.Append("\r\n");
                    sb.Append("\r\n");
                }
            }
            return returns;
        }

        /// <summary>
        /// Compile a function call. Returns true if it's a call to return a value
        /// </summary>
        private static bool CompileFunctionCall(int level, bool debug, StringBuilder sb, Node node, Scope parameterNames)
        {
            sb.Append(Compile(node, level + 1, debug, parameterNames, null));

            if (debug)
            {
                sb.Append(Fill(level, ' '));
                sb.Append("// Function : \"" + StringEncoding.Decode(node.Text));
                sb.Append("\" with " + node.Children.Count);
                sb.Append(" parameter(s)");
                sb.Append("\r\n");
                sb.Append(Fill(level, ' '));
            }

            sb.Append("f");
            sb.Append(node.Text).Append(" ");
            sb.Append(node.Children.Count); // parameter count
            sb.Append("\r\n");

            return (node.Text == "return") && (node.Children.Count > 0);
        }

        /// <summary>
        /// Compile a custom function definition
        /// </summary>
        private static void CompileFunctionDefinition(int level, bool debug, Node node, StringBuilder sb, Scope parameterNames)
        {
            // 1) Compile the func to a temporary string
            // 2) Inject a new 'def' op-code, that names the function and does an unconditional jump over it.
            // 3) Inject the compiled func
            // 4) Inject a new 'return' op-code

            if (node.Children.Count != 2) throw new Exception("Function definition must have 3 parts: the name, the parameter list, and the definition.\r\n" +
                                                              "Call like `def (   myFunc ( param1 param2 ) ( ... statements ... )   )`");

            var bodyNode = node.Children.Last.Value;
            var definitionNode = node.Children.First.Value;

            if (definitionNode.Children.Any(c=>c.IsLeaf == false)) throw new Exception("Function parameters must be simple names.\r\n" +
                                                                                       "`def ( myFunc (  param1  ) ( ... ) )` is OK,\r\n" +
                                                                                       "`def ( myFunc ( (param1) ) ( ... ) )` is not OK");
            if (bodyNode.Text != "()") throw new Exception("Bare functions not supported. Wrap you function body in (parenthesis)");

            var functionName = definitionNode.Text;
            var argCount = definitionNode.Children.Count;

            ParameterPositions(parameterNames, definitionNode.Children.Select(c => c.Text));

            var subroutine = Compile(bodyNode, level, debug, parameterNames, null);
            var tokenCount = subroutine.ByteCode.Split(new[] { '\t', '\n', '\r', ' ' }, StringSplitOptions.RemoveEmptyEntries).Length;

            if (debug)
            {
                sb.Append(Fill(level, ' '));
                sb.Append("// Function definition : \"" + functionName);
                sb.Append("\" with " + argCount);
                sb.Append(" parameter(s)");
                sb.Append("\r\n");
                sb.Append(Fill(level, ' '));
            }
            
            sb.Append("d");
            sb.Append(functionName);
            sb.Append(" ");
            sb.Append(argCount);
            sb.Append(" ");
            sb.Append(tokenCount); // relative jump to skip the function opcodes
            sb.Append("\r\n");

            // Write the actual function
            sb.Append(subroutine);

            if (subroutine.ReturnsValues)
            {
                // Add an invalid return opcode. This will show an error message.
                sb.Append("ct" + functionName + "\r\n");
            }
            else
            {
                // Add the 'return' call
                sb.Append("cret\r\n");
            }

            // Then the runner will need to interpret both the new op-codes
            // This would include a return-stack-push for calling functions,
            //   a jump-to-absolute-position by name
            //   a jump-to-absolute-position by return stack & pop
        }

        /// <summary>
        /// Map parameter names to positional names. This must match the expectations of the interpreter
        /// </summary>
        private static void ParameterPositions(Scope parameterNames,IEnumerable<string> paramNames)
        {
            parameterNames.PushScope();
            var i = 0;
            foreach (var paramName in paramNames)
            {
                if (parameterNames.InScope(paramName)) throw new Exception("Duplicate parameter '"+paramName+"'.\r\n" +
                                                                           "All parameter names must be unique in a single function definition.");

                parameterNames.SetValue(paramName, Scope.NameFor(i)); // Pattern for positional vars.
                i++;
            }
        }
        
        private static string EscapeEncodedTextForDebug(Node root)
        {
            return StringEncoding.Decode(root.Text)
                .Replace("\\", "\\\\")
                .Replace("\t", "\\t")
                .Replace("\n", "\\n")
                .Replace("\r", "");
        }
        
        public static string Fill(int nb, char car)
        {
            var sb = new StringBuilder();
            for (int i = 0; i < nb * 4; i++)
            {
                sb.Append(car);
            }
            return sb.ToString();
        }

    }
}
