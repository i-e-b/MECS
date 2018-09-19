using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Windows.Forms;

namespace VisualREPL
{
    public class TextBoxStreamOutput: TextWriter {
        private readonly TextBox _consoleTextBox;
        private readonly CircularString _cache;
        private bool _dirty;

        public TextBoxStreamOutput(TextBox consoleTextBox)
        {
            _cache = new CircularString(4096);
            _dirty = false;
            _consoleTextBox = consoleTextBox;

            var writerThread = new Thread(WriterWorker){IsBackground = true };
            writerThread.Start();
        }

        private void WriterWorker()
        {
            while (true)
            {
                if (!_dirty) {
                    Thread.Sleep(500);
                    continue;
                }
                _dirty = false;
                _consoleTextBox.Parent.Invoke(new Action(() =>
                {
                    _consoleTextBox.Text = _cache.ReadAll();
                    _consoleTextBox.SelectionStart = Math.Max(0, _consoleTextBox.Text.Length - 1);
                    _consoleTextBox.ScrollToCaret();
                }));
            }
            // ReSharper disable once FunctionNeverReturns
        }

        public override void Write(char value)
        {
            _cache.Write(value);
            _dirty = true;
        }

        public override Encoding Encoding { get; } = Encoding.UTF8;
    }
}