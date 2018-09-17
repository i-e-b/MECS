using System;
using System.Collections.Generic;
using EvieCompilerSystem.InputOutput;

// TODO: This currently is faking `int` keys.
// It seems to really improve run time.
// TODO: move scope resolution out to the compiler

namespace EvieCompilerSystem.Runtime
{
    /// <summary>
    /// Simple variable scope storage and resolver.
    /// Values are found in the most recently defined scope first
    /// </summary>
    public class Scope
    {
        readonly LinkedList<Dictionary<int, double>> scopes;

        private static readonly ulong[] posParamHash;

        static Scope() {
            posParamHash = new ulong[128]; // this limits the number of parameters, so is quite high
            for (int i = 0; i < 128; i++)
            {
                posParamHash[i] = NanTags.GetCrushedName("__p" + i);
            }
        }

        /// <summary>
        /// Create a new empty value scope
        /// </summary>
        public Scope()
        {
            scopes = new LinkedList<Dictionary<int, double>>();
            scopes.AddLast(new Dictionary<int, double>()); // global scope
        }

        /// <summary>
        /// Create a new scope, copying values (read-only) from another scope stack.
        /// If there are name conflicts in the scope, only the visible values will be copied
        /// </summary>
        /// <param name="importVariables">Variables to copy. All will be added to the global level.</param>
        public Scope(Scope importVariables)
        {
            scopes = new LinkedList<Dictionary<int, double>>();
            var global = new Dictionary<int, double>();

            unchecked
            {
                if (importVariables != null)
                {
                    foreach (var value in importVariables.ListAllVisible())
                    {
                        global.Add((int)value.Key, value.Value);
                    }
                }
            }

            scopes.AddLast(global);
        }

        /// <summary>
        /// List all values in the scope
        /// </summary>
        private IEnumerable<KeyValuePair<int, double>> ListAllVisible()
        {
            unchecked {
            var seen = new HashSet<int>();
            var scope = scopes.Last;
            while (scope != null)
            {
                foreach (var pair in scope.Value)
                {
                    if (seen.Contains(pair.Key)) continue;
                    seen.Add(pair.Key);
                    yield return pair;
                }
                scope = scope.Previous;
            }
            }
        }

        /// <summary>
        /// Start a new inner-most scope.
        /// Parameters are specially named by index (like "__p0", "__p1"). The compiler must match this.
        /// </summary>
        public void PushScope(ICollection<double> parameters = null) {
            unchecked {
            var sd = new Dictionary<int, double>();
            var i = 0;
            if (parameters != null)
            {
                foreach (var parameter in parameters)
                {
                    sd.Add((int)posParamHash[i], parameter);
                    i++;
                }
            }
            scopes.AddLast(sd);
            }
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
        public double Resolve(ulong crushedName){
            unchecked {
            var current = scopes.Last;
            while (current != null) {
                try {
                    return current.Value[(int)crushedName];
                } catch {
                    current = current.Previous;
                }
            }

            throw new Exception("Could not resolve '" + crushedName.ToString("X") + "', check program logic");
            }
        }

        /// <summary>
        /// Set a value by name. If no scope has it, then it will be defined in the innermost scope
        /// </summary>
        public void SetValue(ulong crushedName, double value) {
            unchecked {
            var current = scopes.Last;
            while (current != null) {
                if (current.Value.ContainsKey((int)crushedName)) {
                    current.Value[(int)crushedName] = value;
                    return;
                }
                current = current.Previous;
            }

            scopes.Last.Value.Add((int)crushedName, value);
            }
        }

        /// <summary>
        /// Clear all scopes and restore empty globals
        /// </summary>
        public void Clear()
        {
            scopes.Clear();
            scopes.AddLast(new Dictionary<int, double>()); // global scope
        }

        /// <summary>
        /// Does this name exist in any scopes?
        /// </summary>
        public bool CanResolve(ulong crushedName)
        {
            unchecked{
            var current = scopes.Last;
            while (current != null) {
                if (current.Value.ContainsKey((int)crushedName)) return true;
                current = current.Previous;
            }
            return false;
            }
        }

        /// <summary>
        /// Remove this variable.
        /// <para>
        /// NOTE: this will only work in the local or global scopes, but not any intermediaries.
        /// </para>
        /// It's expected that this will be used mostly on globals. They will be removed in preference to locals.
        /// </summary>
        public void Remove(ulong crushedName)
        {
            unchecked{
            if (scopes.First.Value.Remove((int)crushedName)) return;
            scopes.Last.Value.Remove((int)crushedName);
            }
        }


        /// <summary>
        /// Get the name for a positional argument
        /// </summary>
        public static ulong NameFor(int i)
        {
            return posParamHash[i];
        }

        /// <summary>
        /// Does this name exist in the top level scope?
        /// Will ignore other scopes, including global.
        /// </summary>
        public bool InScope(ulong crushedName)
        {
            unchecked{
            return scopes.Last.Value.ContainsKey((int)crushedName);
            }
        }
    }
}