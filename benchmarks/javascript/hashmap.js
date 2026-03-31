// Hash Map benchmark in JavaScript

class HashMap {
    constructor(capacity = 16) {
        this.capacity = capacity;
        this.buckets = new Array(capacity).fill(null).map(() => []);
        this.size = 0;
    }
    
    _hash(key) {
        let hash = 0;
        const str = String(key);
        for (let i = 0; i < str.length; i++) {
            hash = ((hash << 5) - hash) + str.charCodeAt(i);
            hash = hash & hash;
        }
        return Math.abs(hash) % this.capacity;
    }
    
    put(key, value) {
        const index = this._hash(key);
        const bucket = this.buckets[index];
        for (let i = 0; i < bucket.length; i++) {
            if (bucket[i][0] === key) {
                bucket[i] = [key, value];
                return;
            }
        }
        bucket.push([key, value]);
        this.size++;
    }
    
    get(key) {
        const index = this._hash(key);
        const bucket = this.buckets[index];
        for (const [k, v] of bucket) {
            if (k === key) {
                return v;
            }
        }
        return undefined;
    }
    
    remove(key) {
        const index = this._hash(key);
        const bucket = this.buckets[index];
        for (let i = 0; i < bucket.length; i++) {
            if (bucket[i][0] === key) {
                bucket.splice(i, 1);
                this.size--;
                return true;
            }
        }
        return false;
    }
    
    contains(key) {
        return this.get(key) !== undefined;
    }
}

// Create and populate hash map
const hm = new HashMap();
for (let i = 0; i < 5000; i++) {
    hm.put(`key_${i}`, i * 2);
}

// Get values
let total = 0;
for (let i = 0; i < 5000; i += 10) {
    const val = hm.get(`key_${i}`);
    if (val !== undefined) {
        total += val;
    }
}

// Remove some
for (let i = 0; i < 2500; i += 10) {
    hm.remove(`key_${i}`);
}

console.log(`Map size: ${hm.size}, Sum: ${total}`);
