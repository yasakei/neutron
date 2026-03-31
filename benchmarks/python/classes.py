#!/usr/bin/env python3

# Class Inheritance benchmark in Python

class Animal:
    def __init__(self, name):
        self.name = name
    
    def speak(self):
        return "..."

class Mammal(Animal):
    def __init__(self, name, warm_blooded=True):
        super().__init__(name)
        self.warm_blooded = warm_blooded
    
    def give_birth(self):
        return f"{self.name} gives birth to live young"

class Bird(Animal):
    def __init__(self, name, can_fly=True):
        super().__init__(name)
        self.can_fly = can_fly
    
    def lay_eggs(self):
        return f"{self.name} lays eggs"

class Dog(Mammal):
    def __init__(self, name, breed):
        super().__init__(name)
        self.breed = breed
    
    def speak(self):
        return f"{self.name} says Woof!"
    
    def fetch(self):
        return f"{self.name} fetches the ball"

class Cat(Mammal):
    def __init__(self, name, color):
        super().__init__(name)
        self.color = color
    
    def speak(self):
        return f"{self.name} says Meow!"
    
    def climb(self):
        return f"{self.name} climbs the tree"

class Parrot(Bird):
    def __init__(self, name, vocabulary_size):
        super().__init__(name)
        self.vocabulary_size = vocabulary_size
    
    def speak(self):
        return f"{self.name} says 'Hello!'"
    
    def mimic(self, sound):
        return f"{self.name} mimics: {sound}"

# Create instances
animals = []
for i in range(100):
    animals.append(Dog(f"Dog{i}", "Labrador"))
    animals.append(Cat(f"Cat{i}", "Black"))
    animals.append(Parrot(f"Parrot{i}", 50))

# Test polymorphism
sounds = []
for animal in animals:
    sounds.append(animal.speak())

# Test specific methods
actions = []
for animal in animals:
    if isinstance(animal, Dog):
        actions.append(animal.fetch())
    elif isinstance(animal, Cat):
        actions.append(animal.climb())
    elif isinstance(animal, Parrot):
        actions.append(animal.mimic("whistle"))

print(f"Created {len(animals)} animals, {len(sounds)} sounds, {len(actions)} actions")
