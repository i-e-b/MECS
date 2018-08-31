using System;
using System.IO;
using EvieCompilerSystem.Compiler;
using EvieCompilerSystem.InputOutput;
using EvieCompilerSystem.Runtime;

namespace EvieCompilerSystem
{
    // ReSharper disable once UnusedMember.Global
    public static class Repl
    {

        // ReSharper disable once UnusedMember.Global
        public static void BuildAndRun(string languageInput, TextReader input, TextWriter output) {
            
            var contents = InputReader.ReadContent(languageInput, true);
            
            // Compile
            var reader = new SourceCodeReader();
            var program = reader.Read(contents);
            var bin = Compiler.Compiler.CompileRoot(program, debug: false);

            // Execute
            try
            {
                var interpreter = new ByteCodeInterpreter();

                // Init the interpreter.
                interpreter.Init(bin, input, output);

                /* TODO later: ability to pass in args
                foreach (var pair in argsVariables.ToArray())
                {
                    interpreter.Variables.SetValue(pair.Key, pair.Value);
                }
                */

                interpreter.Execute(false, false);
            }
            catch (Exception e)
            {
                output.WriteLine("Exception : " + e.Message);
                output.WriteLine("\r\n\r\n" + e.StackTrace);
            }
        }

    }
}
