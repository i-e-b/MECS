using System;
using System.Drawing;
using System.Runtime.InteropServices;

namespace VisualREPL
{
    public static class Win32 {
        [DllImport("user32.dll")]
        public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lparam);
        
        [DllImport("user32.dll")]
        public static extern IntPtr SendMessage(IntPtr hWnd, int wMsg, int wParam, ref Point lParam);
        public const int WM_USER = 0x400;
        public const int EM_SETSCROLLPOS = WM_USER + 222;
        public const int EM_GETSCROLLPOS = WM_USER + 221;
    }
}