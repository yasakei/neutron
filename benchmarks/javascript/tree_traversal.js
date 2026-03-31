// Tree Traversal benchmark in JavaScript

class TreeNode {
    constructor(val = 0) {
        this.val = val;
        this.left = null;
        this.right = null;
    }
}

function buildTree(depth, val = 0) {
    if (depth <= 0) {
        return null;
    }
    const node = new TreeNode(val);
    node.left = buildTree(depth - 1, val * 2 + 1);
    node.right = buildTree(depth - 1, val * 2 + 2);
    return node;
}

function inorderTraversal(node, result) {
    if (node) {
        inorderTraversal(node.left, result);
        result.push(node.val);
        inorderTraversal(node.right, result);
    }
}

function preorderTraversal(node, result) {
    if (node) {
        result.push(node.val);
        preorderTraversal(node.left, result);
        preorderTraversal(node.right, result);
    }
}

function postorderTraversal(node, result) {
    if (node) {
        postorderTraversal(node.left, result);
        postorderTraversal(node.right, result);
        result.push(node.val);
    }
}

// Build tree of depth 15
const root = buildTree(15);

// Traverse
const inorderResult = [];
inorderTraversal(root, inorderResult);

const preorderResult = [];
preorderTraversal(root, preorderResult);

const postorderResult = [];
postorderTraversal(root, postorderResult);

const sum = inorderResult.reduce((a, b) => a + b, 0);
console.log(`Tree nodes: ${inorderResult.length}, Sum: ${sum}`);
