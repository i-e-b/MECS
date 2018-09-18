using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;

namespace EvieCompilerSystem.Runtime
{
    /// <summary>
    /// A robin-hood strategy hash table.
    /// Always keys on uint, please BYO hash algorithm.
    /// </summary>
    public class HashTable<TValue> : IDictionary<uint, TValue>
    {
        private const float LOAD_FACTOR = 0.5f; // Lower = more memory, less collisions.

        private Entry[] buckets;
        private uint count;
        private uint countMod;
        private uint countUsed;
        private uint growAt;
        private uint shrinkAt;

        public HashTable(int size) : this((uint)size) { }

        public HashTable(uint size) : this()
        {
            Resize(NextPow2(size), false);
        }

        public HashTable()
        {
            Resize(32, false);
        }

        private IEnumerable<KeyValuePair<uint, TValue>> Entries
        {
            get
            {
                for (uint i = 0; i < count; i++) if (buckets[i].key != 0) yield return new KeyValuePair<uint, TValue>(buckets[i].key, buckets[i].value);
            }
        }

        private void Resize(uint newSize, bool auto = true)
        {
            var oldCount = count;
            var oldBuckets = buckets;

            count = newSize;
            countMod = newSize - 1;
            buckets = new Entry[newSize];

            growAt = auto ? Convert.ToUInt32(newSize * LOAD_FACTOR) : newSize;
            shrinkAt = auto ? newSize >> 2 : 0;

            if ((countUsed > 0) && (newSize != 0))
            {
                countUsed = 0;

                for (uint i = 0; i < oldCount; i++)
                    if (oldBuckets[i].key != 0)
                        PutInternal(oldBuckets[i], false, false);
            }
        }
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private bool Get(uint key, out TValue value)
        {
            uint index;
            if (Find(key, out index))
            {
                value = buckets[index].value;
                return true;
            }

            value = default(TValue);
            return false;
        }

        private void Put(uint key, TValue val, bool canReplace)
        {
            if (countUsed == growAt) ResizeNext();

            PutInternal(new Entry(key, val), canReplace, true);
        }

        private void PutInternal(Entry entry, bool canReplace, bool checkDuplicates)
        {
            uint indexInit = entry.key & countMod;
            uint
                probeCurrent = 0;

            for (uint i = 0; i < count; i++)
            {
                var
                    indexCurrent = (indexInit + i) & countMod;
                if (buckets[indexCurrent].key == 0)
                {
                    countUsed++;
                    buckets[indexCurrent] = entry;
                    return;
                }

                if (checkDuplicates && (entry.key == buckets[indexCurrent].key))
                {
                    if (!canReplace)
                        throw new ArgumentException("An entry with the same key already exists", nameof(entry.key));

                    buckets[indexCurrent] = entry;
                    return;
                }

                var
                    probeDistance = DistanceToInitIndex(indexCurrent);
                if (probeCurrent > probeDistance)
                {
                    probeCurrent = probeDistance;
                    Swap(ref buckets[indexCurrent], ref entry);
                }
                probeCurrent++;
            }
        }
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private bool Find(uint key, out uint index)
        {
            index = 0;
            if (countUsed <= 0) return false;
            uint indexInit = key & countMod, probeDistance = 0;

            for (uint i = 0; i < count; i++)
            {
                index = (indexInit + i) & countMod;

                if (key == buckets[index].key)
                    return true;

                if (buckets[index].key != 0)
                    probeDistance = DistanceToInitIndex(index);

                if (i > probeDistance)
                    break;
            }

            return false;
        }

        private bool RemoveInternal(uint key)
        {
            uint index;
            if (Find(key, out index))
            {
                for (uint i = 0; i < count; i++)
                {
                    var curIndex = (index + i) & countMod;
                    var nextIndex = (index + i + 1) & countMod;

                    if ((buckets[nextIndex].key == 0) || (DistanceToInitIndex(nextIndex) == 0))
                    {
                        buckets[curIndex] = default(Entry);

                        if (--countUsed == shrinkAt)
                            Resize(shrinkAt);

                        return true;
                    }

                    Swap(ref buckets[curIndex], ref buckets[nextIndex]);
                }
            }

            return false;
        }

        private uint DistanceToInitIndex(uint indexStored)
        {
            var indexInit = buckets[indexStored].key & countMod;
            if (indexInit <= indexStored) return indexStored - indexInit;
            return indexStored + (count - indexInit);
        }

        private void ResizeNext()
        {
            Resize(count == 0 ? 1 : count * 2);
        }

        private struct Entry
        {
            public Entry(uint key, TValue value)
            {
                this.key = key;
                this.value = value;
            }

            public readonly uint key;
            public readonly TValue value;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static uint NextPow2(uint c)
        {
            c--;
            c |= c >> 1;
            c |= c >> 2;
            c |= c >> 4;
            c |= c >> 8;
            c |= c >> 16;
            return c + 1;
        }
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static void Swap<T>(ref T first, ref T second)
        {
            var temp = first;
            first = second;
            second = temp;
        }
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void Add(uint key, TValue value)
        {
            Put(key, value, false);
        }
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool ContainsKey(uint key)
        {
            return Find(key, out _);
        }

        public ICollection<uint> Keys
        {
            get { return Entries.Select(entry => entry.Key).ToList(); }
        }
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Remove(uint key)
        {
            return RemoveInternal(key);
        }
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool TryGetValue(uint key, out TValue value)
        {
            return Get(key, out value);
        }

        public ICollection<TValue> Values
        {
            get { return Entries.Select(entry => entry.Value).ToList(); }
        }

        public TValue this[uint key]
        {
            get
            {
                if (!Get(key, out var result))
                    throw new KeyNotFoundException(key.ToString());

                return result;
            }
            set { Put(key, value, true); }
        }
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void Add(KeyValuePair<uint, TValue> item)
        {
            Put(item.Key, item.Value, false);
        }

        public void Clear()
        {
            Resize(0);
        }
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Contains(KeyValuePair<uint, TValue> item)
        {
            return Get(item.Key, out var result) && EqualityComparer<TValue>.Default.Equals(result, item.Value);
        }

        public void CopyTo(KeyValuePair<uint, TValue>[] array, int arrayIndex)
        {
            var kvpList = Entries.ToList();
            kvpList.CopyTo(array, arrayIndex);
        }

        public int Count => (int)countUsed;

        public bool IsReadOnly => false;

        public bool Remove(KeyValuePair<uint, TValue> item)
        {
            return Remove(item.Key);
        }

        public IEnumerator<KeyValuePair<uint, TValue>> GetEnumerator()
        {
            return Entries.ToList().GetEnumerator();
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }
    }
}