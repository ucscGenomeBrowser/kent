# countLines - Count lines in files.
import sys
sys.path.append("/cluster/home/jsp/prog/utillities/py_modules")
import psyco
psyco.full()

def usage():
    """explain usage and exit"""
    sys.exit("""countLines - count lines in files
usage:
    countLines files[s]""")

def countFile(file):
    """Count lines in one file"""
    count = 0;
    f = open(file)
    while 1:
        line = f.readline()
	if line == '':
	    break
	count += 1
    print "total: %d" % count

if (len(sys.argv) != 2):
   usage()
countFile(sys.argv[1])
