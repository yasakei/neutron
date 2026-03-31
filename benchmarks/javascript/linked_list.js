// Linked List benchmark in JavaScript

class Node {
    constructor(data) {
        this.data = data;
        this.next = null;
    }
}

class LinkedList {
    constructor() {
        this.head = null;
    }
    
    append(data) {
        const newNode = new Node(data);
        if (!this.head) {
            this.head = newNode;
            return;
        }
        let current = this.head;
        while (current.next) {
            current = current.next;
        }
        current.next = newNode;
    }
    
    prepend(data) {
        const newNode = new Node(data);
        newNode.next = this.head;
        this.head = newNode;
    }
    
    find(data) {
        let current = this.head;
        while (current) {
            if (current.data === data) {
                return current;
            }
            current = current.next;
        }
        return null;
    }
    
    delete(data) {
        if (!this.head) return;
        if (this.head.data === data) {
            this.head = this.head.next;
            return;
        }
        let current = this.head;
        while (current.next) {
            if (current.next.data === data) {
                current.next = current.next.next;
                return;
            }
            current = current.next;
        }
    }
    
    toList() {
        const result = [];
        let current = this.head;
        while (current) {
            result.push(current.data);
            current = current.next;
        }
        return result;
    }
}

// Create and manipulate linked list
const ll = new LinkedList();
for (let i = 0; i < 1000; i++) {
    ll.append(i);
}

// Find elements
let count = 0;
for (let i = 0; i < 1000; i += 10) {
    if (ll.find(i)) {
        count++;
    }
}

// Delete some elements
for (let i = 0; i < 500; i += 10) {
    ll.delete(i);
}

const result = ll.toList();
console.log(`List length: ${result.length}, Found: ${count}`);
