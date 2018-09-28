using System.Text;
using EvieCompilerSystem.Compiler;

namespace EvieCompilerSystem.InputOutput
{
    /// <summary>
    /// Format source code
    /// </summary>
    public class AutoFormat
    {
        public static string Reformat(string input, ref int cursorPosition, out int scopeStart, out int scopeLength)
        {
            //return null; // uncomment to disable formatting
            var oldCaret = cursorPosition;
            scopeStart = 0;
            scopeLength = 0;
            try
            {
                // Example of reformatting:
                var sourceCodeReader = new SourceCodeTokeniser();
                var program = sourceCodeReader.Read(input, true);
                var sb = new StringBuilder();
                var focus = program.Reformat(0, sb, ref cursorPosition);
                if (focus != null && focus != program)
                {
                    scopeStart = focus.FormattedLocation;
                    scopeLength = focus.FormattedLength;
                }
                return sb.ToString();
            }
            catch
            {
                cursorPosition = oldCaret;
                return null;
            }
        }
    }
}