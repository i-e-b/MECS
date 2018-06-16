using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using CompiledScript.Utils;
using CompiledScript.Compiler;

namespace CompiledScript.Runner
{
    class BasicRunner
    {
        private List<string> program;
        public Dictionary<string, string> Variables { get; set;}
        private int words = 0;

        public virtual void Init(string bin)
        {
            program = new List<string>();
            foreach (string s in bin.Replace("\t", " ").Replace("\n", " ").Replace("\r", " ").Split(' '))
            {
                if (s.Length != 0)
                {
                    program.Add(UrlUtil.Decode(s));
                }
            }
            Variables = new Dictionary<string, string>();
        }

        public virtual string Execute(bool resetVars, bool verbose)
        {
            if (resetVars)
            {
                Variables.Clear();
            }

		    LinkedList<string> stack = new LinkedList<string>();
		    string resultat = null;
		    
            LinkedList<string> param = new LinkedList<string>();
		    int nbParams;
		    int position = 0;
		    int taille = program.Count;
		    
		    while (position < taille)
            {
			    words++;
                
                // Prevent stackoverflow.
                // Ex: if(true 1 10 20)
			    if ((words & 127) == 0)
                {
					while (stack.Count > 200)
                    {
						stack.RemoveFirst();
				    }
			    }

			    string word = program[position];

                if (verbose)
                {
                    Console.WriteLine("---------------------------------------------");
                    Console.WriteLine("Iteration #" + words);
                    Console.WriteLine("pos = "+position + "/" + taille);
                    Console.WriteLine("word = " + word);
                }

			    char action = word[0];

			    switch (action)
                {
			        case 'v': // Value.
				        stack.AddLast(word);
				        break;
			        case 'f': // Function.
				        position++;
				        nbParams = TryParse(program[position].Replace("\n", ""));
				        param.Clear();
                        // Pop values from stack.
				        for (int i = 0; i < nbParams; i++)
                        {
                            try
                            {
                                param.AddFirst(stack.Last().Substring(1));
                                stack.RemoveLast();
                            }
                            catch (Exception)
                            {
                            }
				        }
				        // Evaluate function.
				        resultat = Eval(word.Substring(1), nbParams, param);

                        // Add result on stack as a value.
                        if (resultat != null)
                        {
                            stack.AddLast("v" + resultat);
                        }
                        else
                        {
                            //pile.AddLast("vfalse");
                        }

				        break;
			        case 'c': // Compiler
				        word = word.Substring(1);
                        action = word[0];
                        if (action == 'c') // cmp
                        {
                            string condition = stack.Last();
                            stack.RemoveLast();
					        if (condition.Length == 0)
                            {
						        condition = "false";
					        }
                            else
                            {
						        condition = condition.Substring(1);
					        }

					        position++;
					        string bodyLengthString = program[position];
					        int bodyLength = TryParse(bodyLengthString);

					        // pile.addLast("v"+(!(condition.equals("false") ||
					        // condition.equals("0"))));

                            condition = condition.ToLower();

                            if (condition.Equals("false") || condition.Equals("0"))
                            {
                                int ignorer = bodyLength;
                                position += ignorer;

                                string test = program[position];
                            }
				        }
                        else if (action == 'j') // jmp
                        {
					        position++;
					        string jmpLengthString = program[position];
					        int jmpLength = TryParse(jmpLengthString);
					        position -= 2;
						    position -= jmpLength;

                            string test = program[position];
				        }
				        break;
			        case 'm': // Memory function
				        word = word.Substring(1);
				        // get|set|isset|unset
				        action = word[0];

                        string varName = stack.Last();
                        stack.RemoveLast();
                        //varName = UrlUtil.decode(nomVariable).Substring(1);
                        varName = varName.Substring(1);

				        switch (action)
                        {
				            case 'g':
					            if (Variables.ContainsKey(varName))
                                {
						            stack.AddLast("v" + Variables[varName]);
					            }
                                else
                                {
						            stack.AddLast("vfalse");
					            }
					            break;
                            case 's':
                                string value = stack.Last();
                                stack.RemoveLast();
                                //value = UrlUtil.decode(valeur).Substring(1);
                                value = value.Substring(1);
                                {
                                    Variables.Remove(varName);
                                }
                                Variables[varName] = value;
                                break;
				            case 'i':
					            stack.AddLast("v" + Variables.ContainsKey(varName));
					            break;
				            case 'u':
					            Variables.Remove(varName);
					            break;
				        }
				        break;
			        case 's':
				        // Reserved for system operation.
				        break;
			    }
			    position++;
		    }

		    if (stack.Count != 0)
            {
                resultat = stack.Last();
                stack.RemoveLast();
		    }
            else
            {
			    resultat = "";
		    }

		    stack.Clear();

		    return resultat;
	    }

	    public virtual string Eval(string nom, int nbParams, LinkedList<string> param)
        {
            nom = nom.ToLower();

		    if (nom == "()" && nbParams != 0)
            {
			    return param.ElementAt(nbParams - 1);
		    }

		    if ((nom == "=" || nom == "equals") && nbParams == 2)
            {
			    return ( param.ElementAt(0) == (param.ElementAt(1)) ) + "";
		    }

            if (nom == "eval")
            {
                SourceCodeReader reader = new SourceCodeReader();
                Node program = reader.Read(param.ElementAt(0));
                string contenuBin = CompilerWriter.Compile(program, false);
                BasicRunner basicReader = new BasicRunner();
                basicReader.Init(contenuBin);
                basicReader.Execute(false, false);
            }

            if (nom == "call")
            {
                nom = param.ElementAt(0);
                nbParams--;
                param.RemoveFirst();
                return Eval(nom, nbParams, param);
            }

		    if (nom == "not" && nbParams == 1)
            {
			    string condition = param.ElementAt(0);

                condition = condition.ToLower();

			    return "" + (condition == "false" || condition == "0");
		    }

            if (nom == "or")
            {
                bool continuer = nbParams > 0;
                int i = 0;
                string result = "false";
                string condition;
                while (continuer)
                {
                    condition = param.ElementAt(i);
                    condition = condition.ToLower();

                    if (condition != "false" && condition != "0")
                    {
                        result = "true";
                        break;
                    }
                    i++;
                    continuer = i < nbParams;
                }
                return result;
            }

            if (nom == "and")
            {
                bool continuer = nbParams > 0;
                int i = 0;
                string result = continuer + "";
                string condition;
                while (continuer)
                {
                    condition = param.ElementAt(i);
                    condition = condition.ToLower();

                    if (condition == "false" || condition == "0")
                    {
                        result = "false";
                        break;
                    }
                    i++;
                    continuer = i < nbParams;
                }
                return result;
            }

            if (nom.StartsWith("read"))
            {
                switch (nom.Substring(4))
                {
                    case "key":
                        Console.ReadKey();
                        return null;
                    case "line":
                        return Console.ReadLine();
                    default:
                        break;
                }

            }
            else if (nom == "print")
            {
			    foreach (string s in param)
                {
                    Console.Write(s);
			    }
                Console.WriteLine();
            }
            else if (nom == "substring")
            {
                if (nbParams == 2)
                {
                    return param.ElementAt(0).Substring(TryParse(param.ElementAt(1)));
                }
                else if(nbParams == 3)
                {
                    int start = TryParse(param.ElementAt(1));
                    int length = TryParse(param.ElementAt(2));
                    
                    try
                    {
                        string s = param.ElementAt(0).Substring(start, length);
                        return s;
                    }
                    catch(Exception)
                    {
                    }
                }
            }
            else if (nom == "length" && nbParams == 1)
            {
                return param.ElementAt(0).Length + "";
            }
            else if (nom == "replace" && nbParams == 3)
            {
                string exp = param.ElementAt(0);
                string oldValue = param.ElementAt(1);
                string newValue = param.ElementAt(2);
                exp = exp.Replace(oldValue, newValue);
                return exp;
            }
            else if (nom == "concat")
            {
			    StringBuilder builder = new StringBuilder();
			    foreach (string s in param)
                {
				    builder.Append(s);
			    }
			    return builder.ToString();
		    }
            else if (Regex.IsMatch(nom, "^[\\+|\\-|\\*|\\/|\\%]$") && nbParams == 2)
            {
			    try
                {
				    return EvalMath(nom[0], TryParse(param.ElementAt(0)),
						    TryParse(param.ElementAt(1)));
			    }
                catch (Exception e)
                {
				    return "Math Error : " + e.Message;
			    }
		    }

		    return null; // Void.
	    }

	    private static int TryParse(string s)
        {
            int.TryParse(s, out int result);
            return result;
	    }

	    private static string EvalMath(char op, int opa, int opb)
        {
		    switch (op)
            {
		        case '+':
			        return (opa + opb) + "";
		        case '-':
			        return (opa - opb) + "";
		        case '*':
			        return (opa * opb) + "";
		        case '/':
			        if (opb == 0)
                    {
				        return "Error Math : Divide by 0";
			        }
			        return (opa / opb) + "";
		        case '%':
			        if (opb == 0)
                    {
				        return "Error Math : Divide by 0";
			        }
			        return (opa % opb) + "";
		    }
		    return "Error Math : unknow op : " + op;
	    }
    }
}
