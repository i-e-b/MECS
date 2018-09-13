using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Windows.Forms;
using EvieCompilerSystem;

namespace VisualREPL
{
    public partial class Form1 : Form
    {
        readonly TextBoxStreamOutput streamOut;
        readonly QueueKeyReader streamIn;

        public Form1()
        {
            InitializeComponent();
            streamOut = new TextBoxStreamOutput(consoleTextBox);
            streamIn = new QueueKeyReader();
        }

        private void runButton_Click(object sender, EventArgs e)
        {
            var text = scriptInputBox.Text;
            var sw = new Stopwatch();
            var runner = new Thread(() =>
            {
                SetStatus("Running");
                sw.Start();
                Repl.BuildAndRun(text, streamIn, streamOut,
                    traceCheckbox.Checked, showBytecodeCheck.Checked);
                sw.Stop();
                SetStatus("Complete: "+sw.Elapsed);
            })
            { IsBackground = true };

            runner.Start();
        }

        private void SetStatus(string msg)
        {
            Invoke(new Action(() =>
            {
               statusLabel.Text = msg; 
            }));
        }

        private void consoleTextBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            streamIn.Q(e.KeyChar);
        }

        private void loadFileButton_Click(object sender, EventArgs e)
        {
            switch(openFileDialog1.ShowDialog()) {
                case DialogResult.OK:
                case DialogResult.Yes:
                    scriptInputBox.Text = File.ReadAllText(openFileDialog1.FileName);
                    break;
            }
        }

        private void clearLogButton_Click(object sender, EventArgs e)
        {
            consoleTextBox.Clear();
        }

        private void saveFileButton_Click(object sender, EventArgs e)
        {
            switch (saveFileDialog1.ShowDialog()) {
                case DialogResult.OK:
                case DialogResult.Yes:
                    File.WriteAllText(saveFileDialog1.FileName, scriptInputBox.Text);
                    break;
            }
        }
    }
}
