import sys
import re
import raFile
import filterFile

path = sys.argv[1]
fname = sys.argv[2]
filter = list()

filter = filterFile.FilterFile()
filter.read(fname)

ra = raFile.RaFile()
ra.read(path)
newra = raFile.RaFile()

# create a dictionary to keep track of which RaEntries have what terms
keyDict = dict()
for entry in ra.iterValues():
    if entry == None:
        continue
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
for entry in filter.iterValues():
    matchDict[entry.getValueAt(0)] = list()

# a dictionary to keep track of user made groups based on filter matches
groupDict = dict()

for i in range(ra.count()):
    
    if ra.getKeyAt(i).startswith('#'):
        continue

    r = ra.getValueAt(i)
    for j in range(filter.count()):

        match = True
        f = filter.getValueAt(j) 
        
        if f.getKeyAt(0).startswith('_filter'):
            for k in range(f.count()):
                if f.getKeyAt(k).startswith('_'):
                    continue
                if r.getValue(f.getKeyAt(k)) == None or not re.match(f.getValueAt(k), r.getValue(f.getKeyAt(k))):
                    match = False
                    break

            if match == True:
                matchDict[f.getValueAt(0)].append(ra.getKeyAt(i))
                for m in f.iterMatches():
                    if m[1] == 'add':
                        if m[0] not in groupDict:
                            groupDict[m[0]] = list()
                        #print 'adding ' + ra.getKeyAt(i) + ' to ' +  m[0]
                        groupDict[m[0]].append(ra.getKeyAt(i))
                    elif m[1] == 'remove':
                        if m[0] in groupDict:
                            #print 'removing ' + ra.getKeyAt(i) + ' from ' +  m[0]
                            groupDict[m[0]].remove(ra.getKeyAt(i))

#print groupDict

for j in range(filter.count()):
    f = filter.getValueAt(j)
    if f.getKeyAt(0).startswith('_check'):
        checkDict = dict()
        #print ra
        for entry in ra.iterValues():
            elem = entry.getValue(f.getValue('_key'))
            #print entry.getValueAt(0)
            if elem not in checkDict:
                checkDict[elem] = list()
            checkDict[elem].append(entry.getValueAt(0))
        #print checkDict
        for key in checkDict.iterkeys():
            if checkDict[key] == None:
                continue
            entry = checkDict[key]
            if len(entry) > 1:
                for m in f.iterMatches():
                    if m[1] == 'add':
                        #print entry
                        hashstr = m[0] + ' (' + str(ra.getValue(entry[0]).getValue(f.getValue('_key'))) + ')'
                        if hashstr not in groupDict:
                            groupDict[hashstr] = list()
                        groupDict[hashstr].extend(entry)
                    elif m[1] == 'remove':
                        if hashstr in groupDict:
                            for rem in entry:
                                groupDict[hashstr].remove(rem)

for i in groupDict.iterkeys():
    s = ''
    print s + i + ' (' + str(len(groupDict[i])) + '):'
    for j in groupDict[i]:
        print j
    print ''

