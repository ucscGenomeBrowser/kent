import sys
import raFile

if len(sys.argv) != 3:
    print 'Usage Error. use python test.py file key'
    sys.exit(2)

file = sys.argv[1]
key = sys.argv[2]
ra = raFile.RaFile()
ra.read(file, key)
ra.write()
#ra.writeEntry('valueC')
