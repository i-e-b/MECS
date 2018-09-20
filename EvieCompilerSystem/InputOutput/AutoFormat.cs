using System.Text;

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
            return input;
            //TODO: SourceCodeTokeniser.Read, then Node.Reformat
            // Also todo: inject cursor/caret position. Maybe only reformat when caret is between tokens?
            // maybe don't reformat if tokenising fails.
        }
    }
}