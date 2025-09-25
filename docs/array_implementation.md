# Array Implementation in Neutron

## Overview
Neutron now supports arrays as a first-class data type. Arrays are dynamic, resizable collections that can store elements of any type.

## Features

### 1. Array Literals
Create arrays using literal syntax:
```
var numbers = [1, 2, 3, 4, 5];
var mixed = [1, "hello", true, [1, 2], 3.14];
var empty = [];
```

### 2. Index Access
Access array elements using square bracket notation:
```
var arr = [10, 20, 30];
say(arr[0]);  // prints: 10
var value = arr[1];
```

### 3. Index Assignment
Modify array elements by assigning to an index:
```
var arr = [1, 2, 3];
arr[0] = 100;
say(arr);  // prints: [100, 2, 3]
```

### 4. Native Array Functions
Neutron provides several built-in functions for array manipulation:

#### `array_new()`
Creates a new empty array.
```
var arr = array_new();
```

#### `array_push(array, value)`
Adds an element to the end of an array.
```
var arr = [1, 2];
array_push(arr, 3);
say(arr);  // prints: [1, 2, 3]
```

#### `array_pop(array)`
Removes and returns the last element of an array.
```
var arr = [1, 2, 3];
var last = array_pop(arr);
say(last);  // prints: 3
say(arr);   // prints: [1, 2]
```

#### `array_length(array)`
Returns the number of elements in an array.
```
var arr = [10, 20, 30];
var len = array_length(arr);
say(len);  // prints: 3
```

#### `array_at(array, index)`
Returns the element at the specified index.
```
var arr = ["hello", "world", 42];
var value = array_at(arr, 1);
say(value);  // prints: world
```

#### `array_set(array, index, value)`
Sets the element at the specified index to the given value.
```
var arr = [1, 2, 3];
array_set(arr, 0, 100);
say(arr);  // prints: [100, 2, 3]
```

## Memory Management
Arrays in Neutron use automatic memory management with garbage collection. Objects referenced by array elements are kept alive as long as the array is reachable.

## Error Handling
- Index out of bounds errors will throw runtime errors
- Attempting to access or assign to non-array objects will result in errors

## Examples
```
// Creating and using arrays
var fruits = ["apple", "banana", "cherry"];
say("Fruits: ");
say(fruits);

// Adding elements
array_push(fruits, "date");
say("After adding date: ");
say(fruits);

// Accessing elements
say("First fruit: ");
say(fruits[0]);

// Modifying elements
fruits[1] = "blueberry";
say("After changing second fruit: ");
say(fruits);
```