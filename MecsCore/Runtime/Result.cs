namespace MecsCore.Runtime
{
    /// <summary>
    /// Structure to for operations that might fail.
    /// </summary>
    public struct Result<T> where T: unmanaged
    {
        public bool Success;
        public T Value;
    }

    /// <summary>
    /// Struct that represents nothing. Used for Result where
    /// there is success/failure, but not an outcome
    /// </summary>
    public struct Unit{ }

    /// <summary>
    /// Helpers for result
    /// </summary>
    public static class Result
    {
        public static Result<T> Fail<T>() where T: unmanaged{
            return new Result<T>{ Success = false };
        }
        
        public static Result<T> Ok<T>(T value) where T: unmanaged{
            return new Result<T>{ Success = true, Value = value };
        }
        
        public static Result<Unit> Ok() {
            return new Result<Unit>{ Success = true, Value = default };
        }
    }
}