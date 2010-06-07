# Generate 1000 random phone numbers.
import random

s = "0123456789"
for i in range(1000):
   print "(",
   for j in range(3):
      print random.choice(s),
   print ") ",
   for j in range(3):
      print random.choice(s),
   print "-",
   for j in range(4):
      print random.choice(s),
   print
