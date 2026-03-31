#!/usr/bin/env python3

# Hash Map benchmark in Python

class HashMap:
    def __init__(self, capacity=16):
        self.capacity = capacity
        self.buckets = [[] for _ in range(capacity)]
        self.size = 0
    
    def _hash(self, key):
        return hash(key) % self.capacity
    
    def put(self, key, value):
        index = self._hash(key)
        bucket = self.buckets[index]
        for i, (k, v) in enumerate(bucket):
            if k == key:
                bucket[i] = (key, value)
                return
        bucket.append((key, value))
        self.size += 1
    
    def get(self, key):
        index = self._hash(key)
        bucket = self.buckets[index]
        for k, v in bucket:
            if k == key:
                return v
        return None
    
    def remove(self, key):
        index = self._hash(key)
        bucket = self.buckets[index]
        for i, (k, v) in enumerate(bucket):
            if k == key:
                del bucket[i]
                self.size -= 1
                return True
        return False
    
    def contains(self, key):
        return self.get(key) is not None

# Create and populate hash map
hm = HashMap()
for i in range(5000):
    hm.put(f"key_{i}", i * 2)

# Get values
total = 0
for i in range(0, 5000, 10):
    val = hm.get(f"key_{i}")
    if val is not None:
        total += val

# Remove some
for i in range(0, 2500, 10):
    hm.remove(f"key_{i}")

print(f"Map size: {hm.size}, Sum: {total}")
