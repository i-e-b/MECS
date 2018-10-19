using System;
using System.Collections.Generic;
using MecsCore.InputOutput;

namespace MecsCore.Runtime
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

        private readonly HashLookup<double>[] _scopes;
        private int _currentScopeIdx;

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
            _scopes = new HashLookup<double>[128]; // recursion depth limit
            _currentScopeIdx = 0;
            _scopes[_currentScopeIdx] = new HashLookup<double>(); // global scope
        }
        
        /// <summary>
        /// Create a new value scope, copying references from a parent scope
        /// </summary>
        public Scope(Scope parentScope)
        {
            PotentialGarbage = new HashSet<double>();
            _scopes = new HashLookup<double>[128]; // recursion depth limit
            _currentScopeIdx = 0;

            var global = new HashLookup<double>();

            foreach (var pair in parentScope.ListAllVisible())
            {
                global.Add(pair); // all parent refs become globals, regardless of original hierarchy
            }

            _scopes[_currentScopeIdx] = global;
        }

        /// <summary>
        /// List all values in the scope
        /// </summary>
        public IEnumerable<KeyValuePair<uint, double>> ListAllVisible()
        {
            unchecked
            {
                var seen = new HashSet<uint>();
                for (int i = _currentScopeIdx; i >= 0; i--)
                {
                    var scope = _scopes[i];
                    foreach (var pair in scope)
                    {
                        if (seen.Contains(pair.Key)) continue;
                        seen.Add(pair.Key);
                        yield return pair;
                    }
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
                var sd = new HashLookup<double>();
                var i = 0;
                if (parameters != null)
                {
                    foreach (var parameter in parameters)
                    {
                        sd.Add(posParamHash[i], parameter);
                        i++;
                    }
                }
                _currentScopeIdx++; // TODO: check for current > length
                _scopes[_currentScopeIdx] = sd;
            }
        }

        /// <summary>
        /// Remove innermost scope, and drop back to the previous one
        /// </summary>
        public void DropScope() {
            var last = _scopes[_currentScopeIdx];
            _scopes[_currentScopeIdx] = null;
            _currentScopeIdx--;

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
                for (int i = _currentScopeIdx; i >= 0; i--)
                {
                    var current = _scopes[i];
                    if (current.Get(crushedName, out var found)) return found;
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
                for (int i = _currentScopeIdx; i >= 0; i--)
                {
                    var current = _scopes[i];
                    if (!current.Get(crushedName, out var oldValue))
                    {
                        continue;
                    }

                    // ReSharper disable once CompareOfFloatsByEqualityOperator
                    if (oldValue == newValue) return;
                    if (NanTags.IsAllocated(oldValue)) PotentialGarbage.Add(newValue);
                    current.Put(crushedName, newValue, true);
                    return;
                }

                // nothing already exists to replace
                _scopes[_currentScopeIdx].Add(crushedName, newValue);
            }
        }

        /// <summary>
        /// Clear all scopes and restore empty globals
        /// </summary>
        public void Clear()
        {
            for (int i = _currentScopeIdx; i >= 0; i--)
            {
                _scopes[i] = null;
            }
            _currentScopeIdx = 0;
            _scopes[_currentScopeIdx] = new HashLookup<double>();
        }

        /// <summary>
        /// Does this name exist in any scopes?
        /// </summary>
        public bool CanResolve(uint crushedName)
        {
            unchecked
            {
                for (int i = _currentScopeIdx; i >= 0; i--)
                {
                    var current = _scopes[i];
                    if (current.ContainsKey(crushedName)) return true;
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
            if (_scopes[0].Remove(crushedName)) return;
            _scopes[_currentScopeIdx].Remove(crushedName);
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
            return _scopes[_currentScopeIdx].ContainsKey(crushedName);
        }

        /// <summary>
        /// Add an increment to a stored number
        /// </summary>
        public void MutateNumber(uint crushedName, sbyte increment)
        {
            unchecked
            {
                for (int i = _currentScopeIdx; i >= 0; i--)
                {
                    var current = _scopes[i];
                    if (current.DirectChange(crushedName, a => a + increment)) return;
                }

                throw new Exception("Could not resolve '" + crushedName.ToString("X") + "', check program logic");
            }
        }
    }
}