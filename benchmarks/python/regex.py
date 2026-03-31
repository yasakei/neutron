#!/usr/bin/env python3

# Regex Matching benchmark in Python

# Test data
test_strings = [
    "Contact us at support@example.com or sales@company.org",
    "Call 123-456-7890 or 987-654-3210",
    "Server IP: 192.168.1.1 or 10.0.0.255",
    "Visit https://example.com or http://test.org/page",
    "Hello World This Is A Test String Here",
]

# Email pattern matching (count @)
def match_email(text):
    count = 0
    for c in text:
        if c == "@":
            count += 1
    return count

# Phone pattern matching (xxx-xxx-xxxx)
def match_phone(text):
    count = 0
    i = 0
    while i < len(text) - 11:
        if text[i + 3] == "-" and text[i + 7] == "-":
            count += 1
            i += 12
        else:
            i += 1
    return count

# URL pattern matching
def match_url(text):
    return 1 if "http" in text else 0

# Capitalized word matching
def match_capitalized(text):
    count = 0
    for c in text:
        if "A" <= c <= "Z":
            count += 1
    return count

# Run all matches
total_matches = 0
for _ in range(100):
    for text in test_strings:
        total_matches += match_email(text)
        total_matches += match_phone(text)
        total_matches += match_url(text)
        total_matches += match_capitalized(text)

print(f"Total matches: {total_matches}")
