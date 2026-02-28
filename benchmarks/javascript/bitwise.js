#!/usr/bin/env bun

let a = 0;
let b = 0;
let c = 0;

for (let i = 0; i < 5000000; i++) {
    a = i & 0xFFFF;
    b = i | 0xAAAA;
    c = a ^ b;
    c = ~c;
    c = c << 2;
    c = c >> 1;
}

console.log(`Bitwise result: ${c}`);