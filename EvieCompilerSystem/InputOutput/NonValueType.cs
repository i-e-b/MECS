namespace EvieCompilerSystem.InputOutput
{
    public enum NonValueType
    {
        /// <summary>Nothing returned</summary>
        Void = 0,

        /// <summary>No result, but as part of a return</summary>
        Unit = 1,

        /// <summary>
        /// A result was expected, but not produced (like trying to read an undefined slot in a collection)
        /// Use of NaR will propagate through calculations.
        /// </summary>
        Not_a_Result = 2,

        /// <summary>
        /// A non-recoverable run-time error occured. The interpreter will stop for inspection.
        /// </summary>
        Exception = 3
    }
}