# Arrays Module Documentation

The `arrays` module provides comprehensive array manipulation and utility functions for Neutron programs. This module offers both basic operations and advanced functional programming methods for working with arrays.

## Usage

```neutron
use arrays;

// Create a new array
var myArray = arrays.new();

// Add elements
arrays.push(myArray, 1);
arrays.push(myArray, 2);
arrays.push(myArray, 3);

// Get array information
var length = arrays.length(myArray);  // 3
var firstElement = arrays.at(myArray, 0);  // 1

// Modify array
var lastElement = arrays.pop(myArray);  // 3
arrays.set(myArray, 0, 10);  // Change first element to 10
```

## Core Array Functions

### `arrays.new()`
Creates a new empty array.

**Parameters:** None

**Returns:** New empty array

**Example:**
```neutron
var arr = arrays.new();
say("Empty array: " + arrays.to_string(arr));  // []
```

---

### `arrays.length(array)`
Returns the length of an array.

**Parameters:**
- `array`: The array to get length of

**Returns:** Number representing the length

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 1);
arrays.push(arr, 2);
var len = arrays.length(arr);
say("Length: " + len);  // 2
```

---

### `arrays.push(array, value)`
Adds an element to the end of an array.

**Parameters:**
- `array`: The array to add to
- `value`: The value to add

**Returns:** Nil (void)

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, "hello");
arrays.push(arr, "world");
say("Array: " + arrays.to_string(arr));  // ["hello", "world"]
```

---

### `arrays.pop(array)`
Removes and returns the last element of an array.

**Parameters:**
- `array`: The array to remove from

**Returns:** The removed element

**Throws:** Runtime error if array is empty

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 1);
arrays.push(arr, 2);

var last = arrays.pop(arr);
say("Last element: " + last);  // 2
say("Remaining: " + arrays.to_string(arr));  // [1]
```

---

### `arrays.at(array, index)`
Retrieves the element at a specific index.

**Parameters:**
- `array`: The array to access
- `index`: Zero-based index of the element

**Returns:** The element at the specified index

**Throws:** Runtime error if index is out of bounds

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, "first");
arrays.push(arr, "second");

var element = arrays.at(arr, 1);
say("Element at index 1: " + element);  // "second"
```

---

### `arrays.set(array, index, value)`
Sets the element at a specific index.

**Parameters:**
- `array`: The array to modify
- `index`: Zero-based index of the element to set
- `value`: The value to set

**Returns:** The value that was set

**Throws:** Runtime error if index is out of bounds

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 10);
arrays.push(arr, 20);

arrays.set(arr, 0, 100);
say("Modified array: " + arrays.to_string(arr));  // [100, 20]
```

## Advanced Array Methods

### `arrays.slice(array, start, end)`
Returns a shallow copy of a portion of an array.

**Parameters:**
- `array`: The array to slice
- `start`: Starting index (inclusive)
- `end`: Ending index (exclusive), optional (defaults to array length)

**Returns:** New array containing the sliced elements

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 1);
arrays.push(arr, 2);
arrays.push(arr, 3);
arrays.push(arr, 4);
arrays.push(arr, 5);

var subArr = arrays.slice(arr, 1, 4);
say("Sliced array: " + arrays.to_string(subArr));  // [2, 3, 4]
```

---

### `arrays.join(array, separator)`
Joins all elements of an array into a string.

**Parameters:**
- `array`: The array to join
- `separator`: String to use as separator, optional (defaults to ",")

**Returns:** String representation of the array elements

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, "apple");
arrays.push(arr, "banana");
arrays.push(arr, "cherry");

var joined = arrays.join(arr, ", ");
say("Joined: " + joined);  // "apple, banana, cherry"
```

---

### `arrays.reverse(array)`
Reverses the array in place.

**Parameters:**
- `array`: The array to reverse

**Returns:** The same array object (for chaining)

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 1);
arrays.push(arr, 2);
arrays.push(arr, 3);

arrays.reverse(arr);
say("Reversed: " + arrays.to_string(arr));  // [3, 2, 1]
```

---

### `arrays.sort(array)`
Sorts the array in place. Numbers are sorted before strings.

**Parameters:**
- `array`: The array to sort

**Returns:** The same array object (for chaining)

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 3);
arrays.push(arr, 1);
arrays.push(arr, 2);

arrays.sort(arr);
say("Sorted: " + arrays.to_string(arr));  // [1, 2, 3]
```

---

### `arrays.index_of(array, value)`
Returns the first index at which a given element can be found.

**Parameters:**
- `array`: The array to search in
- `value`: The value to search for

**Returns:** Index of the first occurrence, or -1 if not found

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, "a");
arrays.push(arr, "b");
arrays.push(arr, "c");

var index = arrays.index_of(arr, "b");
say("Index of 'b': " + index);  // 1
```

---

### `arrays.contains(array, value)`
Checks if an array contains a specific value.

**Parameters:**
- `array`: The array to check
- `value`: The value to search for

**Returns:** Boolean indicating whether the value exists in the array

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 1);
arrays.push(arr, 2);
arrays.push(arr, 3);

var hasTwo = arrays.contains(arr, 2);
var hasFive = arrays.contains(arr, 5);

say("Contains 2: " + hasTwo);  // true
say("Contains 5: " + hasFive);  // false
```

---

### `arrays.remove(array, value)`
Removes the first occurrence of a value from the array.

**Parameters:**
- `array`: The array to modify
- `value`: The value to remove

**Returns:** Boolean indicating if the value was found and removed

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 1);
arrays.push(arr, 2);
arrays.push(arr, 3);

var removed = arrays.remove(arr, 2);
say("Removed: " + removed);  // true
say("Array: " + arrays.to_string(arr));  // [1, 3]
```

---

### `arrays.remove_at(array, index)`
Removes the element at the specified index.

**Parameters:**
- `array`: The array to modify
- `index`: Index of the element to remove

**Returns:** The removed element

**Throws:** Runtime error if index is out of bounds

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, "first");
arrays.push(arr, "second");
arrays.push(arr, "third");

var removed = arrays.remove_at(arr, 1);
say("Removed: " + removed);  // "second"
say("Array: " + arrays.to_string(arr));  // ["first", "third"]
```

---

### `arrays.clear(array)`
Removes all elements from an array.

**Parameters:**
- `array`: The array to clear

**Returns:** The same array object (for chaining)

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 1);
arrays.push(arr, 2);

arrays.clear(arr);
say("Length after clear: " + arrays.length(arr));  // 0
```

---

### `arrays.clone(array)`
Creates a shallow copy of an array.

**Parameters:**
- `array`: The array to clone

**Returns:** New array with the same elements

**Example:**
```neutron
var original = arrays.new();
arrays.push(original, 1);
arrays.push(original, 2);

var cloned = arrays.clone(original);
arrays.push(cloned, 3);

say("Original: " + arrays.to_string(original));  // [1, 2]
say("Clone: " + arrays.to_string(cloned));      // [1, 2, 3]
```

---

### `arrays.to_string(array)`
Converts an array to its string representation.

**Parameters:**
- `array`: The array to convert

**Returns:** String representation of the array

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 1);
arrays.push(arr, "hello");

var str = arrays.to_string(arr);
say("Array as string: " + str);  // [1, hello]
```

---

## Utility Methods

### `arrays.flat(array)`
Creates a new array with all sub-array elements concatenated into it recursively up to the specified depth.

**Parameters:**
- `array`: The array to flatten

**Returns:** New flattened array

**Example:**
```neutron
var nested = arrays.new();
var inner = arrays.new();
arrays.push(inner, 2);
arrays.push(inner, 3);
arrays.push(nested, 1);
arrays.push(nested, inner);
arrays.push(nested, 4);

var flat = arrays.flat(nested);
// Result would be [1, 2, 3, 4] - not implemented in this simplified version
```

---

### `arrays.fill(array, value, start, end)`
Fills all the elements of an array from a start index to an end index with a static value.

**Parameters:**
- `array`: The array to fill
- `value`: Value to fill the array with
- `start`: Start index, optional (defaults to 0)
- `end`: End index, optional (defaults to array length)

**Returns:** The same array object for chaining

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 1);
arrays.push(arr, 2);
arrays.push(arr, 3);

arrays.fill(arr, 0);
say("Filled: " + arrays.to_string(arr));  // [0, 0, 0]
```

---

### `arrays.range(start, end, step)`
Creates an array of numbers following an arithmetic progression.

**Parameters:**
- `start`: Starting number
- `end`: Ending number (exclusive), optional (defaults to start)
- `step`: Step size, optional (defaults to 1)

**Returns:** New array with range values

**Example:**
```neutron
var range = arrays.range(1, 5);
say("Range: " + arrays.to_string(range));  // [1, 2, 3, 4]

var stepRange = arrays.range(0, 10, 2);
say("Step range: " + arrays.to_string(stepRange));  // [0, 2, 4, 6, 8]
```

---

### `arrays.shuffle(array)`
Randomly shuffles the elements of an array in place.

**Parameters:**
- `array`: The array to shuffle

**Returns:** The same array object for chaining

**Example:**
```neutron
var arr = arrays.new();
arrays.push(arr, 1);
arrays.push(arr, 2);
arrays.push(arr, 3);
arrays.push(arr, 4);

arrays.shuffle(arr);
// Array now has elements in random order
```

## Common Usage Patterns

### Array Building
```neutron
use arrays;

fun createNumberArray(start, end) {
    var arr = arrays.new();
    var current = start;
    while (current < end) {
        arrays.push(arr, current);
        current = current + 1;
    }
    return arr;
}

var numbers = createNumberArray(1, 6);
say("Numbers: " + arrays.to_string(numbers));  // [1, 2, 3, 4, 5]
```

### Array Transformation
```neutron
use arrays;

fun doubleNumbers(arr) {
    var result = arrays.clone(arr);
    var i = 0;
    var len = arrays.length(result);
    
    while (i < len) {
        var current = arrays.at(result, i);
        arrays.set(result, i, current * 2);
        i = i + 1;
    }
    return result;
}

var original = arrays.new();
arrays.push(original, 1);
arrays.push(original, 2);
arrays.push(original, 3);

var doubled = doubleNumbers(original);
say("Doubled: " + arrays.to_string(doubled));  // [2, 4, 6]
```

### Data Filtering
```neutron
use arrays;

fun filterEvenNumbers(arr) {
    var result = arrays.new();
    var i = 0;
    var len = arrays.length(arr);
    
    while (i < len) {
        var current = arrays.at(arr, i);
        if (current % 2 == 0) {
            arrays.push(result, current);
        }
        i = i + 1;
    }
    return result;
}

var numbers = arrays.range(1, 11);
var evens = filterEvenNumbers(numbers);
say("Evens: " + arrays.to_string(evens));  // [2, 4, 6, 8, 10]
```

### Error Handling
```neutron
use arrays;

fun safeGet(array, index) {
    if (index < 0 or index >= arrays.length(array)) {
        say("Index " + index + " is out of bounds");
        return nil;
    }
    return arrays.at(array, index);
}

var arr = arrays.new();
arrays.push(arr, "a");
arrays.push(arr, "b");

var valid = safeGet(arr, 1);     // "b"
var invalid = safeGet(arr, 5);   // nil, with error message
```

## Performance Considerations

- Array operations like push/pop at the end are efficient
- Array operations like insert/remove in the middle may require element shifting
- Clone operations create shallow copies of elements
- Consider using range for creating number sequences instead of manual loops

## Module Integration

The arrays module works seamlessly with other Neutron features:

```neutron
use arrays;
use fmt;

// Combine with fmt module for type checking
var arr = arrays.new();
arrays.push(arr, 42);
arrays.push(arr, "hello");

var firstType = fmt.type(arrays.at(arr, 0));
say("First element type: " + firstType);  // "number"
```