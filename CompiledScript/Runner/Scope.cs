using System;
using System.Collections.Generic;

namespace CompiledScript.Runner
{
    /// <summary>
    /// Simple variable scope storage and resolver.
    /// Values are found in the most recently defined scope first
    /// </summary>
    internal class Scope
    {
        readonly LinkedList<Dictionary<string, string>> scopes;

        public Scope()
        {
            scopes = new LinkedList<Dictionary<string, string>>();
        }
        
        /// <summary>
        /// Start a new inner-most scope.
        /// Parameters are specially named by index (like "__p0", "__p1"). The compiler must match this.
        /// </summary>
        public void PushScope(ICollection<string> parameters) {
            var sd = new Dictionary<string, string>();
            var i = 0;
            foreach (var parameter in parameters)
            {
                sd.Add("__p"+i, parameter);
            }
            scopes.AddLast(sd);
        }

        /// <summary>
        /// Remove innermost scope, and drop back to the previous one
        /// </summary>
        public void DropScope() {
            scopes.RemoveLast();
        }

        /// <summary>
        /// Read a value by name
        /// </summary>
        public string Resolve(string name){
            var current = scopes.Last;
            while (current != null) {
                if (current.Value.ContainsKey(name)) return current.Value[name];
                current = current.Previous;
            }

            throw new Exception("Could not resolve '"+name+"', check program logic");
        }

        /// <summary>
        /// Set a value by name. If no scope has it, then it will be defined in the innermost scope
        /// </summary>
        public void SetValue(string name, string value) {
            var current = scopes.Last;
            while (current != null) {
                if (current.Value.ContainsKey(name)) {
                    current.Value[name] = value;
                    return;
                }
                current = current.Previous;
            }

            scopes.Last.Value.Add(name, value);
        }
    }
}