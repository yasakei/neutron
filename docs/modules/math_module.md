# Math Module Documentation

The `math` module provides essential mathematical operations and functions for Neutron programs.

## Usage

```neutron
use math;

// Now you can use math functions
var result = math.add(10, 5);
var power = math.pow(2, 8);
```

## Arithmetic Operations

### `math.add(a, b)`
Adds two numbers together.

**Parameters:**
- `a` (number): First number
- `b` (number): Second number

**Returns:** Number representing the sum of a and b

**Example:**
```neutron
use math;

var sum = math.add(15, 25);
say("15 + 25 = " + sum); // Output: 15 + 25 = 40

var decimal = math.add(3.14, 2.86);
say("3.14 + 2.86 = " + decimal); // Output: 3.14 + 2.86 = 6
```

**Throws:** Runtime error if arguments are not numbers

---

### `math.subtract(a, b)`
Subtracts the second number from the first.

**Parameters:**
- `a` (number): Number to subtract from
- `b` (number): Number to subtract

**Returns:** Number representing a minus b

**Example:**
```neutron
use math;

var difference = math.subtract(50, 20);
say("50 - 20 = " + difference); // Output: 50 - 20 = 30

var negative = math.subtract(10, 15);
say("10 - 15 = " + negative); // Output: 10 - 15 = -5
```

**Throws:** Runtime error if arguments are not numbers

---

### `math.multiply(a, b)`
Multiplies two numbers together.

**Parameters:**
- `a` (number): First number
- `b` (number): Second number

**Returns:** Number representing the product of a and b

**Example:**
```neutron
use math;

var product = math.multiply(7, 8);
say("7 * 8 = " + product); // Output: 7 * 8 = 56

var decimal = math.multiply(2.5, 4);
say("2.5 * 4 = " + decimal); // Output: 2.5 * 4 = 10
```

**Throws:** Runtime error if arguments are not numbers

---

### `math.divide(a, b)`
Divides the first number by the second.

**Parameters:**
- `a` (number): Dividend (number to be divided)
- `b` (number): Divisor (number to divide by)

**Returns:** Number representing a divided by b

**Example:**
```neutron
use math;

var quotient = math.divide(20, 4);
say("20 / 4 = " + quotient); // Output: 20 / 4 = 5

var decimal = math.divide(7, 3);
say("7 / 3 = " + decimal); // Output: 7 / 3 = 2.33333...
```

**Throws:** 
- Runtime error if arguments are not numbers
- Runtime error if attempting to divide by zero

---

## Advanced Mathematical Functions

### `math.pow(base, exponent)`
Raises a number to the power of another number.

**Parameters:**
- `base` (number): The base number
- `exponent` (number): The exponent

**Returns:** Number representing base raised to the power of exponent

**Example:**
```neutron
use math;

var square = math.pow(5, 2);
say("5^2 = " + square); // Output: 5^2 = 25

var cube = math.pow(3, 3);
say("3^3 = " + cube); // Output: 3^3 = 27

var fractional = math.pow(16, 0.5);
say("16^0.5 = " + fractional); // Output: 16^0.5 = 4 (square root)

var negative = math.pow(2, -3);
say("2^-3 = " + negative); // Output: 2^-3 = 0.125
```

**Throws:** Runtime error if arguments are not numbers

---

### `math.sqrt(number)`
Calculates the square root of a number.

**Parameters:**
- `number` (number): The number to find the square root of

**Returns:** Number representing the square root

**Example:**
```neutron
use math;

var root = math.sqrt(144);
say("√144 = " + root); // Output: √144 = 12

var decimal = math.sqrt(2);
say("√2 = " + decimal); // Output: √2 = 1.41421...

var perfect = math.sqrt(49);
say("√49 = " + perfect); // Output: √49 = 7
```

**Throws:** Runtime error if argument is not a number

**Note:** The square root of negative numbers is not supported and may produce undefined results.

---

### `math.abs(number)`
Returns the absolute value (non-negative value) of a number.

**Parameters:**
- `number` (number): The number to get the absolute value of

**Returns:** Number representing the absolute value

**Example:**
```neutron
use math;

var positive = math.abs(-17);
say("|-17| = " + positive); // Output: |-17| = 17

var unchanged = math.abs(25);
say("|25| = " + unchanged); // Output: |25| = 25

var zero = math.abs(0);
say("|0| = " + zero); // Output: |0| = 0
```

**Throws:** Runtime error if argument is not a number

## Common Usage Patterns

### Calculator Operations
```neutron
use math;

fun calculate(operation, a, b) {
    if (operation == "add") {
        return math.add(a, b);
    } else if (operation == "subtract") {
        return math.subtract(a, b);
    } else if (operation == "multiply") {
        return math.multiply(a, b);
    } else if (operation == "divide") {
        return math.divide(a, b);
    } else if (operation == "power") {
        return math.pow(a, b);
    }
    return 0;
}

var result = calculate("multiply", 6, 7);
say("Result: " + result); // Output: Result: 42
```

### Mathematical Formulas
```neutron
use math;

// Pythagorean theorem: c = √(a² + b²)
fun pythagorean(a, b) {
    var aSquared = math.pow(a, 2);
    var bSquared = math.pow(b, 2);
    var sum = math.add(aSquared, bSquared);
    return math.sqrt(sum);
}

var hypotenuse = pythagorean(3, 4);
say("Hypotenuse: " + hypotenuse); // Output: Hypotenuse: 5
```

### Distance Calculations
```neutron
use math;

// Distance between two points: √((x₂-x₁)² + (y₂-y₁)²)
fun distance(x1, y1, x2, y2) {
    var dx = math.subtract(x2, x1);
    var dy = math.subtract(y2, y1);
    var dxSquared = math.pow(dx, 2);
    var dySquared = math.pow(dy, 2);
    var sum = math.add(dxSquared, dySquared);
    return math.sqrt(sum);
}

var dist = distance(0, 0, 3, 4);
say("Distance: " + dist); // Output: Distance: 5
```

### Statistics and Averages
```neutron
use math;

// Calculate average of multiple values
fun average(values) {
    // Note: This is a simplified example
    // In practice, you'd iterate through an array
    var sum = math.add(values[0], values[1]);
    sum = math.add(sum, values[2]);
    return math.divide(sum, 3);
}

// Calculate standard deviation
fun standardDeviation(mean, values) {
    var variance = 0;
    // Simplified calculation for demonstration
    var diff1 = math.subtract(values[0], mean);
    var diff2 = math.subtract(values[1], mean);
    var diff3 = math.subtract(values[2], mean);
    
    variance = math.add(variance, math.pow(diff1, 2));
    variance = math.add(variance, math.pow(diff2, 2));
    variance = math.add(variance, math.pow(diff3, 2));
    
    variance = math.divide(variance, 3);
    return math.sqrt(variance);
}
```

### Financial Calculations
```neutron
use math;

// Compound interest: A = P(1 + r)^t
fun compoundInterest(principal, rate, time) {
    var onePlusRate = math.add(1, rate);
    var compound = math.pow(onePlusRate, time);
    return math.multiply(principal, compound);
}

var finalAmount = compoundInterest(1000, 0.05, 10);
say("Final amount: $" + finalAmount); // Output: Final amount: $1628.89
```

### Geometric Calculations
```neutron
use math;

// Area of a circle: π × r²
fun circleArea(radius) {
    var pi = 3.14159;
    var radiusSquared = math.pow(radius, 2);
    return math.multiply(pi, radiusSquared);
}

// Volume of a sphere: (4/3) × π × r³
fun sphereVolume(radius) {
    var pi = 3.14159;
    var fourThirds = math.divide(4, 3);
    var radiusCubed = math.pow(radius, 3);
    var temp = math.multiply(pi, radiusCubed);
    return math.multiply(fourThirds, temp);
}

var area = circleArea(5);
var volume = sphereVolume(5);
say("Circle area: " + area);
say("Sphere volume: " + volume);
```

## Error Handling

All math functions throw runtime errors when provided with invalid arguments:

```neutron
use math;

// Safe division with zero check
fun safeDivide(a, b) {
    if (b == 0) {
        say("Error: Division by zero!");
        return nil;
    }
    return math.divide(a, b);
}

var result = safeDivide(10, 0); // Will print error message
if (result != nil) {
    say("Result: " + result);
}
```

## Mathematical Constants

Since Neutron doesn't have built-in constants, you can define commonly used mathematical constants:

```neutron
use math;

// Mathematical constants
var PI = 3.141592653589793;
var E = 2.718281828459045;
var GOLDEN_RATIO = 1.618033988749895;

// Using constants in calculations
fun circleCircumference(radius) {
    return math.multiply(math.multiply(2, PI), radius);
}

var circumference = circleCircumference(10);
say("Circumference: " + circumference);
```

## Performance Considerations

- All operations are performed using double-precision floating-point arithmetic
- Very large numbers may lose precision
- For integer-only operations, results may include decimal places (e.g., 5/2 = 2.5)
- The `abs()` function currently works with integer conversion, which may affect precision for very large decimals

## Compatibility

The math module is available in both interpreter mode and compiled binaries, providing consistent mathematical operations across all execution modes of Neutron programs.
