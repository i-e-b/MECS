using System;

namespace CompiledScript.Utils
{
    class StringEncoding
    {
        public static string Encode(string text)
        {
            return Uri.EscapeDataString(text);
        }

        public static string Decode(string text)
        {
            return Uri.UnescapeDataString(text);
        }
    }
}
