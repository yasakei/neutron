import time

a = 0
b = 0
c = 0

for i in range(5000000):
    a = i & 0xFFFF
    b = i | 0xAAAA
    c = a ^ b
    c = ~c
    c = c << 2
    c = c >> 1

print(f"Bitwise result: {c}")
