#!/hive/groups/encode/dcc/bin/python
import sys, os, re, argparse, subprocess, math
from ucscgenomics import ra, track, qa

class makeNotes(object):
    def checkMetaDbForFiles(self, status, state):
        if state == 'new':
            (mdb, files, loose) = (self.newMdb, self.newReleaseFiles, self.loose)
        elif state == 'old':
            (mdb, files, loose) = (self.oldMdb, self.oldReleaseFiles, self.loose)

        errors = []
        revokedset = set()
        revokedfiles = set()
        atticset = set()
        supplementalset = set()
        filtermdb = ra.RaFile()

        for i in files:
            if re.match('supplemental', i):
                supplementalset.add(i)
            if not re.match('wgEncode.*', i):
                continue

            filestanza = mdb.filter(lambda s: re.match(".*%s.*" % i,s['fileName']), lambda s: s)
            #should only return 1, just in case
            if filestanza:
                for j in filestanza:
                    filtermdb[j.name] = j
                    if 'objStatus' in j and re.search('revoked|replaced|renamed', j['objStatus']):
                        revokedfiles.add(i)
                        revokedset.add(j.name)
                    if 'attic' in j:
                        atticset.add(j.name)
            else:
                #pass
                if loose and re.match('.*bai', i):
                    pass
                else:
                    errors.append("metaDb: %s is not mentioned in %s" % (i, status))

        return (filtermdb, revokedset, revokedfiles, atticset, supplementalset, errors)

    def __checkAlphaForDropped(self, status, type):
        (new, old) = (self.newMdb, self.oldMdb)
        errors=[]
        diff = set(old) -set(new)
        for i in diff:
            errors.append("%s: %s missing from %s" % (type, i, status))
        return errors

    def __checkFilesForDropped(self):
        (new, old) = (self.newReleaseFiles, self.oldReleaseFiles)
        diff = set(old) - set(new)
        return diff

    def checkTableStatus(self, status, state):
        errors=[]
        revokedset = set()
        (database, composite, loose) = (self.database, self.composite, self.loose)
        if state == 'new':
            (mdb, files, revokedset) = (self.newMdb, self.newReleaseFiles, self.revokedSet)
        elif state == 'old':
            (mdb, files) = (self.oldMdb, self.oldReleaseFiles)
        #home = os.environ['HOME']
        #dbhost = ''
        #dbuser = ''
        #dbpassword = ''
        #p = re.compile('db.(\S+)=(\S+)')
        #with open("%s/.hg.conf" % home) as f:
        #    for line in f:
        #        line.rstrip("\n\r")
        #        if p.match(line):
        #            m = p.match(line)
        #            if m.groups(1)[0] == 'host':
        #                dbhost = m.groups(1)[1]
        #            if m.groups(1)[0] == 'user':
        #                dbuser = m.groups(1)[1]
        #            if m.groups(1)[0] == 'password':
        #                dbpassword = m.groups(1)[1]

        #db = MySQLdb.connect (host = dbhost,
        #            user = dbuser,
        #            passwd = dbpassword,
        #            db = database)

        #cursor = db.cursor ()
        #cursor.execute ("show tables like '%s%s'" % (composite, "%"))
        #tableset = set(cursor.fetchall())

        mdbtableset = set(mdb.filter(lambda s: s['objType'] == 'table' and 'tableName' in s and 'attic' not in s, lambda s: s['metaObject']))
        mdbtableset = mdbtableset - revokedset
        mdbtableset = set(mdb.filter(lambda s: s['metaObject'] in mdbtableset, lambda s: s['tableName']))
        revokedtableset = set(mdb.filter(lambda s: s['metaObject'] in revokedset, lambda s: s['tableName']))
        sep = "','"
        tablestr = sep.join(mdbtableset)
        tablestr = "'" + tablestr + "'"

        #this should really be using python's database module, but I'd need admin access to install it
        #at this point, I am just parsing the output form hgsql
        cmd = "hgsql %s -e \"select table_name from information_schema.TABLES where table_name in (%s)\"" % (database, tablestr)
        p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
        cmdoutput = p.stdout.read()

        sqltableset = set(cmdoutput.split("\n")[1:-1])

        missingTableNames = set(mdb.filter(lambda s: s['objType'] == 'table' and 'tableName' not in s and 'attic' not in s, lambda s: s['metaObject']))

        missingFromDb = mdbtableset - sqltableset

        if missingTableNames:
            for i in missingTableNames:
                errors.append("table: %s is type obj, but missing tableName field called by %s" % (i, status))

        if missingFromDb:
            for i in missingFromDb:
                errors.append("table: %s table not found in Db called by %s" % (i, status))

        return (mdbtableset, revokedtableset, errors)


    def getGbdbFiles(self, state):
        database = self.database
        revokedset = set()
        if state == 'new':
            (tableset, revokedset, mdb) = (self.newTableSet, self.revokedSet, self.newMdb)
        elif state == 'old':
            (tableset, mdb) = (self.oldTableSet, self.oldMdb)

        errors = []

        gbdbtableset = qa.getGbdbTables(self.database, tableset)

        revokedtableset = qa.getGbdbTables(self.database, revokedset)

        file1stanzalist = mdb.filter(lambda s: s['tableName'] in gbdbtableset, lambda s: s)
        revokedstanzalist = mdb.filter(lambda s: s['tableName'] in revokedtableset, lambda s: s)
        gbdbfileset = set()
        revokedfileset = set()

        for i in file1stanzalist:
            filelist = i['fileName'].split(',')
            for j in filelist:
                if os.path.isfile("/gbdb/%s/bbi/%s" % (database, j)):
                    gbdbfileset.add(j)
                else:
                    errors.append("gbdb: %s does not exist in /gbdb/%s/bbi" % (j, database))

        for i in revokedstanzalist:
            filelist = i['fileName'].split(',')
            for j in filelist:
                if os.path.isfile("/gbdb/%s/bbi/%s" % (database, j)):
                    revokedfileset.add(j)
                else:
                    errors.append("gbdb: revoked gbdb %s does not exist in /gbdb/%s/bbi" % (j, database))

        return (gbdbfileset, revokedfileset, errors)

    def __getTableSize(self):
        (mdbtableset, database) = (self.newTableSet, self.database)
        tablesize = float(0)
        tablelist = list()
        for i in mdbtableset:
            tablelist.append("table_name = '%s'" % i)
        orsep = " OR "
        orstr = orsep.join(tablelist)

        cmd = "hgsql %s -e \"SELECT ROUND(data_length/1024/1024,2) total_size_mb, ROUND(index_length/1024/1024,2) total_index_size_mb FROM information_schema.TABLES WHERE %s\"" % (database, orstr)
        p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
        cmdoutput = p.stdout.read()
        for i in cmdoutput.split("\n")[1:-1]:
            fields = i.split()
            for j in fields:
                tablesize = tablesize + float(j)
        return math.ceil(tablesize)

    def __checkMd5sums(self):
        (newfiles, oldfiles, loose) = (self.newReleaseFiles, self.oldReleaseFiles, self.loose)
        errors = []
        for i in oldfiles:
            if i not in newfiles:
                pass
            elif re.match('wgEncode.*', i):
                if oldfiles[i].md5sum != newfiles[i].md5sum:
                    errors.append("file: %s have changed md5sums between releases. %s vs %s" % (i, oldfiles[i].md5sum, newfiles[i].md5sum))
        if loose:
            return list()
        else:
            return errors

    def __makeFileSizes(self, c, args, inlist):
        checklist = list()
        for i in inlist:
            checklist.append("%s/%s" % (c.downloadsDirectory + 'release' + args['releaseNew'], i))    
        filesizes = 0
        for i in checklist:
            realpath = os.path.realpath(i)
            filesizes = filesizes + int(os.path.getsize(realpath))

        filesizes = math.ceil(float(filesizes) / (1024**2))
        return int(filesizes)

    def __cleanSpecialFiles(self, inlist):
        specialRemoveList = ['md5sum.history']
        for i in specialRemoveList:
            if i in inlist:
                inlist.remove(i)

        return(inlist)

    def __separateOutAdditional(self):
        (oldReleaseFiles, totalFiles, newSupplementalSet, oldSupplementalSet) = (self.oldTotalFiles, self.totalFiles, self.newSupplementalSet, self.oldSupplementalSet)
        additionalList = set()
        oldAdditionalList = set()
        newTotal = set()
        newOld = set()
        for i in totalFiles:
            if i in newSupplementalSet:
                continue
            elif not re.match('wgEncode.*', i):
                additionalList.add(i)
            else:
                newTotal.add(i)
        for i in oldReleaseFiles:
            if not re.match('wgEncode.*', i):
                if i in totalFiles:
                    pass
                elif i in newSupplementalSet:
                    continue
                else:
                    oldAdditionalList.add(i)
            else:
                newOld.add(i)

        oldReleaseFiles = newOld

        return(newOld, additionalList, oldAdditionalList, newTotal)

    def __printWithPath(self, set, c, release):
        output = []
        for i in sorted(set):
            output.append("%s/%s" % (c.httpDownloadsPath + 'release' + release, i))
        return output
    def __printGbdbPath(self, set, database):
        output = []
        for i in sorted(set):
            output.append("/gbdb/%s/bbi/%s" % (database, i))
        return output

    def __printIter(self, inlist):
        output = []
        for i in sorted(inlist):
            output.append(i)
        return output

    def printReport(self, args, c):
        (totalFiles, newGbdbSet, newTableSet, additionalList, oldAdditionalList, pushTables, pushFiles, pushGbdbs, oldTableSet, oldReleaseFiles, oldGbdbSet, atticSet, revokedFiles, revokedTableSet, revokedGbdbs, missingFiles, newSupplementalSet, oldSupplementalSet, tableSize) = (self.totalFiles, self.newGbdbSet, self.newTableSet, self.additionalList, self.oldAdditionalList, self.pushTables, self.pushFiles, self.pushGbdbs, self.oldTableSet, self.oldTotalFiles, self.oldGbdbSet, self.atticSet, self.revokedFiles, self.revokedTableSet, self.revokedGbdbs, self.missingFiles, self.newSupplementalSet, self.oldSupplementalSet, self.tableSize)
        #the groups here need to be predefined, I just copied and pasted after working out what they were
        sep = "\n"
        output = []
        output.append("mkChangeNotes v2")
        output.append("%s %s Release %s vs Release %s" % (args['database'], args['composite'], args['releaseNew'], args['releaseOld']))
        output.append("")
        output.append("QA Count Summaries for Release %s:" % args['releaseNew'])
        output.append("Tables: %d" % int(len(newTableSet)))
        output.append("Files: %d" % int(len(totalFiles - revokedFiles)))
        output.append("Gbdbs: %d" % int(len(newGbdbSet)))
        output.append("Supplemental: %d" % int(len(newSupplementalSet - oldSupplementalSet)))
        output.append("Other: %d" % int(len(additionalList)))
        output.append("\n")
        output.append("Sizes of New:")

        totalsize = 0
        size = 0
        tableGb = int(tableSize/1024)
        totalsize = totalsize + tableSize
        if tableGb > 1:
            output.append("Tables: %d MB (%d GB)" % (tableSize, tableGb))
        elif tableSize:
            output.append("Tables: %d MB" % tableSize)

        size = int(self.__makeFileSizes(c, args, pushFiles))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Files: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Files: %d MB" % size)
        
        size = int(self.__makeFileSizes(c, args, pushGbdbs))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Gbdbs: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Gbdbs: %d MB" % size)
        
        size = int(self.__makeFileSizes(c, args, (newSupplementalSet - oldSupplementalSet)))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Supplemental: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Supplemental: %d MB" % size)
        
        size = int(self.__makeFileSizes(c, args, (additionalList)))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Other: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Other: %d MB" % size)
      
        if int(totalsize/1024) > 1:
            output.append("Total: %d MB (%d GB)" % (totalsize, int(totalsize/1024)))
        else:
            output.append("Total: %d MB" % totalsize)

        tableprint = len(newTableSet | oldTableSet | revokedTableSet)
        self.newTables = set(pushTables)
        if tableprint:
            output.append("\n")
            output.append("TABLES:")
            output.append("New: %s" % len(pushTables))
            output.append("Untouched: %s" % len(oldTableSet & newTableSet))
            output.append("Revoked/Replaced/Renamed: %s" % len(revokedTableSet))
            output.append("New + Untouched: %s" % len(newTableSet))
            output.append("Total (New + Untouched + Revoked/Replaced/Renamed): %s" % len(newTableSet | oldTableSet | revokedTableSet))
        if tableprint and not args['summary']:
            output.append("")
            output.append("New Tables (%s):" % len(pushTables))
            output.extend(self.__printIter(pushTables))
            output.append("")
            output.append("Untouched (%s):" % len(oldTableSet & newTableSet))
            output.extend(self.__printIter(oldTableSet & newTableSet))
            output.append("")
            output.append("Revoked/Replaced/Renamed Tables (%s):" % len(revokedTableSet))
            output.extend(self.__printIter(revokedTableSet))

        dlprint = len(totalFiles | oldReleaseFiles | revokedFiles)
        self.newFiles = set(self.__printWithPath((pushFiles - revokedFiles), c, args['releaseNew']))
        if dlprint:
            output.append("\n")
            #downlaodables = total - revoked
            output.append("DOWNLOAD FILES:")
            output.append("New: %s" % len(pushFiles - revokedFiles))
            output.append("Untouched: %s" % len((totalFiles & oldReleaseFiles) - revokedFiles))
            output.append("Revoked/Replaced/Renamed: %s" % len(revokedFiles))
            output.append("New + Untouched: %s" % len((pushFiles - revokedFiles) | ((totalFiles & oldReleaseFiles) - revokedFiles)))
            output.append("Total (New + Untouched + Revoked/Replaced/Renamed): %s" % len(totalFiles | oldReleaseFiles | revokedFiles))
        if dlprint and not args['summary']:
            output.append("")
            output.append("New Download Files (%s):" % len(pushFiles - revokedFiles))
            output.extend(sorted(list(self.newFiles)))
            output.append("")
            output.append("Untouched Download Files (%s):" % len((totalFiles & oldReleaseFiles) - revokedFiles))
            output.extend(self.__printWithPath(((totalFiles & oldReleaseFiles) - revokedFiles), c, args['releaseNew']))
            output.append("")
            output.append("Revoked/Replaced/Renamed Download Files (%s):" % len(revokedFiles))
            output.extend(self.__printWithPath(revokedFiles, c, args['releaseNew']))

        gbdbprint = len(newGbdbSet | oldGbdbSet | revokedGbdbs) 
        self.newGbdbs = set(self.__printGbdbPath(pushGbdbs, args['database']))
        if gbdbprint:
            output.append("\n")
            output.append("GBDBS:")
            output.append("New: %s" % len(pushGbdbs))
            output.append("Untouched: %s" % len((newGbdbSet & oldGbdbSet) - revokedGbdbs))
            output.append("Revoked/Replaced/Renamed: %s" % len(revokedGbdbs))
            output.append("New + Untouched: %s" % len(pushGbdbs | ((newGbdbSet & oldGbdbSet) - revokedGbdbs)))
            output.append("Total (New + Untouched + Revoked/Replaced/Renamed): %s" % len(newGbdbSet | oldGbdbSet | revokedGbdbs))
        if gbdbprint and not args['summary']:
            output.append("")
            output.append("New Gbdb Files (%s):" % len(pushGbdbs))
            output.extend(sorted(list(self.newGbdbs)))
            output.append("")
            output.append("Untouched Gbdb Files (%s):" % len((newGbdbSet & oldGbdbSet) - revokedGbdbs))
            output.extend(self.__printGbdbPath((newGbdbSet & oldGbdbSet) - revokedGbdbs, args['database']))
            output.append("")
            output.append("Revoked/Replaced/Renamed Gbdb Files (%s):" % len(revokedGbdbs))
            output.extend(self.__printGbdbPath(revokedGbdbs, args['database']))
            
        supplementalprint = len(newSupplementalSet | oldSupplementalSet)
        self.newSupplemental = set(self.__printWithPath(newSupplementalSet - oldSupplementalSet, c, args['releaseNew']))
        if supplementalprint:
            output.append("\n")
            output.append("SUPPLEMENTAL FILES:")
            output.append("New: %s" % len(newSupplementalSet - oldSupplementalSet))
            output.append("Untouched: %s" % len(oldSupplementalSet & newSupplementalSet))
            output.append("Removed: %s" % len(oldSupplementalSet - newSupplementalSet))
            output.append("New + Untouched: %s" % len((newSupplementalSet - oldSupplementalSet) | (oldSupplementalSet & newSupplementalSet)))
            output.append("Total: %s" % len(newSupplementalSet | oldSupplementalSet))
        if supplementalprint and not args['summary']:
            output.append("")
            output.append("New Supplemental Files (%s):" % len(newSupplementalSet - oldSupplementalSet))
            output.extend(sorted(list(self.newSupplemental)))
            output.append("")
            output.append("Untouched Supplemental Files (%s):" % len(oldSupplementalSet & newSupplementalSet))
            output.extend(self.__printWithPath(oldSupplementalSet & newSupplementalSet, c, args['releaseNew']))
            output.append("")
            output.append("Removed Supplemental Files (%s):" % len(oldSupplementalSet - newSupplementalSet))
            output.extend(self.__printWithPath(oldSupplementalSet - newSupplementalSet, c, args['releaseNew']))
            
        otherprint = len(additionalList | oldAdditionalList)
        self.newOthers = set(self.__printWithPath(additionalList, c, args['releaseNew']))
        if otherprint:
            output.append("\n")
            output.append("OTHER FILES:")
            output.append("New: %s" % len(additionalList))
            output.append("Revoked/Replace: %s" % len(oldAdditionalList - additionalList))
            output.append("Total: %s" % len(additionalList | oldAdditionalList))
        if otherprint and not args['summary']:
            output.append("")
            output.append("New Other Files (%s):" % len(additionalList))
            output.extend(sorted(list(self.newOthers)))
            output.append("")
            output.append("Revoked Other Files (%s):" % len(oldAdditionalList - additionalList))
            output.extend(self.__printWithPath((oldAdditionalList - additionalList), c, args['releaseNew']))
        output.append("\n")
        
        if len(missingFiles):
            output.append("Files that dropped between releases (%s):" % len(missingFiles))
            output.extend(self.__printWithPath(missingFiles, c, args['releaseOld']))
            output.append("\n")
        
        if not args['ignore']:
            output.append("No Errors")
        return output

    def printReportOne(self, args, c):
        (totalFiles, revokedFiles, newGbdbSet, revokedGbdbs, newTableSet, revokedTables, additionalList, atticSet, newSupplementalSet, tableSize) = (self.totalFiles, self.revokedFiles, self.newGbdbSet, self.revokedGbdbs, self.newTableSet, self.revokedTableSet, self.additionalList, self.atticSet, self.newSupplementalSet, self.tableSize)
        output = []
        output.append("mkChangeNotes v2")
        output.append("%s %s Release %s" % (args['database'], args['composite'], args['releaseNew']))
        output.append("")
        output.append("QA Count Summaries for Release %s:" % args['releaseNew'])
        output.append("Tables: %d" % int(len(newTableSet - revokedTables)))
        output.append("Files: %d" % int(len(totalFiles - revokedFiles)))
        output.append("Gbdbs: %d" % int(len(newGbdbSet - revokedGbdbs)))
        output.append("Supplemental: %d" % int(len(newSupplementalSet)))
        output.append("Other: %d" % int(len(additionalList)))
        output.append("")
        output.append("REVOKED:")
        output.append("Tables: %s" % len(revokedTables))
        output.append("Files: %s" % len(revokedFiles))
        output.append("Gbdbs: %s" % len(revokedGbdbs))
        output.append("\n")
        totalsize = 0;
        output.append("Sizes of New:")
        tableGb = int(tableSize / 1024)
        if tableGb > 1:
            output.append("Tables: %d MB (%d GB)" % (tableSize, tableGb))
        else:
            output.append("Tables: %d MB" % tableSize)
        totalsize = totalsize + tableSize
        size = int(self.__makeFileSizes(c, args, totalFiles - revokedFiles))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Files: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Files: %d MB" % size)
        size = int(self.__makeFileSizes(c, args, newGbdbSet - revokedGbdbs))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Gbdbs: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Gbdbs: %d MB" % size)
        size = int(self.__makeFileSizes(c, args, newSupplementalSet))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Supplemental: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Supplemental: %d MB" % size)
        size = int(self.__makeFileSizes(c, args, (additionalList)))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Other: %d MB" % size)
        else:
            output.append("Other: %d MB" % size)
        if int(totalsize/1024) > 1:
            output.append("Total: %d MB (%d GB)" % (totalsize, int(totalsize/1024)))
        else:
            output.append("Total: %d MB" % totalsize)
        output.append("\n")
        self.newTables = set(self.__printIter(newTableSet - revokedTables))
        self.newFiles = set(self.__printWithPath(totalFiles - revokedFiles, c, args['releaseNew']))
        self.newGbdbs = set(self.__printGbdbPath(newGbdbSet - revokedGbdbs, args['database']))
        self.newSupplemental = set(self.__printWithPath(newSupplementalSet, c, args['releaseNew']))
        self.newOthers = set(self.__printWithPath(additionalList, c, args['releaseNew']))
        if not args['summary']:
            output.append("")
            if len(newTableSet - revokedTables):
                output.append("New Tables (%s):" % len(self.newTables))
                output.extend(sorted(list(self.newTables)))
                output.append("\n")
            if len(totalFiles - revokedFiles):
                output.append("New Download Files (%s):" % len(self.newFiles))
                output.extend(sorted(list(self.newFiles)))
                output.append("\n")
            if len(newGbdbSet - revokedGbdbs):
                output.append("New Gbdb Files (%s):" % len(newGbdbSet - revokedGbdbs))
                output.extend(sorted(list(self.newGbdbs)))
                output.append("\n")
            if len(newSupplementalSet):
                output.append("New Supplemental Files (%s):" % len(newSupplementalSet))
                output.extend(sorted(list(self.newSupplemental)))
                output.append("\n")
            if len(additionalList):
                output.append("New Other Files (%s):" % len(additionalList))
                output.extend(sorted(list(self.newOthers)))
                output.append("\n")
            if len(revokedTables):
                output.append("Revoked Tables (%s):" % len(revokedTables))
                output.extend(self.__printIter(revokedTables))
                output.append("\n")
            if len(revokedFiles):
                output.append("Revoked Files (%s):" % len(revokedFiles))
                output.extend(self.__printWithPath(revokedFiles, c, args['releaseNew']))
                output.append("\n")
            if len(revokedGbdbs):
                output.append("Revoked Gbdbs (%s):" % len(revokedGbdbs))
                output.extend(self.__printGbdbPath(revokedGbdbs, args['database']))
                output.append("\n")
        if not args['ignore']:
            output.append("No Errors")
        return output

    def printErrors(self, errors):
        errorsDict = {}
        output = []
        for i in errors:
            line = i.split(":", 1)
            try:
                errorsDict[line[0]].append(line[1])
            except:
                errorsDict[line[0]] = []
                errorsDict[line[0]].append(line[1])
        output.append("Errors (%s):" % len(errors))
        for i in sorted(errorsDict.keys()):
            output.append("%s:" % i)
            for j in sorted(errorsDict[i]):
                output.append("%s" % j)
        return output

    def __init__(self, args):
        self.releaseNew = args['releaseNew']
        self.releaseOld = args['releaseOld']
        self.database = args['database']
        self.composite = args['composite']
        self.loose = args['loose']
        self.ignore = args['ignore']
        self.summary = args['summary']
        self.specialMdb = args['specialMdb']
        self.args = args

        errors = []
        c = track.CompositeTrack(self.database, self.composite, None, self.specialMdb)

        #sanitize arguments
        if not self.releaseOld.isdigit():
            self.releaseOld = 'solo'
        elif int(self.releaseOld) <= 0:
            self.releaseOlf = 'solo'
        elif self.releaseOld > self.releaseNew:
            self.releaseOld = 'solo'

        if int(self.releaseNew) > 1 and str(self.releaseOld) != 'solo':

            self.newReleaseFiles = c.releases[int(self.releaseNew)-1]
            self.oldReleaseFiles = c.releases[int(self.releaseOld)-1]

            self.newMdb = c.alphaMetaDb
            self.oldMdb = c.publicMetaDb

            #check if all files listed in release directories have associated metaDb entries
            (self.newMdb, self.revokedSet, self.revokedFiles, self.atticSet, self.newSupplementalSet, newFileErrors) = self.checkMetaDbForFiles("alpha metaDb", "new")
            (self.oldMdb, spam, eggs, ham, self.oldSupplementalSet, oldFileErrors) = self.checkMetaDbForFiles("public metaDb", "old")
            errors.extend(newFileErrors)
            errors.extend(oldFileErrors)

            #checks to see that nothing has disappeared between public and alpha
            errors.extend(self.__checkAlphaForDropped("alpha metaDb", "stanza"))
            self.missingFiles = self.__checkFilesForDropped()
            errors.extend(self.__checkMd5sums())

            #checks and gets tables that are present, also returns a revoked set of tables for new
            (self.newTableSet, self.revokedTableSet, newTableError) = self.checkTableStatus("alpha metaDb", "new")
            (self.oldTableSet, spam, oldTableError) = self.checkTableStatus("public metaDb", "old")
            errors.extend(newTableError)
            errors.extend(oldTableError)

            #same as above except for gbdbs
            (self.newGbdbSet, self.revokedGbdbs, newGbdbError) = self.getGbdbFiles("new")
            (self.oldGbdbSet, eggs, oldGbdbError) = self.getGbdbFiles("old")
            errors.extend(newGbdbError)
            errors.extend(oldGbdbError)

            #for ease of typing
            totalFiles = set(self.newReleaseFiles)
            oldTotalFiles = set(self.oldReleaseFiles)

            #these could honestly be moved earlier, get a file list processing section or something
            #they clean out special fiels out and separated the master fiels list into the 3 required
            #ones: wgEncode, supplemental and additional.
            self.totalFiles = self.__cleanSpecialFiles(totalFiles)
            self.oldTotalFiles = self.__cleanSpecialFiles(oldTotalFiles)
            (self.oldTotalFiles, self.additionalList, self.oldAdditionalList, self.totalFiles) = self.__separateOutAdditional()

            #get the stuff you need to push, also table sizes        
            self.pushTables = set(sorted((self.newTableSet - self.oldTableSet)))
            self.pushFiles = set(sorted((self.totalFiles - self.oldTotalFiles)))
            self.pushGbdbs = set(sorted((self.newGbdbSet - self.oldGbdbSet)))
            self.tableSize = self.__getTableSize()
            
            #don't output.append(report unless ignore option is on or no errors
            if (not errors) or self.ignore:
                self.output = self.printReport(args, c)
            else:
                self.output = self.printErrors(errors)


        elif self.releaseOld == 'solo':

            self.newReleaseFiles = c.releases[int(self.releaseNew)-1]

            self.newMdb = c.alphaMetaDb

            (self.newMdb, self.revokedSet, self.revokedFiles, self.atticSet, self.newSupplementalSet, newFileErrors) = self.checkMetaDbForFiles("alpha metaDb", "new")
            errors.extend(newFileErrors)

            (self.newTableSet, self.revokedTableSet, newTableError) = self.checkTableStatus("alpha metaDb", "new")
            errors.extend(newTableError)

            self.tableSize = self.__getTableSize()

            (self.newGbdbSet, self.revokedGbdbs, newGbdbError) = self.getGbdbFiles("new")
            errors.extend(newGbdbError)

            #set for easy operations
            totalFiles = set(self.newReleaseFiles)

            #clean out special fiels we don't push i.e. md5sum.history
            self.totalFiles = self.__cleanSpecialFiles(totalFiles)

            #makes list for additional files
            (self.oldTotalFiles, self.oldSupplementalSet) = (set(), set())
            (self.oldReleaseFiles, self.additionalList, self.oldAdditionalList, self.totalFiles) = self.__separateOutAdditional()
            if (not errors) or self.ignore:
                self.output = self.printReportOne(args, c) 
            else:
                self.output = self.printErrors(errors)
