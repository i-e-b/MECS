using CompiledScript.Compiler;
using CompiledScript.Runner;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace CompiledScript
{
    class Program
    {
        static void Main()
        {
            var content = File.ReadAllText("program.txt");
            Execute(content, false, new Dictionary<string, string>());
        }

        public static void Execute(string inSRC, bool verbose, Dictionary<string, string> argsVariables)
        {
            string contenu = FileReader.ReadComments(inSRC, true);
            Console.WriteLine("\nSOURCE CODE");
            Console.WriteLine("-----------------------------------------------");
            Console.WriteLine(contenu);

            // COMPILE.
            SourceCodeReader reader = new SourceCodeReader();
            Node program = reader.Read(contenu);

            Console.WriteLine("\nTREE VIEW");
            Console.WriteLine("-----------------------------------------------");
            Console.WriteLine(program.Show());

            Console.WriteLine("\nCOMPILED SCRIPT (debug)");
            Console.WriteLine("-----------------------------------------------");
            string contenuBinDebug = CompilerWriter.Compile(reader.Read(contenu), true);
            Console.WriteLine(contenuBinDebug);

            string contenuBin = CompilerWriter.Compile(program, false);
            Console.WriteLine("\nCOMPILED SCRIPT (release)");
            Console.WriteLine("-----------------------------------------------");
            Console.WriteLine(contenuBin);
            File.WriteAllText("bin.txt", contenuBin);

            // PREPARE RUN.
            string contenuBinRead = FileReader.ReadComments(File.ReadAllText("bin.txt"), true);
            try
            {
                BasicRunner readerBin = new BasicRunner();

                // Init the interpreter.
                readerBin.Init(contenuBinRead);

                foreach (KeyValuePair<string, string> pair in argsVariables.ToArray())
                {
                    readerBin.Variables.Add(pair.Key, pair.Value);
                }

                // EXECUTE
                Console.WriteLine("\nSCRIPT EXECUTION");
                Console.WriteLine("-----------------------------------------------");
                readerBin.Execute(false, verbose);
            }
            catch (Exception e)
            {
                Console.WriteLine(e.StackTrace);
                Console.WriteLine("Exception : " + e.Message);
                Console.ReadLine();
            }
        }
    }
}
