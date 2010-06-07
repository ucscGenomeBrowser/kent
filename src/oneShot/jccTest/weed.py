# Weed out things from the dictionary that make it
# produce boring results - abbreviations and single
# letters mostly.

import sys

def anyVowel(s):
    for c in s:
        if c in "aeiouI":
	    return True
    return False

def allCap(s):
    for c in s:
        if not c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
	    return False
    return True

s = open(sys.argv[1])
while 1:
    line = s.readline()
    if len(line) == 0:
        break
    words = line.split()
    for word in words:
	word.replace('-','')
        if len(word) > 1 and anyVowel(word) and not allCap(word):
	   print word
print 'I'
print 'a'
