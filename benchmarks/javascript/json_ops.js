// JSON Parse/Generate benchmark in JavaScript

// Complex data structure
function createTestData() {
    const users = [];
    for (let i = 0; i < 100; i++) {
        users.push({
            id: i,
            name: `User_${i}`,
            email: `user${i}@example.com`,
            active: i % 2 === 0,
            scores: [i * 10, i * 20, i * 30],
            profile: {
                age: 20 + i % 50,
                city: `City_${i % 10}`,
                country: `Country_${i % 5}`
            }
        });
    }
    
    return {
        users: users,
        metadata: {
            version: "1.0",
            count: 100,
            tags: ["tag1", "tag2", "tag3", "tag4", "tag5"]
        }
    };
}

// Create test data
const data = createTestData();

// Serialize to JSON (multiple times)
const jsonStrings = [];
for (let i = 0; i < 50; i++) {
    const jsonStr = JSON.stringify(data);
    jsonStrings.push(jsonStr);
}

// Parse JSON (multiple times)
let parsedCount = 0;
let totalIdSum = 0;

for (const jsonStr of jsonStrings) {
    const parsed = JSON.parse(jsonStr);
    parsedCount++;
    
    for (const user of parsed.users) {
        totalIdSum += user.id;
    }
}

console.log(`Parsed: ${parsedCount} JSON objects, Total ID sum: ${totalIdSum}`);
