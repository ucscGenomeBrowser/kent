# wordUse - count words in file and print top ten
import sys

def usage():
    """explain usage and exit"""
    sys.exit("""wordUse - count words in file and print top ten 
usage:
    wordUse files""")

class wordCount:
    """Count number of uses of word."""
    def __init__(self, word):
        self.word = word
	self.count = 0
    def __cmp__(self,other):
        return other.count - self.count

   
def wordUse(file):
    """Count words in file and print top ten"""
    f = open(file)
    dict = {}
    while 1:
        line = f.readline()
	if line == '':
	    break
	words = line.split()
	for word in words:
	    if not dict.has_key(word):
		wc = wordCount(word)
		dict[word] = wc
	    else:
	        wc = dict[word]
	    wc.count += 1;
    list = []
    for key in dict.keys():
        list.append(dict[key])
    list.sort()
    for i in range(10):
	wc = list[i]
        print "%s\t%d" % (wc.word, wc.count)

if (len(sys.argv) != 2):
   usage()
wordUse(sys.argv[1])
