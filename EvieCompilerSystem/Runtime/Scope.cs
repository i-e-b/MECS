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
        /// <summary>
        /// Reference tags that have gone out of scope.
        /// </summary>
        /// <remarks>This should be written to whenever a pointer
        /// becomes from one reference. It may be available from another location.</remarks>
        public readonly HashSet<double> PotentialGarbage;

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
            PotentialGarbage = new HashSet<double>();
            scopes = new LinkedList<HashTable<double>>();
            scopes.AddLast(new HashTable<double>()); // global scope
        }
        
        /// <summary>
        /// Create a new value scope, copying references from a parent scope
        /// </summary>
        public Scope(Scope parentScope)
        {
            PotentialGarbage = new HashSet<double>();
            scopes = new LinkedList<HashTable<double>>();

            var global = new HashTable<double>();

            foreach (var pair in parentScope.ListAllVisible())
            {
                global.Add(pair); // all parent refs become globals, regardless of original hierarchy
            }

            scopes.AddLast(global);
        }

        /// <summary>
        /// List all values in the scope
        /// </summary>
        public IEnumerable<KeyValuePair<uint, double>> ListAllVisible()
        {
            unchecked
            {
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
            var last = scopes.Last.Value;
            scopes.RemoveLast();

            // this could be done on another thread
            foreach (var token in last.Values)
            {
                if (NanTags.IsAllocated(token)) PotentialGarbage.Add(token);
            }
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
        public void SetValue(uint crushedName, double newValue) {
            unchecked
            {
                var current = scopes.Last;
                while (current != null)
                {
                    if (current.Value.ContainsKey(crushedName))
                    {
                        var oldValue = current.Value[crushedName];
                        // ReSharper disable once CompareOfFloatsByEqualityOperator
                        if (oldValue != newValue)
                        {
                            if (NanTags.IsAllocated(oldValue)) PotentialGarbage.Add(newValue);
                            current.Value[crushedName] = newValue;
                        }
                        return;
                    }
                    current = current.Previous;
                }

                scopes.Last.Value.Add(crushedName, newValue);
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