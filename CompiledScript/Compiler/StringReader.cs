using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace CompiledScript.Compiler
{
    public class StringReader
    {
	    private string text;
	    private int cursorPosition;
	
	    public StringReader(string text)
        {
		    this.text = text;
	    }

        public string ReadUntilCharFromList(string target)
        {
		    int indexFound = text.IndexOf(target, cursorPosition);
		    if (indexFound == -1)
            {
			    return null;
		    }

            string before = this.text.Substring(cursorPosition, indexFound - cursorPosition);
		    cursorPosition = indexFound + target.Length;
		
		    return before;
	    }

	    public bool IsEndOfFile()
        {
		    return cursorPosition >= text.Length;
	    }
        
	    public string ReadToEnd()
        {
		    int beginIndex = cursorPosition;
		    int endIndex = text.Length;
		    cursorPosition = text.Length;
		    return text.Substring(beginIndex, endIndex - beginIndex);
	    }
    }
}
