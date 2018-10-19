using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Windows.Forms;
using MecsCore;
using MecsCore.InputOutput;

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
                    traceCheckbox.Checked, showBytecodeCheck.Checked, memTraceCheckBox.Checked);
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
                    saveFileDialog1.FileName = openFileDialog1.FileName; // for overwriting
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

        private void scriptInputBox_SelectionChanged(object sender, EventArgs e)
        {
            if (_textChanging) return;
            if (scriptInputBox.SelectionLength > 0) return; // don't mess while doing selection operations
            scriptInputBox_TextChanged(null, null); // reformat and re-highlight scope
        }

        private void scriptInputBox_TextChanged(object sender, EventArgs e)
        {
            if (_textChanging) return;
            if (scriptInputBox.SelectionLength > 0) return; // don't mess while doing selection operations
            lock (_textLock) {
                try
                {
                    _textChanging = true;

                    // Try reformatting
                    var caret = scriptInputBox.SelectionStart;
                    var updated = AutoFormat.Reformat(scriptInputBox.Text, ref caret, out var scopeStart, out var scopeLength);

                    // If failed, change nothing
                    if (updated == null) return;

                    RefreshSourceDisplay(updated, scopeStart, scopeLength, caret);
                }
                finally
                {
                    _textChanging = false;
                }
            }
        }

        private void RefreshSourceDisplay(string updated, int scopeStart, int scopeLength, int caret)
        {
            // Stop the text box from rendering, save scroll position
            var scroll = ControlHelper.GetScrollPos(scriptInputBox);
            ControlHelper.Suspend(scriptInputBox);

            // Update text and styling
            scriptInputBox.Text = updated;

            try
            {
                scriptInputBox.Select(scopeStart, scopeLength);
                scriptInputBox.BackColor = Color.White;
                scriptInputBox.SelectionBackColor = Color.LightGray;
            }
            catch
            {
                /*ignore*/
            }

            // Set caret
            scriptInputBox.Select(caret, 0);

            // Restore scroll, enable rendering, trigger a refresh
            ControlHelper.SetScrollPos(scriptInputBox, scroll);
            ControlHelper.Resume(scriptInputBox);
            scriptInputBox.Invalidate();
        }
    }
}
