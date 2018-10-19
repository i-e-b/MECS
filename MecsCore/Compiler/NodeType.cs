namespace MecsCore.Compiler
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
        /// The root of a parsed document
        /// </summary>
        Root = 4,

        // Negatives are only expressed if meta-data is retained

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
        /// Opening or closing parenthesis
        /// </summary>
        ScopeDelimiter = -4,

        /// <summary>
        /// String delimiter or similar
        /// </summary>
        Delimiter = -5
    }
}