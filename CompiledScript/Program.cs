using CompiledScript.Compiler;
using CompiledScript.Runner;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace CompiledScript
{
    class Program
    {
        static void Main()
        {
            var content = File.ReadAllText("fib.ecs");
            Execute(content, verbose: false, argsVariables: new Dictionary<string, string>());
        }

        public static void Execute(string inSRC, bool verbose, Dictionary<string, string> argsVariables)
        {
            string contents = FileReader.ReadContent(inSRC, true);
            Console.WriteLine("\nSOURCE CODE");
            Console.WriteLine("-----------------------------------------------");
            Console.WriteLine(contents);

            // COMPILE.
            SourceCodeReader reader = new SourceCodeReader();
            Node program = reader.Read(contents);

            Console.WriteLine("\nTREE VIEW");
            Console.WriteLine("-----------------------------------------------");
            Console.WriteLine(program.Show());

            Console.WriteLine("\nCOMPILED SCRIPT (debug)");
            Console.WriteLine("-----------------------------------------------");
            Console.WriteLine(CompilerWriter.CompileRoot(reader.Read(contents), debug: true));

            string bin = CompilerWriter.CompileRoot(program, debug: false);
            Console.WriteLine("\nCOMPILED SCRIPT (release)");
            Console.WriteLine("-----------------------------------------------");
            Console.WriteLine(bin);
            File.WriteAllText("bin.txt", bin);

            // PREPARE RUN.
            string byteCodeReader = FileReader.ReadContent(File.ReadAllText("bin.txt"), true);
            try
            {
                var interpreter = new BasicInterpreter();

                // Init the interpreter.
                interpreter.Init(byteCodeReader);

                foreach (var pair in argsVariables.ToArray())
                {
                    interpreter.Variables.SetValue(pair.Key, pair.Value);
                }

                // EXECUTE
                Console.WriteLine("\nSCRIPT EXECUTION");
                Console.WriteLine("-----------------------------------------------");

                var sw = new Stopwatch();
                sw.Start();
                interpreter.Execute(false, verbose);
                sw.Stop();

                Console.WriteLine("\r\nFinished. Execution took "+sw.Elapsed);
                Console.ReadLine();
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
