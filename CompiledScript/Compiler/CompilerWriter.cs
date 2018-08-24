using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using CompiledScript.Utils;

namespace CompiledScript.Compiler
{
    class CompilerWriter
    {
        public static string Fill(int nb, char car)
        {
            var builder = new StringBuilder();
            for (int i = 0; i < nb * 4; i++)
            {
                builder.Append(car);
            }
            return builder.ToString();
        }

        public static string Compile(Node root, bool debug)
        {
            var builder = new StringBuilder();

            if (debug)
            {
                builder.Append("/*\r\n");
                builder.Append("CompiledScript - Intermediate Language\r\n");
                builder.Append("Date    : " + DateTime.Now + "\r\n");
                builder.Append("Version : 1.1\r\n");
                builder.Append("*/\r\n\r\n");
            }

            builder.Append(Compile(root, 0, debug));

            return builder.ToString();
        }

        public static string Compile(Node root, int level, bool debug)
        {
		    var builder = new StringBuilder();

		    if (root.IsLeaf)
            {
                if (debug)
                {
                    builder.Append(Fill(level - 1, ' '));
                    builder.Append("// Value : \"");
                    builder.Append(
                        UrlUtil.Decode(root.Text)
                            .Replace("\\", "\\\\")
                            .Replace("\t", "\\t")
                            .Replace("\n", "\\n")
                            .Replace("\r", ""));
                    builder.Append("\"\r\n");
                    builder.Append(Fill(level - 1, ' '));
                }

			    builder.Append("v").Append(root.Text);
			    //builder.Append(" ");
                builder.Append("\r\n");
		    }
            else
            {
                Node rootContainer = root;
                foreach (Node node in rootContainer.Children.ToList())
                {
				    node.Text = UrlUtil.Encode(node.Text);
				    if (!node.IsLeaf)
                    {
                        node.Text = node.Text.ToLower();
					    Node container = node;
					    if (Regex.IsMatch(node.Text, "^(get|set|isset|unset)$") )
                        {
                            switch (node.Text)
                            {
                                case "set":
                                    if (container.Children.Count != 2)
                                    {
                                        throw new Exception(node.Text+" required 2 parameters");
                                    }
                                    break;
                                default:
                                    if (container.Children.Count != 1)
                                    {
                                        throw new Exception(node.Text+" required 1 parameter");
                                    }
                                    break;
                            }

						    int nb = node.Text == "set" ? 1 : 0;
						    Node child = new Node(false);
						    child.Text = container.Children.ElementAt(0).Text;
						    for (int i = nb; i >= 0; i--)
                            {
							    child.Children.AddLast(container.Children.ElementAt(i));
						    }
                            
						    builder.Append(Compile(child, level + 1, debug));

                            if (debug)
                            {
                                builder.Append(Fill(level, ' '));
                                builder.Append("// Memory function : " + node.Text);
                                builder.Append("\r\n");
                                builder.Append(Fill(level, ' '));
                            }

						    builder.Append("m"+node.Text[0]);
						
						    //builder.Append(" ");
                            builder.Append("\r\n");
					    }
                        else if (Regex.IsMatch(node.Text, "if|while"))
                        {
                            if (container.Children.Count < 1)
                            {
                                throw new ArgumentException(node.Text+" required parameter(s)");
                            }

						    bool boucle = node.Text == "while";
                            Node condition = new Node(false);
						    condition.Children.AddLast(container.Children.ElementAt(0));
						    condition.Text = "()";

                            string compiledCondition = Compile(condition, level + 1, debug);

                            if (debug)
                            {
                                builder.Append(Fill(level, ' '));
                                builder.Append("// Condition for : " + node.Text);
                                builder.Append("\r\n");
                            }

                            builder.Append(compiledCondition);

                            Node body = new Node(false);
						    for (int i = 1; i < container.Children.Count; i++)
                            {
							    body.Children.AddLast(container.Children.ElementAt(i));
						    }
						    body.Text = "()";
                            string compileBody = Compile(body, level + 1, debug);
						
						    string clean = FileReader.RemoveInlineComment(compileBody);
						    

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
						    if (boucle)
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
                                builder.Append("\r\n");
                                builder.Append(Fill(level, ' '));
                                builder.Append("// Compare condition for : " + node.Text);
                                builder.Append("\r\n");
                                builder.Append(Fill(level, ' '));
                            }

						    builder.Append("ccmp");
						    builder.Append(" ").Append(nbElementBody);

                            if (debug)
                            {
                                builder.Append(" // If false, skip "+nbElementBody+" element(s)");
                            }

						    //builder.Append(" ");
                            builder.Append("\r\n");
						    
						    builder.Append(compileBody);

                            if (boucle)
                            {
                                if (debug)
                                {
                                    builder.Append(Fill(level, ' '));
                                    builder.Append("// End : " + node.Text);
                                    builder.Append("\r\n");
                                    builder.Append(Fill(level, ' '));
                                }

                                builder.Append("cjmp");
                                builder.Append(" ").Append(nbElementBack);
                                //builder.Append(" ");
                                builder.Append("\r\n");
                                if (debug)
                                {
                                    builder.Append("\r\n");
                                }
                            }
                            else
                            {
                                if (debug)
                                {
                                    builder.Append(Fill(level, ' '));
                                    builder.Append("// End : " + node.Text);
                                    builder.Append("\r\n");
                                    builder.Append("\r\n");
                                }
                            }
					    }
                        else
                        {
                            builder.Append(Compile(node, level + 1, debug));

                            if (debug)
                            {
                                builder.Append(Fill(level, ' '));
                                builder.Append("// Function : \"" + UrlUtil.Decode(node.Text));
                                builder.Append("\" with " + node.Children.Count);
                                builder.Append(" parameter(s)");
                                builder.Append("\r\n");
                                builder.Append(Fill(level, ' '));
                            }
                            
						    builder.Append("f");
						    builder.Append(node.Text).Append(" ");
						    builder.Append(node.Children.Count);
						    //builder.Append(" ");
                            builder.Append("\r\n");
					    }
				    }
                    else
                    {
                        builder.Append(Compile(node, level + 1, debug));
				    }
			    }
		    }
		    return builder.ToString();
	    }
    }
}
