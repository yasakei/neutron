#!/usr/bin/env bun

let s = "";

for (let i = 0; i < 50000; i++) {
    s = s + "a";
}

console.log(`String length: ${s.length}`);