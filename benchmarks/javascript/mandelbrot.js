// Mandelbrot Set benchmark in JavaScript

function mandelbrot(cx, cy, maxIter) {
    let zx = 0;
    let zy = 0;
    let n = 0;
    
    while (zx * zx + zy * zy <= 4 && n < maxIter) {
        const tmp = zx * zx - zy * zy + cx;
        zy = 2 * zx * zy + cy;
        zx = tmp;
        n++;
    }
    return n;
}

function linspace(start, stop, num) {
    const step = num > 1 ? (stop - start) / (num - 1) : 0;
    const result = [];
    for (let i = 0; i < num; i++) {
        result.push(start + step * i);
    }
    return result;
}

function mandelbrotSet(xmin, xmax, ymin, ymax, width, height, maxIter) {
    const r1 = linspace(xmin, xmax, width);
    const r2 = linspace(ymin, ymax, height);
    let count = 0;
    
    for (let i = 0; i < width; i++) {
        for (let j = 0; j < height; j++) {
            const c = mandelbrot(r1[i], r2[j], maxIter);
            if (c === maxIter) {
                count++;
            }
        }
    }
    return count;
}

// Calculate mandelbrot set
const width = 200;
const height = 200;
const xmin = -2.0;
const xmax = 1.0;
const ymin = -1.5;
const ymax = 1.5;
const maxIter = 100;

const count = mandelbrotSet(xmin, xmax, ymin, ymax, width, height, maxIter);
console.log(`Mandelbrot points: ${count}`);
