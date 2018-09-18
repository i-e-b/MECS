using System;
using System.Collections.Generic;
using EvieCompilerSystem.InputOutput;

namespace EvieCompilerSystem.Runtime
{
    /// <summary>
    /// Simple variable scope storage and resolver.
    /// Values are found in the most recently defined scope first
    /// </summary>
    public class Scope
    {
        readonly LinkedList<HashTable<double>> scopes;

        private static readonly uint[] posParamHash;

        static Scope() {
            posParamHash = new uint[128]; // this limits the number of parameters, so is quite high
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
            scopes = new LinkedList<HashTable<double>>();
            scopes.AddLast(new HashTable<double>()); // global scope
        }

        /// <summary>
        /// Create a new scope, copying values (read-only) from another scope stack.
        /// If there are name conflicts in the scope, only the visible values will be copied
        /// </summary>
        /// <param name="importVariables">Variables to copy. All will be added to the global level.</param>
        public Scope(Scope importVariables)
        {
            scopes = new LinkedList<HashTable<double>>();
            var global = new HashTable<double>();

            if (importVariables != null)
            {
                foreach (var value in importVariables.ListAllVisible())
                {
                    global.Add(value.Key, value.Value);
                }
            }

            scopes.AddLast(global);
        }

        /// <summary>
        /// List all values in the scope
        /// </summary>
        private IEnumerable<KeyValuePair<uint, double>> ListAllVisible()
        {
            unchecked {
            var seen = new HashSet<uint>();
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
            unchecked
            {
                var sd = new HashTable<double>();
                var i = 0;
                if (parameters != null)
                {
                    foreach (var parameter in parameters)
                    {
                        sd.Add(posParamHash[i], parameter);
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
        public double Resolve(uint crushedName){
            unchecked
            {
                var current = scopes.Last;
                while (current != null)
                {
                    try
                    {
                        return current.Value[crushedName];
                    }
                    catch
                    {
                        current = current.Previous;
                    }
                }

                throw new Exception("Could not resolve '" + crushedName.ToString("X") + "', check program logic");
            }
        }

        /// <summary>
        /// Set a value by name. If no scope has it, then it will be defined in the innermost scope
        /// </summary>
        public void SetValue(uint crushedName, double value) {
            unchecked
            {
                var current = scopes.Last;
                while (current != null)
                {
                    if (current.Value.ContainsKey(crushedName))
                    {
                        current.Value[crushedName] = value;
                        return;
                    }
                    current = current.Previous;
                }

                scopes.Last.Value.Add(crushedName, value);
            }
        }

        /// <summary>
        /// Clear all scopes and restore empty globals
        /// </summary>
        public void Clear()
        {
            scopes.Clear();
            scopes.AddLast(new HashTable<double>()); // global scope
        }

        /// <summary>
        /// Does this name exist in any scopes?
        /// </summary>
        public bool CanResolve(uint crushedName)
        {
            unchecked
            {
                var current = scopes.Last;
                while (current != null)
                {
                    if (current.Value.ContainsKey(crushedName)) return true;
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
        public void Remove(uint crushedName)
        {
            if (scopes.First.Value.Remove(crushedName)) return;
            scopes.Last.Value.Remove(crushedName);
        }


        /// <summary>
        /// Get the name for a positional argument
        /// </summary>
        public static uint NameFor(int i)
        {
            return posParamHash[i];
        }

        /// <summary>
        /// Does this name exist in the top level scope?
        /// Will ignore other scopes, including global.
        /// </summary>
        public bool InScope(uint crushedName)
        {
            return scopes.Last.Value.ContainsKey(crushedName);
        }

        /// <summary>
        /// Add an increment to a stored number
        /// </summary>
        public void MutateNumber(uint crushedName, sbyte increment)
        {
            unchecked
            {
                var current = scopes.Last;
                while (current != null)
                {
                    try
                    {
                        current.Value.DirectChange(crushedName, a => a + increment);
                        return;
                    }
                    catch
                    {
                        current = current.Previous;
                    }
                }

                throw new Exception("Could not resolve '" + crushedName.ToString("X") + "', check program logic");
            }
        }
    }
}