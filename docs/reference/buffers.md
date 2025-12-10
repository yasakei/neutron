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

Accessing indices outside the buffer's range will result in a runtime error and terminate the program.

```js
// This will cause a runtime error
// buf[100] = 1; 
```

## Value Validation

Assigning values outside the range 0-255 will result in a runtime error and terminate the program.

```js
// This will cause a runtime error
// buf[0] = 256;
```
