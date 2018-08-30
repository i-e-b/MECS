using System;
using System.Collections;
using System.Collections.Generic;

namespace CompiledScript.Runner
{
    /// <summary>
    /// Simple variable scope storage and resolver.
    /// Values are found in the most recently defined scope first
    /// </summary>
    public class Scope
    {
        readonly LinkedList<Dictionary<string, string>> scopes;

        /// <summary>
        /// Create a new empty value scope
        /// </summary>
        public Scope()
        {
            scopes = new LinkedList<Dictionary<string, string>>();
            scopes.AddLast(new Dictionary<string, string>()); // global scope
        }

        /// <summary>
        /// Create a new scope, copying values (read-only) from another scope stack.
        /// If there are name conflicts in the scope, only the visible values will be copied
        /// </summary>
        /// <param name="importVariables">Variables to copy. All will be added to the global level.</param>
        public Scope(Scope importVariables)
        {
            scopes = new LinkedList<Dictionary<string, string>>();
            var global = new Dictionary<string, string>();

            if (importVariables != null) {
                foreach (var value in importVariables.ListAllVisible()){
                    global.Add(value.Key, value.Value);
                }
            }

            scopes.AddLast(global);
        }

        /// <summary>
        /// List all values in the scope
        /// </summary>
        private IEnumerable<KeyValuePair<string,string>> ListAllVisible()
        {
            var seen = new HashSet<string>();
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

        /// <summary>
        /// Start a new inner-most scope.
        /// Parameters are specially named by index (like "__p0", "__p1"). The compiler must match this.
        /// </summary>
        public void PushScope(ICollection<string> parameters = null) {
            var sd = new Dictionary<string, string>();
            var i = 0;
            if (parameters != null)
            {
                foreach (var parameter in parameters)
                {
                    sd.Add(NameFor(i), parameter);
                    i++;
                }
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

        /// <summary>
        /// Clear all scopes and restore empty globals
        /// </summary>
        public void Clear()
        {
            scopes.Clear();
            scopes.AddLast(new Dictionary<string, string>()); // global scope
        }

        /// <summary>
        /// Does this name exist in any scopes?
        /// </summary>
        public bool CanResolve(string name)
        {
            var current = scopes.Last;
            while (current != null) {
                if (current.Value.ContainsKey(name)) return true;
                current = current.Previous;
            }
            return false;
        }

        /// <summary>
        /// Remove this variable.
        /// <para>
        /// NOTE: this will only work in the local or global scopes, but not any intermediaries.
        /// </para>
        /// It's expected that this will be used mostly on globals. They will be removed in preference to locals.
        /// </summary>
        public void Remove(string name)
        {
            if (scopes.First.Value.Remove(name)) return;
            scopes.Last.Value.Remove(name);
        }

        /// <summary>
        /// Get the name for a positional argument
        /// </summary>
        public static string NameFor(int i)
        {
            return "__p" + i;
        }

        /// <summary>
        /// Does this name exist in the top level scope?
        /// Will ignore other scopes, including global.
        /// </summary>
        public bool InScope(string name)
        {
            return scopes.Last.Value.ContainsKey(name);
        }
    }
}