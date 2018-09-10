using System;
using System.IO;
using EvieCompilerSystem.InputOutput;
using EvieCompilerSystem.Runtime;

namespace EvieCompilerSystem
{
    // ReSharper disable once UnusedMember.Global
    public static class Repl
    {

        // ReSharper disable once UnusedMember.Global
        public static void BuildAndRun(string languageInput, TextReader input, TextWriter output) {
            
            var contents = SourceCodeReader.ReadContent(languageInput, true);
            
            // Compile
            var sourceCodeReader = new Compiler.SourceCodeTokeniser();
            var program = sourceCodeReader.Read(contents);
            var compiledOutput = Compiler.Compiler.CompileRoot(program, debug: false);

            var stream = new MemoryStream();
            compiledOutput.WriteToStream(stream);
            stream.Seek(0,SeekOrigin.Begin);
            var byteCodeReader = new RuntimeMemoryModel(stream);

            // Execute
            try
            {
                var interpreter = new ByteCodeInterpreter();

                // Init the interpreter.
                interpreter.Init(byteCodeReader, input, output);

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
