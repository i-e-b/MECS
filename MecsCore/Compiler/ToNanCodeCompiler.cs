﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using MecsCore.InputOutput;
using MecsCore.Runtime;

namespace MecsCore.Compiler
{
    /// <summary>
    /// Compile source code to NanTagged file or memory layout
    /// </summary>
    public class ToNanCodeCompiler
    {
        public static string BaseDirectory = "";

        public static NanCodeWriter CompileRoot(Node root, bool debug)
        {
            var wr = new NanCodeWriter();

            if (debug)
            {
                wr.Comment("/*\r\n"
                           +"CompiledScript - Intermediate Language\r\n"
                           +"Date    : " + DateTime.Now + "\r\n"
                           +"Version : 2.0\r\n"
                           +"*/\r\n\r\n");
            }

            var parameterNames = new Scope(); // renaming for local parameters
            var includedFiles = new HashSet<string>();

            wr.Merge(Compile(root, 0, debug, parameterNames, includedFiles, Context.Default));

            return wr;
        }

        /// <summary>
        /// Function/Program compiler. This is called recursively when subroutines are found
        /// </summary>
        public static NanCodeWriter Compile(Node root, int indent, bool debug, Scope parameterNames, HashSet<string> includedFiles, Context compileContext)
        {
		    var wr = new NanCodeWriter();

            if (root.IsLeaf)
            {
                EmitLeafNode(root, debug, parameterNames, compileContext, wr);
            }
            else
            {
                var rootContainer = root;
                foreach (var node in rootContainer.Children.ToList())
                {
				    if (!node.IsLeaf)
                    {
					    if (IsMemoryFunction(node) )
					    {
					        CompileMemoryFunction(indent, debug, node, wr, parameterNames);
					    }
                        else if (IsInclude(node))
                        {
                            CompileExternalFile(indent, debug, node, wr, parameterNames, includedFiles);
                        }
                        else if (IsFlowControl(node))
					    {
					        wr.ReturnsValues |= CompileConditionOrLoop(indent, debug, node, wr, parameterNames);
					    }
                        else if (IsFunctionDefinition(node))
                        {
                            CompileFunctionDefinition(indent, debug, node, wr, parameterNames);
                        }
                        else
					    {
					        wr.ReturnsValues |= CompileFunctionCall(indent, debug, wr, node, parameterNames);
					    }
				    }
                    else
                    {
                        wr.Merge(Compile(node, indent + 1, debug, parameterNames, includedFiles, compileContext));
				    }
			    }
		    }

		    return wr;
	    }

        /// <summary>
        /// Output atom values
        /// </summary>
        private static void EmitLeafNode(Node root, bool debug, Scope parameterNames, Context compileContext, NanCodeWriter wr)
        {
            double leafValue = 0;
            bool substitute = false;
            var valueName = root.Text;
            var nameHash = NanTags.GetCrushedName(valueName);
            if (parameterNames.CanResolve(nameHash)) {
                substitute = true;
                leafValue = parameterNames.Resolve(nameHash);
            }

            // An unwrapped variable name?
            if (IsUnwrappedIdentifier(valueName, root, compileContext))
            {
                if (debug) {
                    wr.Comment("// treating '"+valueName+"' as an implicit get()");
                }
                if (substitute) wr.Memory('g', leafValue);
                else wr.Memory('g', valueName, 0);

                return;
            }

            if (debug)
            {
                wr.Comment("// Value : \"" + root + "\"\r\n");
                if (substitute)
                {
                    wr.Comment("// Parameter reference redefined as '" + valueName + "'\r\n");
                }
            }

            switch (root.NodeType)
            {
                case NodeType.Numeric:
                    wr.LiteralNumber(double.Parse(valueName.Replace("_","")));
                    break;

                case NodeType.StringLiteral:
                    wr.LiteralString(valueName);
                    break;

                case NodeType.Atom:
                    if (valueName == "true") wr.LiteralInt32(-1);
                    else if (valueName == "false") wr.LiteralInt32(0);
                    else if (substitute) wr.RawToken(leafValue);
                    else wr.VariableReference(valueName);
                    break;

                default:
                    throw new Exception("Unexpected compiler state");
            }
        }

        private static bool IsUnwrappedIdentifier(string valueName, Node root, Context compileContext)
        {
            if ( root.NodeType != NodeType.Atom || compileContext == Context.MemoryAccess) return false;

            switch (valueName) {
                // reserved identifiers
                case "false":
                case "true":
                    return false;

                default:
                    return true;
            }
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

        private static void CompileMemoryFunction(int level, bool debug, Node node, NanCodeWriter wr, Scope parameterNames)
        {
            // Check for special increment mode
            if (node.Text == "set" && Optimisations.IsSmallIncrement(node, out var incr, out var target))
            {
                wr.Increment(incr, target);
                return;
            }

            // Pivot the node's children into 1st child, with the rest as 1st child's children
            var child = new Node(false, node.SourceLocation) {Text = node.Children.First.Value.Text};
            var paramCount = node.Children.Count - 1;
            for (int i = paramCount; i > 0; i--) { child.AddLast(node.Children.ElementAt(i)); }

            wr.Merge(Compile(child, level + 1, debug, parameterNames, null, Context.MemoryAccess));

            if (debug)
            {
                wr.Comment("// Memory function : " + node.Text);
            }

            wr.Memory(node.Text[0], child.Text, paramCount);
        }

        private static void CompileExternalFile(int level, bool debug, Node node, NanCodeWriter wr, Scope parameterNames, HashSet<string> includedFiles)
        {
            //     1) Check against import list. If already done, warn and skip.
            //     2) Read file. Fail = terminate with error
            //     3) Compile to opcodes
            //     4) Inject opcodes in `wr`

            if (includedFiles == null) throw new Exception("Files can only be included at the root level");

            var targetFile = Path.Combine(BaseDirectory, node.Children.First.Value.Text);

            if (includedFiles.Contains(targetFile)) {
                wr.Comment("// Ignored import: " + targetFile);
                return;
            }

            if (!File.Exists(targetFile)) throw new Exception("Import failed. Can't read file '" + targetFile + "'");

            var text = File.ReadAllText(targetFile);
            includedFiles.Add(targetFile);
            
            var reader = new SourceCodeTokeniser();
            var parsed = reader.Read(text, false);
            var programFragment = Compile(parsed, level, debug, parameterNames, includedFiles, Context.External);


            if (debug)
            {
                wr.Comment("// File import: \"" + targetFile);
            }

            wr.Merge(programFragment);

            if (debug)
            {
                wr.Comment("// <-- End of file import: \"" + targetFile);
            }
        }

        private static bool CompileConditionOrLoop(int level, bool debug, Node node, NanCodeWriter wr, Scope parameterNames)
        {
            bool returns = false;
            if (node.Children.Count < 1)
            {
                throw new ArgumentException(node.Text + " required parameter(s)");
            }

            bool isLoop = node.Text == "while";
            var context = isLoop ? Context.Loop : Context.Condition;
            var condition = new Node(false, -2);
            condition.AddLast(node.Children.ElementAt(0));
            condition.Text = "()";

            var topOfBlock = wr.Position() - 1;
            

            var body = new Node(false, -2);

            for (int i = 1; i < node.Children.Count; i++)
            {
                body.AddLast(node.Children.ElementAt(i));
            }

            body.Text = "()";
            var compiledBody = Compile(body, level + 1, debug, parameterNames, null, context);
            returns |= compiledBody.ReturnsValues;
            var opCodeCount = compiledBody.OpCodeCount();
            if (isLoop) opCodeCount++; // also skip the end unconditional jump

            if (debug)
            {
                wr.Comment("// Compare condition for : " + node.Text +", If false, skip " + compiledBody.OpCodeCount() + " element(s)");
            }

            if (Optimisations.IsSimpleComparion(condition, opCodeCount))
            {
                // output just the arguments
                var argNodes = Optimisations.ReadSimpleComparison(condition, out var cmpOp, out var argCount);
                var conditionArgs = Compile(argNodes, level + 1, debug, parameterNames, null, context);
                wr.Merge(conditionArgs);
                wr.CompoundCompareJump(cmpOp, argCount, (ushort) opCodeCount);
            }
            else
            {
                var conditionCode = Compile(condition, level + 1, debug, parameterNames, null, context);

                if (debug)
                {
                    wr.Comment("// Condition for : " + node.Text);
                }

                wr.Merge(conditionCode);
                wr.CompareJump(opCodeCount);
            }

            wr.Merge(compiledBody);

            if (debug) { wr.Comment("// End : " + node.Text); }
            if (isLoop)
            {

                var distance = wr.Position() - topOfBlock;
                wr.UnconditionalJump((uint)distance);
            }
            return returns;
        }

        /// <summary>
        /// Compile a function call. Returns true if it's a call to return a value
        /// </summary>
        private static bool CompileFunctionCall(int level, bool debug, NanCodeWriter wr, Node node, Scope parameterNames)
        {
            var funcName = node.Text;
            if (Desugar.NeedsDesugaring(funcName)) {
                node = Desugar.ProcessNode(funcName, parameterNames, node);
                var frag = Compile(node, level + 1, debug, parameterNames, null, Context.Default);
                wr.Merge(frag);
                return frag.ReturnsValues;
            }

            wr.Merge(Compile(node, level + 1, debug, parameterNames, null, Context.Default));


            if (debug)
            {
                wr.Comment("// Function : \"" + funcName
                          + "\" with " + node.Children.Count + " parameter(s)");
            }

            if (funcName == "return") { wr.Return(node.Children.Count); }
            else { wr.FunctionCall(funcName, node.Children.Count); }

            return (funcName == "return") && (node.Children.Count > 0); // is there a value return?
        }

        /// <summary>
        /// Compile a custom function definition
        /// </summary>
        private static void CompileFunctionDefinition(int level, bool debug, Node node, NanCodeWriter wr, Scope parameterNames)
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
            if (bodyNode.Text != "()") throw new Exception("Bare functions not supported. Wrap your function body in (parenthesis)");

            var functionName = definitionNode.Text;
            var argCount = definitionNode.Children.Count;

            ParameterPositions(parameterNames, definitionNode.Children.Select(c => c.Text), wr);

            var subroutine = Compile(bodyNode, level, debug, parameterNames, null, Context.Default);
            var tokenCount = subroutine.OpCodeCount();

            if (debug)
            {
                wr.Comment("// Function definition : \"" + functionName
                         + "\" with " + argCount + " parameter(s)");
            }

            wr.FunctionDefine(functionName, argCount, tokenCount);
            wr.Merge(subroutine);

            if (subroutine.ReturnsValues)
            {
                // Add an invalid return opcode. This will show an error message.
                wr.InvalidReturn();
            }
            else
            {
                // Add the 'return' call
                wr.Return(0);
            }

            // Then the runner will need to interpret both the new op-codes
            // This would include a return-stack-push for calling functions,
            //   a jump-to-absolute-position by name
            //   a jump-to-absolute-position by return stack & pop
        }

        /// <summary>
        /// Map parameter names to positional names. This must match the expectations of the interpreter
        /// </summary>
        private static void ParameterPositions(Scope parameterNames, IEnumerable<string> paramNames, NanCodeWriter wr)
        {
            parameterNames.PushScope();
            var i = 0;
            foreach (var paramName in paramNames)
            {
                if (parameterNames.InScope(NanTags.GetCrushedName(paramName)))
                    throw new Exception("Duplicate parameter '"+paramName+"'.\r\n" +
                                        "All parameter names must be unique in a single function definition.");

                var originalReference = NanTags.GetCrushedName(paramName);
                var parameterReference = Scope.NameFor(i);
                var parameterByteCode = NanTags.EncodeVariableRef(parameterReference);

                parameterNames.SetValue(originalReference, parameterByteCode);

                wr.AddSymbol(originalReference, paramName);
                wr.AddSymbol(parameterReference, "param["+i+"]");

                i++;
            }
        }
    }
}
