#!/usr/bin/env python3

# Mandelbrot Set benchmark in Python

def mandelbrot(c, max_iter):
    z = 0
    n = 0
    while abs(z) <= 2 and n < max_iter:
        z = z * z + c
        n += 1
    return n

def mandelbrot_set(xmin, xmax, ymin, ymax, width, height, max_iter):
    r1 = linspace(xmin, xmax, width)
    r2 = linspace(ymin, ymax, height)
    count = 0
    for r in r1:
        for i in r2:
            c = complex(r, i)
            z = mandelbrot(c, max_iter)
            if z == max_iter:
                count += 1
    return count

def linspace(start, stop, num):
    step = (stop - start) / (num - 1) if num > 1 else 0
    return [start + step * i for i in range(num)]

# Calculate mandelbrot set
width, height = 200, 200
xmin, xmax = -2.0, 1.0
ymin, ymax = -1.5, 1.5
max_iter = 100

count = mandelbrot_set(xmin, xmax, ymin, ymax, width, height, max_iter)
print(f"Mandelbrot points: {count}")
