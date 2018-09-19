using System.Linq;
using System.Text;

namespace VisualREPL
{
    /// <summary>
    /// A circular buffer for strings, encoded in ASCII only
    /// </summary>
    public class CircularString
    {
        private readonly byte[] _b;
        private readonly byte[] _bt;
        private long _wp;
        private readonly object writeLock = new object();

        public CircularString(uint length)
        {
            _b = Enumerable.Repeat((byte)0, (int)length).ToArray();//new byte[length];
            _bt = new byte[length];
            _wp = 0;
        }

        public string ReadAll(){
            var l = _b.Length;
            var sb = new StringBuilder(l);
            for (int i = 0; i < l; i++)
            {
                var idx = (_wp + i) % l;
                if (_b[idx] > 0) sb.Append((char)(_b[idx]));
            }
            return sb.ToString();
        }

        public void Write(string s) {
            if (s == null) return;
            lock (writeLock)
            {
                var len = Encoding.ASCII.GetBytes(s, 0, s.Length, _bt, 0);
                for (int i = 0; i < len; i++)
                {
                    _b[_wp] = _bt[i];
                    _wp = (_wp + 1) % _b.Length;
                }
            }
        }

        public void Write(object state)
        {
            Write(state.ToString());
        }

        public void WriteLine(object state)
        {
            Write(state.ToString());
            Write("\r\n");
        }
    }
}