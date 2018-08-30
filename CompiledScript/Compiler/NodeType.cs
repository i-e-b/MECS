namespace CompiledScript.Compiler
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
        Atom = 3
    }
}