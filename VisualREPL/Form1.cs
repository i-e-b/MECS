using System;
using System.IO;
using System.Threading;
using System.Windows.Forms;
using EvieCompilerSystem;

namespace VisualREPL
{
    public partial class Form1 : Form
    {
        TextBoxStreamOutput streamOut;
        public Form1()
        {
            InitializeComponent();
            streamOut = new TextBoxStreamOutput(consoleTextBox);
        }

        private void consoleTextBox_PreviewKeyDown(object sender, PreviewKeyDownEventArgs e)
        {
            // TODO: drive the input reader
        }

        private void runButton_Click(object sender, EventArgs e)
        {
            var text = scriptInputBox.Text;
            //var runner = new Thread(()=> {
            Repl.BuildAndRun(text, new NewlineReader(), streamOut);
            //}){ IsBackground = true};

            //runner.Start();
        }
    }

    // spits out endless newlines
    internal class NewlineReader : TextReader
    {
        public override int Read()
        {
            return '\n';
        }
    }
}
