// Class Inheritance benchmark in JavaScript

class Animal {
    constructor(name) {
        this.name = name;
    }
    
    speak() {
        return "...";
    }
}

class Mammal extends Animal {
    constructor(name, warmBlooded = true) {
        super(name);
        this.warmBlooded = warmBlooded;
    }
    
    giveBirth() {
        return `${this.name} gives birth to live young`;
    }
}

class Bird extends Animal {
    constructor(name, canFly = true) {
        super(name);
        this.canFly = canFly;
    }
    
    layEggs() {
        return `${this.name} lays eggs`;
    }
}

class Dog extends Mammal {
    constructor(name, breed) {
        super(name);
        this.breed = breed;
    }
    
    speak() {
        return `${this.name} says Woof!`;
    }
    
    fetch() {
        return `${this.name} fetches the ball`;
    }
}

class Cat extends Mammal {
    constructor(name, color) {
        super(name);
        this.color = color;
    }
    
    speak() {
        return `${this.name} says Meow!`;
    }
    
    climb() {
        return `${this.name} climbs the tree`;
    }
}

class Parrot extends Bird {
    constructor(name, vocabularySize) {
        super(name);
        this.vocabularySize = vocabularySize;
    }
    
    speak() {
        return `${this.name} says 'Hello!'`;
    }
    
    mimic(sound) {
        return `${this.name} mimics: ${sound}`;
    }
}

// Create instances
const animals = [];
for (let i = 0; i < 100; i++) {
    animals.push(new Dog(`Dog${i}`, "Labrador"));
    animals.push(new Cat(`Cat${i}`, "Black"));
    animals.push(new Parrot(`Parrot${i}`, 50));
}

// Test polymorphism
const sounds = [];
for (const animal of animals) {
    sounds.push(animal.speak());
}

// Test specific methods
const actions = [];
for (const animal of animals) {
    if (animal instanceof Dog) {
        actions.push(animal.fetch());
    } else if (animal instanceof Cat) {
        actions.push(animal.climb());
    } else if (animal instanceof Parrot) {
        actions.push(animal.mimic("whistle"));
    }
}

console.log(`Created ${animals.length} animals, ${sounds.length} sounds, ${actions.length} actions`);
