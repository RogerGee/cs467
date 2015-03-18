import sys
import random
import string

if len(sys.argv) <= 1:
    print "usage: python knapsack-inst.py <number-of-items>"
    exit(1)

number = int(sys.argv[1])
if number <= 0:
    print "enter positive number of items"
    exit(1)
lower = number/2 if number!=1 else 1
upper = number * 5

random.seed()
limit = random.randint(number*5,number*15)

table = set()

print limit
for i in range(number):
    while True:
        s = ""
        for j in range(random.randint(1,1+number/26)):
            s += random.choice(string.ascii_lowercase)
        if not s in table:
            break
    table.add(s)

    c = random.randint(lower,upper)
    v = random.randint(lower,upper)

    print s+","+str(c)+","+str(v)
