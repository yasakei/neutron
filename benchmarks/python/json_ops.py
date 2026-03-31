#!/usr/bin/env python3

# JSON Parse/Generate benchmark in Python

import json

# Complex data structure
def create_test_data():
    return {
        "users": [
            {
                "id": i,
                "name": f"User_{i}",
                "email": f"user{i}@example.com",
                "active": i % 2 == 0,
                "scores": [i * 10, i * 20, i * 30],
                "profile": {
                    "age": 20 + i % 50,
                    "city": f"City_{i % 10}",
                    "country": f"Country_{i % 5}"
                }
            }
            for i in range(100)
        ],
        "metadata": {
            "version": "1.0",
            "count": 100,
            "tags": ["tag1", "tag2", "tag3", "tag4", "tag5"]
        }
    }

# Create test data
data = create_test_data()

# Serialize to JSON (multiple times)
json_strings = []
for _ in range(50):
    json_str = json.dumps(data)
    json_strings.append(json_str)

# Parse JSON (multiple times)
parsed_count = 0
total_id_sum = 0
for json_str in json_strings:
    parsed = json.loads(json_str)
    parsed_count += 1
    for user in parsed["users"]:
        total_id_sum += user["id"]

print(f"Parsed: {parsed_count} JSON objects, Total ID sum: {total_id_sum}")
