using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Windows.Forms;

namespace VisualREPL
{
    /// <summary>
    /// Simulate Console.Read... for WinForms GUI
    /// </summary>
    internal class QueueKeyReader : TextReader
    {
        Queue<char> q = new Queue<char>();

        public override int Read()
        {
            while (q.Count < 1) {
                Thread.Sleep(100); // Block,
                Application.DoEvents(); // without freezing main thread
            }
            return q.Dequeue();
        }

        public void Q(char c)
        {
            q.Enqueue(c);
        }
    }
}