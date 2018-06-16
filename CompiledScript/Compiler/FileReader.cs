using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace CompiledScript.Compiler
{
    class FileReader
    {
        public static string ReadComments(string content, bool removeComment)
        {
            return removeComment
                ? RemoveMultiLinesComment(RemoveInlineComment(content))
                : content;
        }

        public static string RemoveInlineComment(string expression)
        {
            StringReader reader = new StringReader(expression);
            StringBuilder builder = new StringBuilder();
            string part;

            while (!reader.IsEndOfFile())
            {
                part = reader.ReadUntilCharFromList("//");
                if (part != null)
                {
                    builder.Append(part);
                    part = reader.ReadUntilCharFromList("\n");
                    if (part != null)
                    {
                        continue;
                    }
                    else
                    {
                        reader.ReadToEnd();
                    }
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
            StringReader reader = new StringReader(expression);
            StringBuilder builder = new StringBuilder();
            string part;

            while (!reader.IsEndOfFile())
            {
                part = reader.ReadUntilCharFromList("/*");
                if (part != null)
                {
                    builder.Append(part);
                    part = reader.ReadUntilCharFromList("*/");
                    if (part != null)
                    {
                        continue;
                    }
                    else
                    {
                        reader.ReadToEnd();
                    }
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
