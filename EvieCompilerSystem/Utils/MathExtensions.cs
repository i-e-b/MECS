
namespace EvieCompilerSystem.Utils
{
    // ReSharper disable once UnusedMember.Global
    public static class MathExtensions {
        
        /// <summary>
        /// Successive addition
        /// </summary>
        public static double ChainSum(this double[] collection)
        {
            unchecked
            {
                double accum = 0;
                var first = true;

                for (var i = 0; i < collection.Length; i++)
                {
                    var v = collection[i];
                    if (first)
                    {
                        accum = v;
                        first = false;
                        continue;
                    }

                    accum += v;
                }

                return accum;
            }
        }

        /// <summary>
        /// Successive subtraction
        /// </summary>
        public static double ChainDifference(this double[] collection)
        {
            unchecked
            {
                double accum = 0;
                var first = true;

                for (var i = 0; i < collection.Length; i++)
                {
                    var v = collection[i];
                    if (first)
                    {
                        accum = v;
                        first = false;
                        continue;
                    }

                    accum -= v;
                }

                return accum;
            }
        }

        /// <summary>
        /// Successive division
        /// </summary>
        public static double ChainDivide(this double[] collection)
        {
            unchecked
            {
                double accum = 0;
                var first = true;

                for (var i = 0; i < collection.Length; i++)
                {
                    var v = collection[i];
                    if (first)
                    {
                        accum = v;
                        first = false;
                        continue;
                    }

                    accum /= v;
                }

                return accum;
            }
        }

        /// <summary>
        /// Successive division remainder
        /// </summary>
        public static double ChainRemainder(this double[] collection)
        {
            unchecked
            {
                double accum = 0;
                var first = true;

                for (var i = 0; i < collection.Length; i++)
                {
                    var v = collection[i];
                    if (first)
                    {
                        accum = v;
                        first = false;
                        continue;
                    }

                    accum %= v;
                }

                return accum;
            }
        }


        /// <summary>
        /// Successive multiplication
        /// </summary>
        public static double ChainProduct(this double[] collection)
        {
            unchecked
            {
                double accum = 1;

                for (var i = 0; i < collection.Length; i++)
                {
                    var v = collection[i];
                    accum *= v;
                }

                return accum;
            }
        }
    }
}