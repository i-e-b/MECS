using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Windows.Forms;

namespace VisualREPL
{
    public class TextBoxStreamOutput: TextWriter {
        private readonly TextBox _consoleTextBox;
        private readonly StringBuilder _cache;
        private readonly object _lock;

        public TextBoxStreamOutput(TextBox consoleTextBox)
        {
            _cache = new StringBuilder();
            _lock = new object();
            _consoleTextBox = consoleTextBox;

            var writerThread = new Thread(WriterWorker){IsBackground = true };
            writerThread.Start();
        }

        private void WriterWorker()
        {
            while (true)
            {
                if (_cache.Length < 1) {
                    Thread.Sleep(500);
                    continue;
                }
                string next;
                lock(_lock) {
                    next = _cache.ToString();
                    _cache.Clear();
                }
                _consoleTextBox.Parent.Invoke(new Action(() =>
                {
                    _consoleTextBox.AppendText(next);
                    _consoleTextBox.SelectionStart = _consoleTextBox.Text.Length - 1;
                    _consoleTextBox.ScrollToCaret();
                }));
            }
            // ReSharper disable once FunctionNeverReturns
        }

        public override void Write(char value)
        {
            lock(_lock) {
                _cache.Append(value);
            }
        }

        public override Encoding Encoding { get; } = Encoding.UTF8;
    }
}