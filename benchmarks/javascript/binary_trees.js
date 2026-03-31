// Binary Trees benchmark in JavaScript

class BinaryTreeNode {
    constructor(val = 0) {
        this.val = val;
        this.left = null;
        this.right = null;
    }
}

function insert(root, val) {
    if (root === null) return new BinaryTreeNode(val);
    if (val < root.val) root.left = insert(root.left, val);
    else root.right = insert(root.right, val);
    return root;
}

function search(root, val) {
    if (root === null || root.val === val) return root;
    if (val < root.val) return search(root.left, val);
    return search(root.right, val);
}

function height(root) {
    if (root === null) return 0;
    return 1 + Math.max(height(root.left), height(root.right));
}

function countNodes(root) {
    if (root === null) return 0;
    return 1 + countNodes(root.left) + countNodes(root.right);
}

function inorderSum(root) {
    if (root === null) return 0;
    return inorderSum(root.left) + root.val + inorderSum(root.right);
}

function buildBalanced(arr, lo, hi, root) {
    if (lo > hi) return root;
    const mid = Math.floor((lo + hi) / 2);
    root = insert(root, arr[mid]);
    root = buildBalanced(arr, lo, mid - 1, root);
    root = buildBalanced(arr, mid + 1, hi, root);
    return root;
}

const sortedOrder = Array.from({length: 1000}, (_, i) => i);
let root = null;
root = buildBalanced(sortedOrder, 0, 999, root);

let foundCount = 0;
for (let i = 0; i < 1000; i += 10) {
    if (search(root, i)) foundCount++;
}

const treeHeight = height(root);
const nodeCount = countNodes(root);
const totalSum = inorderSum(root);

console.log(`Found: ${foundCount}, Height: ${treeHeight}, Nodes: ${nodeCount}, Sum: ${totalSum}`);
