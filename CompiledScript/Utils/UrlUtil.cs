using System;

namespace CompiledScript.Utils
{
    class UrlUtil
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
