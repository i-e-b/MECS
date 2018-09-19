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
            var finalOut = new StringBuilder(input.Length + 100);

            var lineSoFar = new StringBuilder(120);

            var lastLineIndent = 0;
            var scount = 0;
            var len = input.Length;
            var prevCur = cursorPosition;
            for (var index = 0; index < len; index++)
            {
                char c = input[index];

                // TODO: ignore anything inside comments
                // todo: stack to track scope positions to output scope range
                if (c == '(') scount++;
                if (c == ')') {
                    scount--;
                    lastLineIndent = scount;
                }

                if (!IsNewline(c))
                {
                    lineSoFar.Append(c);
                }
                else
                {
                    var indent = lastLineIndent * SpacesPerTab;
                    if (index < prevCur) cursorPosition += indent;
                    if (index + 1 < prevCur) cursorPosition += 1;

                    finalOut.Append(' ', indent);
                    finalOut.Append(lineSoFar);
                    finalOut.Append("\n");

                    lineSoFar.Clear();
                    lastLineIndent = scount;

                    // skip leading whitespace
                    while (index + 1 < len && IsSpace(input[index + 1]))
                    {
                        if (index + 1 < prevCur) cursorPosition--;
                        index++;
                    }
                }
            }

            if (lineSoFar.Length > 0) finalOut.Append(lineSoFar);

            return finalOut.ToString();
        }

        private static bool IsSpace(char c)
        {
            return c == ' ' || c == '\t';
        }

        private static bool IsNewline(char c)
        {
            return c == '\r' || c == '\n';
        }
    }
}