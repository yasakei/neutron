// N-Body Simulation benchmark in JavaScript

class Body {
    constructor(x, y, vx, vy, mass) {
        this.x = x;
        this.y = y;
        this.vx = vx;
        this.vy = vy;
        this.mass = mass;
    }
}

function simulate(bodies, dt, steps) {
    for (let step = 0; step < steps; step++) {
        // Calculate forces
        for (let i = 0; i < bodies.length; i++) {
            const body1 = bodies[i];
            let fx = 0, fy = 0;

            for (let j = 0; j < bodies.length; j++) {
                if (i !== j) {
                    const body2 = bodies[j];
                    const dx = body2.x - body1.x;
                    const dy = body2.y - body1.y;
                    const distSq = dx * dx + dy * dy;
                    const dist = Math.sqrt(distSq);

                    if (dist > 0.1) {
                        const f = (body1.mass * body2.mass) / distSq;
                        fx += f * dx / dist;
                        fy += f * dy / dist;
                    }
                }
            }

            body1.vx += fx / body1.mass * dt;
            body1.vy += fy / body1.mass * dt;
        }

        // Update positions
        for (const body of bodies) {
            body.x += body.vx * dt;
            body.y += body.vy * dt;
        }
    }

    return bodies;
}

// Create solar system-like setup
const bodies = [
    new Body(0, 0, 0, 0, 1000),
    new Body(10, 0, 0, 10, 1),
    new Body(20, 0, 0, 7, 2),
    new Body(30, 0, 0, 5, 1.5),
];

// Run simulation
simulate(bodies, 0.01, 500);

// Output final positions
let totalEnergy = 0;
for (const b of bodies) {
    totalEnergy += b.x * b.x + b.y * b.y;
}
console.log(`Final energy: ${totalEnergy}`);
