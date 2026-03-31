// Ray Tracer benchmark in JavaScript
// Simple ray-sphere intersection - single sphere at origin

function sqrtApprox(x) {
    if (x <= 0) return 0;
    let guess = x / 2;
    for (let i = 0; i < 20; i++) {
        guess = (guess + x / guess) / 2;
    }
    return guess;
}

function sphereIntersect(sx, sy, sz, sr, ox, oy, oz, dx, dy, dz) {
    const ocx = ox - sx;
    const ocy = oy - sy;
    const ocz = oz - sz;
    
    const a = dx * dx + dy * dy + dz * dz;
    const b = 2 * (ocx * dx + ocy * dy + ocz * dz);
    const c = ocx * ocx + ocy * ocy + ocz * ocz - sr * sr;
    
    const disc = b * b - 4 * a * c;
    if (disc < 0) {
        return -1;
    }
    return (-b - sqrtApprox(disc)) / (2 * a);
}

function render(width, height) {
    let pixelsHit = 0;
    
    // Single sphere at origin with radius 10
    const sx = 0, sy = 0, sz = 0, sr = 10;
    
    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            // Normalized device coordinates
            const nx = (x / width - 0.5) * 2;
            const ny = (y / height - 0.5) * 2;
            
            // Ray from (0,0,-20) in direction (nx, ny, 1) normalized
            const dirLen = sqrtApprox(nx * nx + ny * ny + 1);
            const rdx = nx / dirLen;
            const rdy = ny / dirLen;
            const rdz = 1 / dirLen;
            
            const t = sphereIntersect(sx, sy, sz, sr, 0, 0, -20, rdx, rdy, rdz);
            if (t > 0.001) {
                pixelsHit++;
            }
        }
    }
    
    return pixelsHit;
}

// Render
const width = 100;
const height = 100;
const pixels = render(width, height);
console.log(`Rendered ${width}x${height}, lit pixels: ${pixels}`);
