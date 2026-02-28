#!/usr/bin/env bun

class Point {
    constructor(x, y) {
        this.x = x;
        this.y = y;
    }
    
    dist() {
        return this.x * this.x + this.y * this.y;
    }
}

let sumVal = 0;

for (let i = 0; i < 100000; i++) {
    const p = new Point(i, i + 1);
    sumVal += p.dist();
}

console.log(`Object result: ${sumVal}`);