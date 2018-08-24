using System;
//using System.Text;

namespace CompiledScript.Utils
{
    class StringEncoding
    {
        public static string Encode(string text)
        {
            //return Convert.ToBase64String(Encoding.UTF8.GetBytes(text));
            return Uri.EscapeDataString(text);
        }

        public static string Decode(string text)
        {
            //return Encoding.UTF8.GetString(Convert.FromBase64String(text));
            return Uri.UnescapeDataString(text);
        }
    }
}
