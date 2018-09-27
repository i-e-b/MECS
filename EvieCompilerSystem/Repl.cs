using System;
using System.Diagnostics;
using System.IO;
using System.Text;
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
        /// <summary>
        /// Start with source code, and run program to termination
        /// </summary>
        /// <param name="languageInput">Source code</param>
        /// <param name="input">User typing input (for readline / readkey etc)</param>
        /// <param name="output">Print output from the program</param>
        /// <param name="trace">Flag. If true, write the entire interpreter flow to output</param>
        /// <param name="printIL">Flag. If true, write a view of the bytecode IL to output</param>
        /// <param name="traceMemory">Flag. If true, write memory and GC state</param>
        /// <returns>Run time, excluding compile time</returns>
        public static TimeSpan BuildAndRun(string languageInput, TextReader input, TextWriter output,
            bool trace, bool printIL, bool traceMemory) {
            
            // Compile
            var sourceCodeReader = new SourceCodeTokeniser();

            var program = sourceCodeReader.Read(languageInput, false);
            if ( ! program.IsValid) {
                output.WriteLine("Program is not well formed"); // TODO: be more helpful
                return TimeSpan.Zero;
            }

            ToNanCodeCompiler.BaseDirectory = baseDirectory;
            var compiledOutput = ToNanCodeCompiler.CompileRoot(program, debug: false);

            // Load
            var stream = new MemoryStream();
            compiledOutput.WriteToStream(stream);
            stream.Seek(0,SeekOrigin.Begin);
            var memoryModel = new RuntimeMemoryModel(stream);
            
            if (printIL)
            {
                output.WriteLine("======= BYTE CODE SUMMARY ==========");
                compiledOutput.AddSymbols(ByteCodeInterpreter.DebugSymbolsForBuiltIns());
                output.WriteLine(memoryModel.ToString(compiledOutput.GetSymbols()));
                output.WriteLine("====================================");
            }

            // Execute
            var sw = new Stopwatch();
            try
            {
                var interpreter = new ByteCodeInterpreter();

                // Init the interpreter.
                interpreter.Init(memoryModel, input, output, debugSymbols: compiledOutput.GetSymbols());

                /* TODO later: ability to pass in args
                foreach (var pair in argsVariables.ToArray())
                {
                    interpreter.Variables.SetValue(pair.Key, pair.Value);
                }
                */

                sw.Start();
                interpreter.Execute(false, trace);
                sw.Stop();

                if (traceMemory) {
                    output.WriteLine("======== GARBAGE SUMMARY ===========");
                    foreach (var token in memoryModel.Variables.PotentialGarbage) {
                        output.WriteLine(NanTags.Describe(token));
                    }
                    output.WriteLine("====================================");
                }

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
