# Buffers

Buffers are raw byte arrays used for handling binary data. They are created using the `sys.alloc(size)` function.

## Creating a Buffer

```js
use sys;

// Create a buffer of 10 bytes, initialized to 0
var buf = sys.alloc(10);
```

## Accessing and Modifying Data

Buffers can be accessed and modified using array indexing syntax `[]`. Values must be integers between 0 and 255.

```js
buf[0] = 255;
buf[1] = 128;

var val = buf[0]; // 255
```

## Bounds Checking

Accessing indices outside the buffer's range will throw a runtime error. This error can be caught using a `try-catch` block. If uncaught, it will terminate the program.

```js
try {
    buf[100] = 1; 
} catch (e) {
    print(e); // "Buffer index out of bounds."
}
```

## Value Validation

Assigning values outside the range 0-255 will throw a runtime error. This error can be caught using a `try-catch` block.

```js
try {
    buf[0] = 256;
} catch (e) {
    print(e); // "Buffer value must be a byte (0-255)."
}
```
