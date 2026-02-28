#!/usr/bin/env bun

// Dictionary/Object operations benchmark
// Tests object creation, property access, and manipulation

class Person {
    constructor(name, age, city) {
        this.name = name;
        this.age = age;
        this.city = city;
    }
    
    getInfo() {
        return `${this.name} is ${this.age} years old from ${this.city}`;
    }
}

// Create many objects
const people = [];
for (let i = 0; i < 1000; i++) {
    const person = new Person(`Person${i}`, 20 + (i % 50), `City${i % 10}`);
    people.push(person);
}
console.log("Created 1000 person objects");

// Access properties
const totalAge = people.reduce((sum, p) => sum + p.age, 0);
console.log(`Total age: ${totalAge}`);

// Call methods
let count = 0;
for (let i = 0; i < 100; i++) {
    const info = people[i].getInfo();
    count++;
}
console.log(`Called getInfo() ${count} times`);

// Modify properties
for (let i = 0; i < 500; i++) {
    people[i].age++;
}
console.log("Modified 500 objects");

// Count by city
const cityCounts = people.filter(p => p.city === "City0").length;
console.log(`People in City0: ${cityCounts}`);