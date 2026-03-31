#!/usr/bin/env python3
import sys
sys.setrecursionlimit(10000)

# Binary Trees benchmark in Python

class BinaryTreeNode:
    __slots__ = ['val', 'left', 'right']
    def __init__(self, val=0):
        self.val = val
        self.left = None
        self.right = None

def insert(root, val):
    if root is None:
        return BinaryTreeNode(val)
    if val < root.val:
        root.left = insert(root.left, val)
    else:
        root.right = insert(root.right, val)
    return root

def search(root, val):
    if root is None or root.val == val:
        return root
    if val < root.val:
        return search(root.left, val)
    return search(root.right, val)

def height(root):
    if root is None:
        return 0
    return 1 + max(height(root.left), height(root.right))

def count_nodes(root):
    if root is None:
        return 0
    return 1 + count_nodes(root.left) + count_nodes(root.right)

def inorder_sum(root):
    if root is None:
        return 0
    return inorder_sum(root.left) + root.val + inorder_sum(root.right)

def build_balanced(arr, lo, hi, root):
    if lo > hi:
        return root
    mid = (lo + hi) // 2
    root = insert(root, arr[mid])
    root = build_balanced(arr, lo, mid - 1, root)
    root = build_balanced(arr, mid + 1, hi, root)
    return root

sorted_order = list(range(1000))
root = None
root = build_balanced(sorted_order, 0, 999, root)

# Operations
found_count = 0
for i in range(0, 1000, 10):
    if search(root, i):
        found_count += 1

tree_height = height(root)
node_count = count_nodes(root)
total_sum = inorder_sum(root)

print(f"Found: {found_count}, Height: {tree_height}, Nodes: {node_count}, Sum: {total_sum}")
