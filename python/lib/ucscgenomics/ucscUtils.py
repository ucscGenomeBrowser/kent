#!/hive/groups/encode/dcc/bin/python
import sys, string, os, re, argparse, subprocess, math

def isGbdbFile(file, table, database):
    errors = []
    if os.path.isfile("/gbdb/%s/bbi/%s" % (database, file)):
        return 1
    else:
        cmd = "hgsql %s -e \"select fileName from (%s)\"" % (database, table)
        p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
        cmdoutput = p.stdout.read()
        if os.path.isfile(cmdoutput.split("\n")[1]):
            return 1
        else:
            return 0
            
def makeFileSizes(inlist, path=None):
    checklist = list()

    for i in inlist:
    	if path:
            checklist.append("%s/%s" % (path, i))    
        else:
            checklist.append(i)
    filesizes = 0
    for i in checklist:
        realpath = os.path.realpath(i)
        filesizes = filesizes + int(os.path.getsize(realpath))

    filesizes = math.ceil(float(filesizes) / (1024**2))

    return int(filesizes)
    
def printIter(set, path=None):
    output = []
    for i in sorted(set):
        if path:
            output.append("%s/%s" % (path, i))
        else:
            output.append("%s" % (i))
    return output

def zeros(m,n):
    cross = list()
    for i in range(m):
        cross.append(list())
        for j in range(n):
            cross[i].append(0)
    return cross

def mergeList(list1,list2):
    """ Takes in two lists, returns a single list merged into a consensus list using a modified Needleman-Wunsch
        alignment. The comment lines with two comments are there to signalify a change to Smith-Waterman, remove the 
        double commented lines to convert.
        Single comment lines are for debugging, remove when this routine matures.
    """

    m,n = len(list1),len(list2)
    score = zeros(m+1,n+1)
    pointer = zeros(m+1,n+1)
    penalty = -1
    max_i = 0
    max_j = 0
    maxScore = 0
     
    #print "len_i = %s, len_j = %s" % (m,n)
    for i in range(1,m+1):
        for j in range(1,n+1):
            scoreUp = score[i-1][j]+penalty
            scoreDown = score[i][j-1]+penalty
            scoreDiagonal = score[i-1][j-1]
            if list1[i-1] == list2[j-1]:
                scoreDiagonal = scoreDiagonal + 10
            score[i][j] = max(0,scoreUp,scoreDown,scoreDiagonal)
            if score[i][j] == 0:
                pointer[i][j] = 0
            if score[i][j] == scoreUp:
                pointer[i][j] = 1
            if score[i][j] == scoreDown:
                pointer[i][j] = 2
            if score[i][j] == scoreDiagonal:
                pointer[i][j] = 3
            ##if score[i][j] >= maxScore:
            ##    max_i = i
            ##    max_j = j
            ##    maxScore = score[i][j]

    #for k in pointer:
    #    line = ""
    #    for l in k:
    #        line = line + "%s " % l
    #    print line

    align1,align2 = list(),list()
    ##after_i,after_j = max_i,max_j
    #print "max_i = %s, max_j = %s" % (max_i, max_j)
    while pointer[i][j] != 0:
        if pointer[i][j] == 3:
            align1.append(str(list1[i-1]))
            align2.append(str(list2[j-1]))
            i = i-1
            j = j-1
        elif pointer[i][j] == 2:
            align1.append('-')
            align2.append(str(list2[j-1]))
            j = j-1
        elif pointer[i][j] == 1:
            align1.append(str(list1[i-1]))
            align2.append('-')
            i = i-1

    before_i,before_j = i,j
    align1.reverse()
    align2.reverse()
    consensus = list()

    #for i in range(len(align1)):
    #    print "%s = %s" % (align1[i], align2[i])

    for i in range(len(align1)):
        if align1[i] == align2[i]:
            consensus.append(align1[i])
        elif align1[i] == "-":
            consensus.append(align2[i])
        elif align2[i] == "-":
            consensus.append(align1[i])
        elif align1[i] != align2[i]:
            consensus.append(align1[i])
            consensus.append(align2[i])
    before = list()
    if before_i != 0:
        before.extend(list1[0:before_i])
    if before_j != 0:
        before.extend(list2[0:before_j])
    ##if after_i < len(list1):
    ##    consensus.extend(list1[after_i:])
    ##if after_j < len(list2):
    ##    consensus.extend(list2[after_j:])
    before.extend(consensus)
    setcon = list()
    p = re.compile('^#')
    p2 = re.compile('^\s*$')
    for i in before:
        if p.match(i) or p2.match(i):
            setcon.append(i)
            continue
        if i in setcon:
            continue
        else:
            setcon.append(i)
    
    return setcon
