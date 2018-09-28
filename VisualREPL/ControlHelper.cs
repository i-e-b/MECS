using System;
using System.Drawing;
using System.Windows.Forms;

namespace VisualREPL
{
    public static class ControlHelper {
        public static void Suspend(Control c) {
            if (c == null || c.IsDisposed || !c.IsHandleCreated) return;
            Win32.SendMessage(c.Handle, 0x00B, (IntPtr)0, IntPtr.Zero);
        }

        public static Point GetScrollPos(Control c){
            Point scrollPt = Point.Empty;
            Win32.SendMessage(c.Handle, Win32.EM_GETSCROLLPOS, 0, ref scrollPt);
            return scrollPt;
        }
        
        public static void SetScrollPos(Control c, Point scrollPoint){
            Win32.SendMessage(c.Handle, Win32.EM_SETSCROLLPOS, 0, ref scrollPoint);
        }
        
        public static void Resume(Control c) {
            if (c == null || c.IsDisposed || !c.IsHandleCreated) return;
            Win32.SendMessage(c.Handle, 0x00B, (IntPtr)1, IntPtr.Zero);
        }
    }
}