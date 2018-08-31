using System;
using System.Collections.Generic;

namespace EvieCompilerSystem.Utils
{
    // ReSharper disable once UnusedMember.Global
    public static class MathExtensions {

        /// <summary>
        /// Successive subtraction
        /// </summary>
        public static int ChainDifference<T>(this ICollection<T> collection, Func<T, int> selector) {
            int accum = 0;
            var first = true;

            foreach (var v in collection)
            {
                if (first) {
                    accum = selector(v);
                    first = false;
                    continue;
                }
                 
                accum -= selector(v);
            }
            return accum;
        }
        
        /// <summary>
        /// Successive division
        /// </summary>
        public static int ChainDivide<T>(this ICollection<T> collection, Func<T, int> selector) {
            int accum = 0;
            var first = true;

            foreach (var v in collection)
            {
                if (first) {
                    accum = selector(v);
                    first = false;
                    continue;
                }
                 
                accum /= selector(v);
            }
            return accum;
        }
        
        /// <summary>
        /// Successive division remainder
        /// </summary>
        public static int ChainRemainder<T>(this ICollection<T> collection, Func<T, int> selector) {
            int accum = 0;
            var first = true;

            foreach (var v in collection)
            {
                if (first) {
                    accum = selector(v);
                    first = false;
                    continue;
                }
                 
                accum %= selector(v);
            }
            return accum;
        }

        
        /// <summary>
        /// Successive multiplication
        /// </summary>
        public static int ChainProduct<T>(this ICollection<T> collection, Func<T, int> selector) {
            int accum = 1;

            foreach (var v in collection)
            {
                accum *= selector(v);
            }
            return accum;
        }
    }
}