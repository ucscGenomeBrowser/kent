from collections import OrderedDict

mtrFile = open("/hive/data/outside/mtr/mtrflatfile_2.0.txt",'r')
aWig = open("/hive/data/outside/mtr/a.wig",'w')
cWig = open("/hive/data/outside/mtr/c.wig",'w')
gWig = open("/hive/data/outside/mtr/g.wig",'w')
tWig = open("/hive/data/outside/mtr/t.wig",'w')

def writeOutToWig(positionDic,fileNames):
    for key in positionDic.keys():
        try: prevPos
        except:
            prevPos = False
        currentPos = key
        for base, fileName in zip(['A','C','T','G'], fileNames):
            if prevPos == False or int(currentPos)-1 != int(prevPos):
                fileName.write("fixedStep chrom=%s start=%s step=1 span=1\n" % (positionDic[key]['chr'],currentPos))
                if positionDic[key][base] == [] or min(positionDic[key][base]) == '':
                    fileName.write('0\n')
                else:
                    fileName.write(str(min(positionDic[key][base]))+"\n")
            else:
                if positionDic[key][base] == [] or min(positionDic[key][base]) == '':
                    fileName.write('0\n')
                else:
                    fileName.write(str(min(positionDic[key][base]))+"\n")                    
        prevPos = currentPos

for line in mtrFile:
    if line.startswith("Chromosome_name"):
        print("Skipped")
    else:
        try: prevChrom
        except:
            prevChrom = False
        lineSplit = line.split("\t")
        chrom = "chr"+lineSplit[0]
        pos = lineSplit[1]
        alt = lineSplit[3]
        mtr = lineSplit[0]
    
        if not prevChrom:
            positionDic = OrderedDict()
        
        elif chrom != prevChrom:
            print("Writing chrom: "+prevChrom)
            writeOutToWig(positionDic,[aWig,cWig,tWig,gWig])
            positionDic = OrderedDict()
    
        if pos not in positionDic.keys():
            positionDic[pos] = {}
            positionDic[pos]['C'] = []
            positionDic[pos]['A'] = []
            positionDic[pos]['G'] = []
            positionDic[pos]['T'] = []
            positionDic[pos]['chr'] = chrom
        
        positionDic[pos][alt].append(lineSplit[10])
        prevChrom = chrom

print("Writing chrom: "+prevChrom)
writeOutToWig(positionDic,[aWig,cWig,tWig,gWig])
                
mtrFile.close()
aWig.close()
cWig.close()
tWig.close()
gWig.close()
