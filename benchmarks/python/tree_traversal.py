#!/usr/bin/env python3

# Tree Traversal benchmark in Python

class TreeNode:
    def __init__(self, val=0):
        self.val = val
        self.left = None
        self.right = None

def build_tree(depth, val=0):
    if depth <= 0:
        return None
    node = TreeNode(val)
    node.left = build_tree(depth - 1, val * 2 + 1)
    node.right = build_tree(depth - 1, val * 2 + 2)
    return node

def inorder_traversal(node, result):
    if node:
        inorder_traversal(node.left, result)
        result.append(node.val)
        inorder_traversal(node.right, result)

def preorder_traversal(node, result):
    if node:
        result.append(node.val)
        preorder_traversal(node.left, result)
        preorder_traversal(node.right, result)

def postorder_traversal(node, result):
    if node:
        postorder_traversal(node.left, result)
        postorder_traversal(node.right, result)
        result.append(node.val)

# Build tree of depth 15
root = build_tree(15)

# Traverse
inorder_result = []
inorder_traversal(root, inorder_result)

preorder_result = []
preorder_traversal(root, preorder_result)

postorder_result = []
postorder_traversal(root, postorder_result)

print(f"Tree nodes: {len(inorder_result)}, Sum: {sum(inorder_result)}")
