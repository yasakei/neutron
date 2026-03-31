#!/usr/bin/env python3

# Ray Tracer benchmark in Python
# Simple ray-sphere intersection - single sphere at origin

def sqrt_approx(x):
    if x <= 0:
        return 0
    guess = x / 2
    for _ in range(20):
        guess = (guess + x / guess) / 2
    return guess

def sphere_intersect(sx, sy, sz, sr, ox, oy, oz, dx, dy, dz):
    ocx = ox - sx
    ocy = oy - sy
    ocz = oz - sz
    
    a = dx * dx + dy * dy + dz * dz
    b = 2 * (ocx * dx + ocy * dy + ocz * dz)
    c = ocx * ocx + ocy * ocy + ocz * ocz - sr * sr
    
    disc = b * b - 4 * a * c
    if disc < 0:
        return -1
    return (-b - sqrt_approx(disc)) / (2 * a)

def render(width, height):
    pixels_hit = 0
    
    # Single sphere at origin with radius 10
    sx, sy, sz, sr = 0, 0, 0, 10
    
    for y in range(height):
        for x in range(width):
            # Normalized device coordinates
            nx = (x / width - 0.5) * 2
            ny = (y / height - 0.5) * 2
            
            # Ray from (0,0,-20) in direction (nx, ny, 1) normalized
            dir_len = sqrt_approx(nx * nx + ny * ny + 1)
            rdx = nx / dir_len
            rdy = ny / dir_len
            rdz = 1 / dir_len
            
            t = sphere_intersect(sx, sy, sz, sr, 0, 0, -20, rdx, rdy, rdz)
            if t > 0.001:
                pixels_hit += 1
    
    return pixels_hit

# Render
width, height = 100, 100
pixels = render(width, height)
print(f"Rendered {width}x{height}, lit pixels: {pixels}")
