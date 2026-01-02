import time

class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y
    
    def dist(self):
        return self.x * self.x + self.y * self.y

sum_val = 0

for i in range(100000):
    p = Point(i, i + 1)
    sum_val += p.dist()

print(f"Object result: {sum_val}")
