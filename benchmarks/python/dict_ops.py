#!/usr/bin/env python3
# Dictionary/Object operations benchmark
# Tests object creation, property access, and manipulation

class Person:
    def __init__(self, name, age, city):
        self.name = name
        self.age = age
        self.city = city
    
    def get_info(self):
        return f"{self.name} is {self.age} years old from {self.city}"

# Create many objects
people = []
for i in range(1000):
    person = Person(f"Person{i}", 20 + (i % 50), f"City{i % 10}")
    people.append(person)
print("Created 1000 person objects")

# Access properties
total_age = sum(p.age for p in people)
print(f"Total age: {total_age}")

# Call methods
count = 0
for i in range(100):
    info = people[i].get_info()
    count += 1
print(f"Called getInfo() {count} times")

# Modify properties
for i in range(500):
    people[i].age += 1
print("Modified 500 objects")

# Count by city
city_counts = sum(1 for p in people if p.city == "City0")
print(f"People in City0: {city_counts}")
