namespace EvieCompilerSystem.InputOutput
{
    public enum DataType : ulong
    {
        /// <summary>
        /// Not a valid token
        /// </summary>
        Invalid               = 1,

        // Value-in-token ////////////////////
        
        /// <summary>
        /// Token represents a double value
        /// </summary>
        Number                = 2,

        /// <summary>
        /// NoValue - specifically not-a-value / not-a-result. Holds general runtime flags. See `NonValueType` enum
        /// </summary>
        NoValue               = 0x00000000,

        /// <summary>
        /// Variable ref (32 bit hash)
        /// </summary>
        VariableRef           = 0x00010000,

        /// <summary>
        /// Opcode (code, first param, second param)
        /// </summary>
        Opcode                = 0x00020000,

        /// <summary>
        /// Signed 32 bit integer / single boolean
        /// </summary>
        ValInt32              = 0x00030000,

        /// <summary>
        /// Unsigned integer 32
        /// </summary>
        ValUInt32             = 0x00040000,

        /// <summary>
        /// Null-terminated string of max-length 6 bytes. Longer strings must be allocated and tagged as `PtrString`
        /// </summary>
        ValSmallString        = 0x00050000,

        /// <summary>
        /// ...
        /// </summary>
        UNUSED_1              = 0x00060000,

        /// <summary>
        /// Memory pointer to Diagnostic string (note: this type could be abandoned and replaced with a seperate symbols file)
        /// </summary>
        PtrDiagnosticString   = 0x00070000,

        // Allocated ////////////////////

        /// <summary>
        /// Memory pointer to STRING header
        /// </summary>
        PtrString             = 0x80000000,

        /// <summary>
        /// Memory pointer to HASHTABLE
        /// </summary>
        PtrHashtable          = 0x80010000,

        /// <summary>
        /// Memory pointer to GRID (hash table keyed by ints) -- maybe this can be inferred from usage?
        /// </summary>
        PtrGrid               = 0x80020000,

        /// <summary>
        /// Memory pointer to ARRAY of string
        /// </summary>
        PtrArray_String       = 0x80030000,

        /// <summary>
        /// Memory pointer to ARRAY of numeric tokens (double, int32, uint32)
        /// </summary>
        PtrArray_Double       = 0x80040000,

        /// <summary>
        /// Memory pointer to SET of string header pointers
        /// </summary>
        PtrSet_String         = 0x80050000,

        /// <summary>
        ///  Memory pointer to SET of 32 signed integer
        /// </summary>
        PtrSet_Int32          = 0x80060000,

        /// <summary>
        /// Memory pointer to double-linked list node
        /// </summary>
        PtrLinkedList         = 0x80070000,
    }
}