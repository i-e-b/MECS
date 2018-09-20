using System;
using System.Diagnostics;
using System.IO;
using EvieCompilerSystem.Compiler;
using EvieCompilerSystem.InputOutput;
using EvieCompilerSystem.Runtime;

namespace EvieCompilerSystem
{
    // ReSharper disable once UnusedMember.Global
    public static class Repl
    {
        private static string baseDirectory = "";

        // ReSharper disable once UnusedMember.Global
        public static TimeSpan BuildAndRun(string languageInput, TextReader input, TextWriter output, bool trace, bool printIL) {
            
            // Compile
            var sourceCodeReader = new Compiler.SourceCodeTokeniser();

            Node program;
           // program = sourceCodeReader.Read(languageInput, true);
            //output.WriteLine(program.Show());
            program = sourceCodeReader.Read(languageInput, false);

            Compiler.Compiler.BaseDirectory = baseDirectory;
            var compiledOutput = Compiler.Compiler.CompileRoot(program, debug: false);

            // Load
            var stream = new MemoryStream();
            compiledOutput.WriteToStream(stream);
            stream.Seek(0,SeekOrigin.Begin);
            var byteCodeReader = new RuntimeMemoryModel(stream);
            
            if (printIL)
            {
                output.WriteLine("======= BYTE CODE SUMMARY ==========");
                compiledOutput.AddSymbols(ByteCodeInterpreter.DebugSymbolsForBuiltIns());
                output.WriteLine(value: byteCodeReader.ToString(compiledOutput.GetSymbols()));
                output.WriteLine("====================================");
            }

            // Execute
            var sw = new Stopwatch();
            try
            {
                var interpreter = new ByteCodeInterpreter();

                // Init the interpreter.
                interpreter.Init(byteCodeReader, input, output, debugSymbols: compiledOutput.GetSymbols());

                /* TODO later: ability to pass in args
                foreach (var pair in argsVariables.ToArray())
                {
                    interpreter.Variables.SetValue(pair.Key, pair.Value);
                }
                */

                sw.Start();
                interpreter.Execute(false, trace);
                sw.Stop();
                return sw.Elapsed;
            }
            catch (Exception e)
            {
                output.WriteLine("Exception : " + e.Message);
                output.WriteLine("\r\n\r\n" + e.StackTrace);
                sw.Stop();
                return sw.Elapsed;
            }
        }

        /// <summary>
        /// Set the directory used to resolve includes
        /// </summary>
        public static void SetBasePath(string path)
        {
            baseDirectory = path;
        }
    }
}
