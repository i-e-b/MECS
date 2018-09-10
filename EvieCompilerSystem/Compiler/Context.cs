namespace EvieCompilerSystem.Compiler
{
    public enum Context
    {
        /// <summary>
        /// Normal code
        /// </summary>
        Default,

        /// <summary>
        /// In a memory access function
        /// </summary>
        MemoryAccess,

        /// <summary>
        /// Compiling an import
        /// </summary>
        External,

        /// <summary>
        /// A loop block
        /// </summary>
        Loop,

        /// <summary>
        /// A condition block
        /// </summary>
        Condition
    }
}