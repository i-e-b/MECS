namespace EvieCompilerSystem.Compiler
{
    public enum NodeType
    {
        /// <summary>
        /// Unspecified node. Determined by context
        /// </summary>
        Default = 0,

        /// <summary>
        /// A string literal in the source code
        /// </summary>
        StringLiteral = 1,

        /// <summary>
        /// A number literal in the source code
        /// </summary>
        Numeric = 2,

        /// <summary>
        /// An 'atom' -- variable name, function name, etc
        /// </summary>
        Atom = 3,

        /// <summary>
        /// A comment block (including delimiters)
        /// </summary>
        Comment = -1,

        /// <summary>
        /// Whitespace excluding newlines. The *does* include ignored characters like ',' and ';'
        /// </summary>
        Whitespace = -2,

        /// <summary>
        /// A single newline
        /// </summary>
        Newline = -3,

        /// <summary>
        /// Marker for the position of the editor caret
        /// </summary>
        Caret = -4
    }
}