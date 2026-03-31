// Regex Matching benchmark in JavaScript

// Test data
const testStrings = [
    "Contact us at support@example.com or sales@company.org",
    "Call 123-456-7890 or 987-654-3210",
    "Server IP: 192.168.1.1 or 10.0.0.255",
    "Visit https://example.com or http://test.org/page",
    "Hello World This Is A Test String Here",
];

// Email pattern matching (count @)
function matchEmail(text) {
    let count = 0;
    for (let i = 0; i < text.length; i++) {
        if (text[i] === "@") {
            count++;
        }
    }
    return count;
}

// Phone pattern matching (xxx-xxx-xxxx)
function matchPhone(text) {
    let count = 0;
    for (let i = 0; i < text.length - 11; i++) {
        if (text[i + 3] === "-" && text[i + 7] === "-") {
            count++;
            i += 12;
        }
    }
    return count;
}

// URL pattern matching
function matchURL(text) {
    return text.includes("http") ? 1 : 0;
}

// Capitalized word matching
function matchCapitalized(text) {
    let count = 0;
    for (let i = 0; i < text.length; i++) {
        const c = text[i];
        if (c >= "A" && c <= "Z") {
            count++;
        }
    }
    return count;
}

// Run all matches
let totalMatches = 0;
for (let reps = 0; reps < 100; reps++) {
    for (const text of testStrings) {
        totalMatches += matchEmail(text);
        totalMatches += matchPhone(text);
        totalMatches += matchURL(text);
        totalMatches += matchCapitalized(text);
    }
}

console.log(`Total matches: ${totalMatches}`);
