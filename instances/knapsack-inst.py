import sys
import random
import string

if len(sys.argv) <= 1:
    print "usage: python knapsack-inst.py <number-of-items>"
    exit(1)

number = int(sys.argv[1])

random.seed()
limit = random.randint(number,number*10)

print limit
for i in range(number):
    s = ""
    for j in range(random.randint(5,10)):
        s += random.choice(string.ascii_lowercase)

    c = random.randint(number/2,number*5)
    v = random.randint(number/2,number*5)

    print s+","+str(c)+","+str(v)
