using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Windows.Forms;
using EvieCompilerSystem;
using EvieCompilerSystem.InputOutput;

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
                try {
                SetStatus("Running");
                sw.Start();
                var coreTime = Repl.BuildAndRun(text, streamIn, streamOut,
                    traceCheckbox.Checked, showBytecodeCheck.Checked, traceMemory: true); // TODO: checkbox for this
                sw.Stop();
                SetStatus("Complete: " + sw.Elapsed + " (execution: " + coreTime + ")");
                } catch (Exception ex) {
                    MessageBox.Show(ex.Message, "REPL error");
                }
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
                    Repl.SetBasePath(Path.GetDirectoryName(openFileDialog1.FileName));
                    break;
            }
        }

        private void clearLogButton_Click(object sender, EventArgs e)
        {
            streamOut.Clear();
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



        private readonly object _textLock = new object();
        private bool _textChanging;
        private void scriptInputBox_TextChanged(object sender, EventArgs e)
        {
            if (_textChanging) return;
            lock (_textLock) {
                try
                {
                    _textChanging = true;

                    // Try reformatting
                    var caret = scriptInputBox.SelectionStart;
                    var updated = AutoFormat.Reformat(scriptInputBox.Text, ref caret);

                    // If failed, change nothing
                    if (updated == null) return;

                    // Stop the text box from rendering, save scroll position
                    var scroll = ControlHelper.GetScrollPos(scriptInputBox);
                    ControlHelper.Suspend(scriptInputBox);

                    // Update text and caret
                    scriptInputBox.Text = updated;
                    scriptInputBox.SelectionStart = caret;

                    // Restore scroll, enable rendering, trigger a refresh
                    ControlHelper.SetScrollPos(scriptInputBox, scroll);
                    ControlHelper.Resume(scriptInputBox);
                    scriptInputBox.Invalidate();
                }
                finally
                {
                    _textChanging = false;
                }
            }
        }
    }
}
