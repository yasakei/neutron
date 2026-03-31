#!/usr/bin/env python3

# Linked List benchmark in Python

class Node:
    def __init__(self, data):
        self.data = data
        self.next = None

class LinkedList:
    def __init__(self):
        self.head = None
    
    def append(self, data):
        new_node = Node(data)
        if not self.head:
            self.head = new_node
            return
        current = self.head
        while current.next:
            current = current.next
        current.next = new_node
    
    def prepend(self, data):
        new_node = Node(data)
        new_node.next = self.head
        self.head = new_node
    
    def find(self, data):
        current = self.head
        while current:
            if current.data == data:
                return current
            current = current.next
        return None
    
    def delete(self, data):
        if not self.head:
            return
        if self.head.data == data:
            self.head = self.head.next
            return
        current = self.head
        while current.next:
            if current.next.data == data:
                current.next = current.next.next
                return
            current = current.next
    
    def to_list(self):
        result = []
        current = self.head
        while current:
            result.append(current.data)
            current = current.next
        return result

# Create and manipulate linked list
ll = LinkedList()
for i in range(1000):
    ll.append(i)

# Find elements
count = 0
for i in range(0, 1000, 10):
    if ll.find(i):
        count += 1

# Delete some elements
for i in range(0, 500, 10):
    ll.delete(i)

result = ll.to_list()
print(f"List length: {len(result)}, Found: {count}")
