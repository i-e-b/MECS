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
            var content = File.ReadAllText("listMath.ecs"); 
            Execute(content, verbose: false, argsVariables: new Dictionary<string, string>());
        }

        public static void Execute(string inSRC, bool verbose, Dictionary<string, string> argsVariables)
        {
            var contents = FileReader.ReadContent(inSRC, true);

            Console.WriteLine("\nSOURCE CODE");
            Console.WriteLine("-----------------------------------------------");
            Console.WriteLine(contents);

            // COMPILE.
            var reader = new SourceCodeReader();
            var program = reader.Read(contents);

            Console.WriteLine("\nTREE VIEW");
            Console.WriteLine("-----------------------------------------------");
            Console.WriteLine(program.Show());

            Console.WriteLine("\nCOMPILED SCRIPT (debug)");
            Console.WriteLine("-----------------------------------------------");
            Console.WriteLine(Compiler.Compiler.CompileRoot(reader.Read(contents), debug: true));
            
            var swc = new Stopwatch();
            swc.Start();
            var bin = Compiler.Compiler.CompileRoot(program, debug: false);
            swc.Stop();

            Console.WriteLine("\nCOMPILED SCRIPT (release) -- took " + swc.Elapsed);
            Console.WriteLine("-----------------------------------------------");
            Console.WriteLine(bin);
            File.WriteAllText("bin.txt", bin);

            // PREPARE RUN.
            var byteCodeReader = FileReader.ReadContent(File.ReadAllText("bin.txt"), true);
            try
            {
                var interpreter = new ByteCodeInterpreter();

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
                Console.WriteLine("Exception : " + e.Message);
                Console.WriteLine("\r\n\r\n" + e.StackTrace);
                Console.ReadLine();
            }
        }
    }
}
