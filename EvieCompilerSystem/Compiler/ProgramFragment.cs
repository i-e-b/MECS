namespace EvieCompilerSystem.Compiler
{
    internal struct ProgramFragment
    {
        public string ByteCode;
        public bool ReturnsValues;

        public override string ToString()
        {
            return ByteCode;
        }
    }
}