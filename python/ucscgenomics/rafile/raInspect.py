import sys
import re
import raFile
import filterFile

path = sys.argv[1]
fname = sys.argv[2]
filter = list()

filter = filterFile.FilterFile()
filter.read(fname, '_name')

ra = raFile.RaFile()
ra.read(path, 'term')
newra = raFile.RaFile()

# create a dictionary to keep track of which RaEntries have what terms
keyDict = dict()
for entry in ra.iterValues():
    for elem in entry.iterKeys():
        if elem not in keyDict:
            keyDict[elem] = list()
        keyDict[elem].append(entry.getValueAt(0))

cellDict = dict()
for entry in ra.iterValues():
    elem = entry.getValue('cell')
    if elem not in keyDict:
        cellDict[elem] = list()
    cellDict[elem].append(entry.getValueAt(0))

#print cellDict

# create a dictionary to keep track of what entries match what filters
matchDict = dict()
for entry in ra.iterValues():
    matchDict[entry.getValueAt(0)] = list()

# a dictionary to keep track of user made groups based on filter matches
groupDict = dict()

for i in range(ra.count()):
    
    r = ra.getValueAt(i)
    for j in range(filter.count()):

        match = True
        f = filter.getValueAt(j) 
        print f
        for k in f:
            if k[0].startswith('_'):
                continue
            print k
            print k[1] + ', ' + str(r.getValue(k[0]))
            if r.getValue(k[0]) == None or not re.match(k[1], r.getValue(k[0])):
                match = False
                break

        if match == True:
            newra.add(ra.getKeyAt(i), r)
            matchDict[f[0][1]].append(ra.getKeyAt(i))

            for m in f.Match:
                if m[1] == 'add':
                    groupDict[m[0]].append(ra.getKeyAt(i))
                elif m[1] == 'remove':
                    groupDict[m[0]].remove(ra.getKeyAt(i))

            break

print matchDict
#newra.write()
