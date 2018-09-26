using System.Text;
using EvieCompilerSystem.Compiler;

namespace EvieCompilerSystem.InputOutput
{
    /// <summary>
    /// Format source code
    /// </summary>
    public class AutoFormat
    {
        private const int SpacesPerTab = 4;

        public static string Reformat(string input, ref int cursorPosition)
        {
            //return input; // uncomment to disable
            var oldCaret = cursorPosition;
            try
            {
                // Example of reformatting:
                var sourceCodeReader = new SourceCodeTokeniser();
                var program = sourceCodeReader.Read(input, true);
                var sb = new StringBuilder();
                program.Reformat(0, sb, ref cursorPosition);
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