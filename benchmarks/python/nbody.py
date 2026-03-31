#!/usr/bin/env python3

# N-Body Simulation benchmark in Python

import math

class Body:
    def __init__(self, x, y, vx, vy, mass):
        self.x = x
        self.y = y
        self.vx = vx
        self.vy = vy
        self.mass = mass

def simulate(bodies, dt, steps):
    for _ in range(steps):
        for i, body1 in enumerate(bodies):
            fx, fy = 0.0, 0.0
            for j, body2 in enumerate(bodies):
                if i != j:
                    dx = body2.x - body1.x
                    dy = body2.y - body1.y
                    dist_sq = dx * dx + dy * dy
                    dist = math.sqrt(dist_sq)
                    if dist > 0.1:
                        f = (body1.mass * body2.mass) / dist_sq
                        fx += f * dx / dist
                        fy += f * dy / dist
            
            body1.vx += fx / body1.mass * dt
            body1.vy += fy / body1.mass * dt
        
        for body in bodies:
            body.x += body.vx * dt
            body.y += body.vy * dt
    
    return bodies

# Create solar system-like setup
bodies = [
    Body(0, 0, 0, 0, 1000),
    Body(10, 0, 0, 10, 1),
    Body(20, 0, 0, 7, 2),
    Body(30, 0, 0, 5, 1.5),
]

# Run simulation
bodies = simulate(bodies, 0.01, 500)

# Output final energy
total_energy = sum(b.x * b.x + b.y * b.y for b in bodies)
print(f"Final energy: {total_energy}")
