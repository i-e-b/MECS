using System.Text;

namespace CompiledScript.Compiler
{
    class FileReader
    {
        public static string ReadContent(string content, bool removeComment)
        {
            return removeComment
                ? RemoveMultiLinesComment(RemoveInlineComment(content))
                : content;
        }

        public static string RemoveInlineComment(string expression)
        {
            var reader = new StringReader(expression);
            var builder = new StringBuilder();

            while (!reader.IsEndOfFile())
            {
                var part = reader.ReadUntilCharFromList("//");
                if (part != null)
                {
                    builder.Append(part);
                    part = reader.ReadUntilCharFromList("\n");
                    if (part != null) { continue; }

                    reader.ReadToEnd();
                }
                else
                {
                    builder.Append(reader.ReadToEnd());
                }
            }

            return builder.ToString();
        }

        public static string RemoveMultiLinesComment(string expression)
        {
            var reader = new StringReader(expression);
            var builder = new StringBuilder();

            while (!reader.IsEndOfFile())
            {
                var part = reader.ReadUntilCharFromList("/*");
                if (part != null)
                {
                    builder.Append(part);
                    part = reader.ReadUntilCharFromList("*/");
                    if (part != null) { continue; }

                    reader.ReadToEnd();
                }
                else
                {
                    builder.Append(reader.ReadToEnd());
                }
            }
            return builder.ToString();
        }
    }
}
