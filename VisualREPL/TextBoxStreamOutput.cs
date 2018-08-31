using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace VisualREPL
{
    public class TextBoxStreamOutput: TextWriter {
        private readonly TextBox _consoleTextBox;

        public TextBoxStreamOutput(TextBox consoleTextBox)
        {
            _consoleTextBox = consoleTextBox;
        }

        public override void Write(char value)
        {
            _consoleTextBox.Parent.Invoke(new Action(() =>
            {
                _consoleTextBox.AppendText(value.ToString());
                _consoleTextBox.SelectionStart = _consoleTextBox.Text.Length - 1;
                _consoleTextBox.ScrollToCaret();
            }));
        }

        public override Encoding Encoding { get; } = Encoding.UTF8;
    }


    public static class ControlHelper {
        public static void Suspend(Control c) {
            if (c == null || c.IsDisposed || !c.IsHandleCreated) return;
            Win32.SendMessage(c.Handle, 0x00B, (IntPtr)0, IntPtr.Zero);
        }

        
        public static void Resume(Control c) {
            if (c == null || c.IsDisposed || !c.IsHandleCreated) return;
            Win32.SendMessage(c.Handle, 0x00B, (IntPtr)1, IntPtr.Zero);
        }
    }
    public static class Win32 {
        [DllImport("user32.dll")]
        public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lparam);
    }
}