using System.IO;
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
            _consoleTextBox.Text += value;
        }

        public override Encoding Encoding { get; } = Encoding.UTF8;
    }
}