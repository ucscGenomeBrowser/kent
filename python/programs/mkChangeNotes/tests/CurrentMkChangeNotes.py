#!/hive/groups/encode/dcc/bin/python
import sys, os, re, argparse, subprocess, math
from ucscgenomics import ra, track

class makeNotes(object):
    def checkMetaDbForFiles(self, mdb, files, status, loose):
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

    def checkAlphaForDropped(self, new, old, status, type):
        errors=[]
        diff = set(old) -set(new)
        for i in diff:
            errors.append("%s: %s missing from %s" % (type, i, status))
        return errors

    def checkFilesForDropped(self, new, old):
        diff = set(old) - set(new)
        return diff

    def checkTableStatus(self, mdb, files, database, composite, status, loose, revokedset):
        errors=[]

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
        output = p.stdout.read()

        sqltableset = set(output.split("\n")[1:])

        missingTableNames = set(mdb.filter(lambda s: s['objType'] == 'table' and 'tableName' not in s and 'attic' not in s, lambda s: s['metaObject']))

        missingFromDb = mdbtableset - sqltableset

        if missingTableNames:
            for i in missingTableNames:
                errors.append("table: %s is type obj, but missing tableName field called by %s" % (i, status))

        if missingFromDb:
            for i in missingFromDb:
                errors.append("table: %s table not found in Db called by %s" % (i, status))

        return (mdbtableset, revokedtableset, errors)

    def getGbdbFiles(self, database, tableset, revokedset, mdb):
        errors = []
        sep = "','"
        tablestr = sep.join(tableset)
        tablestr = "'" + tablestr + "'"
        revokestr = sep.join(revokedset)
        revokestr = "'" + revokestr + "'"

        cmd = "hgsql %s -e \"select table_name from information_schema.columns where table_name in (%s) and column_name = 'fileName'\"" % (database, tablestr)
        p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
        output = p.stdout.read()

        gbdbtableset = set(output.split("\n")[1:])

        cmd = "hgsql %s -e \"select table_name from information_schema.columns where table_name in (%s) and column_name = 'fileName'\"" % (database, revokestr)
        p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
        output = p.stdout.read()

        revokedtableset = set(output.split("\n")[1:])

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

    def getTableSize(self, mdbtableset, database):
        tablesize = float(0)
        tablelist = list()
        for i in mdbtableset:
            tablelist.append("table_name = '%s'" % i)
        orsep = " OR "
        orstr = orsep.join(tablelist)

        cmd = "hgsql %s -e \"SELECT ROUND(data_length/1024/1024,2) total_size_mb, ROUND(index_length/1024/1024,2) total_index_size_mb FROM information_schema.TABLES WHERE table_name = %s\"" % (database, orstr)
        p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
        output = p.stdout.read()
        for i in output.split("\n")[1:]:
            fields = i.split()
            for j in fields:
                tablesize = tablesize + float(j)
        return math.ceil(tablesize)

    def checkMd5sums(self, newfiles, oldfiles, loose):
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

    def makeFileSizes(self, c, args, inlist):
        checklist = list()
        for i in inlist:
            checklist.append("%s/%s" % (c.downloadsDirectory + 'release' + args['releaseNew'], i))    
        filesizes = 0
        for i in checklist:
            realpath = os.path.realpath(i)
            filesizes = filesizes + int(os.path.getsize(realpath))

        filesizes = math.ceil(float(filesizes) / (1024**2))
        return int(filesizes)

    def cleanSpecialFiles(self, inlist):
        specialRemoveList = ['md5sum.history']
        for i in specialRemoveList:
            if i in inlist:
                inlist.remove(i)

        return(inlist)

    def separateOutAdditional(self, oldReleaseFiles, totalFiles, newSupplementalSet, oldSupplementalSet):
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

    def printWithPath(self, set, c, release):
        output = []
        for i in sorted(set):
            output.append("%s/%s" % (c.httpDownloadsPath + 'release' + release, i))
        return output
    def printGbdbPath(self, set, database):
        output = []
        for i in sorted(set):
            output.append("/gbdb/%s/bbi/%s" % (database, i))
        return output

    def printIter(self, inlist):
        output = []
        for i in sorted(inlist):
            output.append(i)
        return output

    def printReport(self, args, totalFiles, newGbdbSet, newTableSet, additionalList, oldAdditionalList, pushTables, pushFiles, pushGbdbs, c, oldTableSet, oldReleaseFiles, oldGbdbSet, atticSet, revokedFiles, mdb, revokedTableSet, revokedGbdbs, missingFiles, newSupplementalSet, oldSupplementalSet, tableSize):
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
        totalsize = 0
        size = 0
        output.append("Sizes of New:")
        tableGb = int(tableSize/1024)
        if tableGb > 1:
            output.append("Tables: %d MB (%d GB)" % (tableSize, tableGb))
        elif tableSize:
            output.append("Tables: %d MB" % tableSize)
        totalsize = totalsize + tableSize
        size = int(self.makeFileSizes(c, args, pushFiles))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Files: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Files: %d MB" % size)
        size = int(self.makeFileSizes(c, args, pushGbdbs))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Gbdbs: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Gbdbs: %d MB" % size)
        size = int(self.makeFileSizes(c, args, (newSupplementalSet - oldSupplementalSet)))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Supplemental: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Supplemental: %d MB" % size)
        size = int(self.makeFileSizes(c, args, (additionalList)))
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
            output.extend(self.printIter(pushTables))
            output.append("")
            output.append("Untouched (%s):" % len(oldTableSet & newTableSet))
            output.extend(self.printIter(oldTableSet & newTableSet))
            output.append("")
            output.append("Revoked/Replaced/Renamed Tables (%s):" % len(revokedTableSet))
            output.extend(self.printIter(revokedTableSet))
        dlprint = len(totalFiles | oldReleaseFiles | revokedFiles)
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
            output.extend(self.printWithPath((pushFiles - revokedFiles), c, args['releaseNew']))
            output.append("")
            output.append("Untouched Download Files (%s):" % len((totalFiles & oldReleaseFiles) - revokedFiles))
            output.extend(self.printWithPath(((totalFiles & oldReleaseFiles) - revokedFiles), c, args['releaseNew']))
            output.append("")
            output.append("Revoked/Replaced/Renamed Download Files (%s):" % len(revokedFiles))
            output.extend(self.printWithPath(revokedFiles, c, args['releaseNew']))

        gbdbprint = len(newGbdbSet | oldGbdbSet | revokedGbdbs) 
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
            output.extend(self.printGbdbPath(pushGbdbs, args['database']))
            output.append("")
            output.append("Untouched Gbdb Files (%s):" % len((newGbdbSet & oldGbdbSet) - revokedGbdbs))
            output.extend(self.printGbdbPath((newGbdbSet & oldGbdbSet) - revokedGbdbs, args['database']))
            output.append("")
            output.append("Revoked/Replaced/Renamed Gbdb Files (%s):" % len(revokedGbdbs))
            output.extend(self.printGbdbPath(revokedGbdbs, args['database']))
        supplementalprint = len(newSupplementalSet | oldSupplementalSet)
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
            output.extend(self.printWithPath(newSupplementalSet - oldSupplementalSet, c, args['releaseNew']))
            output.append("")
            output.append("Untouched Supplemental Files (%s):" % len(oldSupplementalSet & newSupplementalSet))
            output.extend(self.printWithPath(oldSupplementalSet & newSupplementalSet, c, args['releaseNew']))
            output.append("")
            output.append("Removed Supplemental Files (%s):" % len(oldSupplementalSet - newSupplementalSet))
            output.extend(self.printWithPath(oldSupplementalSet - newSupplementalSet, c, args['releaseNew']))
        otherprint = len(additionalList | oldAdditionalList)
        if otherprint:
            output.append("\n")
            output.append("OTHER FILES:")
            output.append("New: %s" % len(additionalList | (additionalList & oldAdditionalList)))
            output.append("Revoked/Replace: %s" % len(oldAdditionalList - additionalList))
            output.append("Total: %s" % len(additionalList | oldAdditionalList))
        if otherprint and not args['summary']:
            output.append("")
            output.append("New Other Files (%s):" % len(additionalList | (additionalList & oldAdditionalList)))
            output.extend(self.printWithPath(additionalList, c, args['releaseNew']))
            output.append("")
            output.append("Revoked Other Files (%s):" % len(oldAdditionalList - additionalList))
            output.extend(self.printWithPath(oldAdditionalList, c, args['releaseNew']))
        output.append("\n")
        if len(missingFiles):
            output.append("Files that dropped between releases (%s):" % len(missingFiles))
            output.extend(self.printWithPath(missingFiles, c, args['releaseOld']))
            output.append("\n")
        if not args['ignore']:
            output.append("No Errors")
        return output

    def printReportOne(self, args, totalFiles, revokedFiles, newGbdbSet, revokedGbdbs, newTableSet, revokedTables, additionalList, c, atticSet, newSupplementalSet, tableSize):
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
        size = int(self.makeFileSizes(c, args, totalFiles - revokedFiles))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Files: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Files: %d MB" % size)
        size = int(self.makeFileSizes(c, args, newGbdbSet - revokedGbdbs))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Gbdbs: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Gbdbs: %d MB" % size)
        size = int(self.makeFileSizes(c, args, newSupplementalSet))
        totalsize = totalsize + size
        if int(size/1024) > 1:
            output.append("Supplemental: %d MB (%d GB)" % (size, int(size/1024)))
        else:
            output.append("Supplemental: %d MB" % size)
        size = int(self.makeFileSizes(c, args, (additionalList)))
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
        if not args['summary']:
            output.append("")
            if len(newTableSet - revokedTables):
                output.append("New Tables (%s):" % len(newTableSet - revokedTables))
                output.extend(self.printIter(newTableSet - revokedTables))
                output.append("\n")
            if len(totalFiles - revokedFiles):
                output.append("New Download Files (%s):" % len(totalFiles - revokedFiles))
                output.extend(self.printWithPath(totalFiles - revokedFiles, c, args['releaseNew']))
                output.append("\n")
            if len(newGbdbSet - revokedGbdbs):
                output.append("New Gbdb Files (%s):" % len(newGbdbSet - revokedGbdbs))
                output.extend(self.printGbdbPath(newGbdbSet - revokedGbdbs, args['database']))
                output.append("\n")
            if len(newSupplementalSet):
                output.append("New Supplemental Files (%s):" % len(newSupplementalSet))
                output.extend(self.printWithPath(newSupplementalSet, c, args['releaseNew']))
                output.append("\n")
            if len(additionalList):
                output.append("New Other Files (%s):" % len(additionalList))
                output.extend(self.printWithPath(additionalList, c, args['releaseNew']))
                output.append("\n")
            if len(revokedTables):
                output.append("Revoked Tables (%s):" % len(revokedTables))
                output.extend(self.printIter(revokedTables))
                output.append("\n")
            if len(revokedFiles):
                output.append("Revoked Files (%s):" % len(revokedFiles))
                output.extend(self.printWithPath(revokedFiles, c, args['releaseNew']))
                output.append("\n")
            if len(revokedGbdbs):
                output.append("Revoked Gbdbs (%s):" % len(revokedGbdbs))
                output.extend(self.printGbdbPath(revokedGbdbs, args['database']))
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
        releaseNew = args['releaseNew']
        releaseOld = args['releaseOld']
        database = args['database']
        composite = args['composite']
        loose = args['loose']
        ignore = args['ignore']
        summary = args['summary']
        errors = []
        c = track.CompositeTrack(database, composite)
        if int(releaseNew) > 1 and str(releaseOld) != 'solo':

            newReleaseFiles = c.releases[int(releaseNew)-1]
            oldReleaseFiles = c.releases[int(releaseOld)-1]

            newMdb = c.alphaMetaDb
            oldMdb = c.publicMetaDb

            #check if all files listed in release directories have associated metaDb entries
            (newMdb, revokedSet, revokedFiles, atticSet, newSupplementalSet, newFileErrors) = self.checkMetaDbForFiles(newMdb, newReleaseFiles, "alpha metaDb", loose)
            (oldMdb, spam, eggs, ham, oldSupplementalSet, oldFileErrors) = self.checkMetaDbForFiles(oldMdb, oldReleaseFiles, "public metaDb", loose)
            errors.extend(newFileErrors)
            errors.extend(oldFileErrors)

            #checks to see that nothing has disappeared between public and alpha
            errors.extend(self.checkAlphaForDropped(newMdb, oldMdb, "alpha metaDb", "stanza"))
            missingFiles = self.checkFilesForDropped(newReleaseFiles, oldReleaseFiles)
            errors.extend(self.checkMd5sums(newReleaseFiles, oldReleaseFiles, loose))

            #checks and gets tables that are present, also returns a revoked set of tables for new
            (newTableSet, revokedTableSet, newTableError) = self.checkTableStatus(newMdb, newReleaseFiles, database, composite, "alpha metaDb", loose, revokedSet)
            (oldTableSet, spam, oldTableError) = self.checkTableStatus(oldMdb, oldReleaseFiles, database, composite, "public metaDb", loose, revokedSet)
            errors.extend(newTableError)
            errors.extend(oldTableError)

            #same as above except for gbdbs
            (newGbdbSet, revokedGbdbs, newGbdbError) = self.getGbdbFiles(database, newTableSet, revokedTableSet, newMdb)
            (oldGbdbSet, eggs, oldGbdbError) = self.getGbdbFiles(database, oldTableSet, set(), oldMdb)
            errors.extend(newGbdbError)
            errors.extend(oldGbdbError)

            #for ease of typing
            totalFiles = set(newReleaseFiles)

            #these could honestly be moved earlier, get a file list processing section or something
            #they clean out special fiels out and separated the master fiels list into the 3 required
            #ones: wgEncode, supplemental and additional.
            totalFiles = self.cleanSpecialFiles(totalFiles)
            oldReleaseFiles = self.cleanSpecialFiles(set(oldReleaseFiles))
            (oldReleaseFiles, additionalList, oldAdditionalList, totalFiles) = self.separateOutAdditional(oldReleaseFiles, totalFiles, newSupplementalSet, oldSupplementalSet)

            #get the stuff you need to push, also table sizes        
            pushTables = set(sorted((newTableSet - oldTableSet)))
            tableSize = self.getTableSize(pushTables, database)
            pushFiles = set(sorted((totalFiles - oldReleaseFiles)))
            pushGbdbs = set(sorted((newGbdbSet - oldGbdbSet)))

            #don't output.append(report unless ignore option is on or no errors
            if (not errors) or ignore:
                self.output = self.printReport(args, totalFiles, newGbdbSet, newTableSet, additionalList, oldAdditionalList, pushTables, pushFiles, pushGbdbs, c, oldTableSet, oldReleaseFiles, oldGbdbSet, atticSet, revokedFiles, newMdb, revokedTableSet, revokedGbdbs, missingFiles, newSupplementalSet, oldSupplementalSet, tableSize)
            else:
                self.output = self.printErrors(errors)


        elif releaseOld == 'solo':

            newReleaseFiles = c.releases[int(releaseNew)-1]

            newMdb = c.alphaMetaDb

            (newMdb, revokedSet, revokedFiles, atticSet, newSupplementalSet, newFileErrors) = self.checkMetaDbForFiles(newMdb, newReleaseFiles, "alpha metaDb", loose)
            errors.extend(newFileErrors)

            (newTableSet, revokedTableSet, newTableError) = self.checkTableStatus(newMdb, newReleaseFiles, database, composite, "alpha metaDb", loose, revokedSet)
            errors.extend(newTableError)

            tableSize = self.getTableSize(newTableSet, database)

            (newGbdbSet, revokedGbdbs, newGbdbError) = self.getGbdbFiles(database, newTableSet, revokedTableSet, newMdb)
            errors.extend(newGbdbError)

            #set for easy operations
            totalFiles = set(newReleaseFiles)

            #clean out special fiels we don't push i.e. md5sum.history
            totalFiles = self.cleanSpecialFiles(totalFiles)

            #makes list for additional files
            (oldReleaseFiles, additionalList, oldAdditionalList, totalFiles) = self.separateOutAdditional(set(), totalFiles, newSupplementalSet, set())
            if (not errors) or ignore:
                self.output = self.printReportOne(args, totalFiles, revokedFiles, newGbdbSet, revokedGbdbs, newTableSet, revokedTableSet, additionalList, c, atticSet, newSupplementalSet, tableSize) 
            else:
                self.output = self.printErrors(errors)


def main():


    parser = argparse.ArgumentParser(
        prog='mkChangeNotes',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description='Writes out notes file for packing to QA',
        epilog=
    """Examples:

    mkChangeNotes hg19 wgEncodeUwDnase 3 2 --loose
    mkChangeNotes hg19 wgEncodeSydhTfbs 1 - --full
    mkChangeNotes hg19 wgEncodeCshlLongRnaSeq 1 -

    """
        )
    parser.add_argument('-l', '--loose', action="store_true", default=0, help='Loose checking for legacy elements. Will be retired once all tracks go through a release cycle')
    parser.add_argument('-i', '--ignore', action="store_true", default=0, help='Ignore errors, output.append(out report.')
    parser.add_argument('-s', '--summary', action="store_true", default=0, help='output.append(summary stats only.')
    parser.add_argument('database', help='The database, typically hg19 or mm9')
    parser.add_argument('composite', help='The composite name, wgEncodeCshlLongRnaSeq for instance')
    parser.add_argument('releaseNew', help='The new release to be released')
    parser.add_argument('releaseOld', nargs='?', default='-', help='The old release that is already released, if on release 1, or solo release mode, put anything here')

    if len(sys.argv) == 1:
        parser.print_help()
        return
    args = parser.parse_args(sys.argv[1:])
    if not args.releaseNew.isdigit():
        parser.print_help()
        return



    if not args.releaseOld.isdigit():
        args.releaseOld = 'solo'    
    elif int(args.releaseOld) > int(args.releaseNew):
        errors.append("Old Release is higher than New Release")
        args.releaseOld = args.releaseNew
        printErrors(errors)
        return


    c = track.CompositeTrack(args.database, args.composite)

    argsdict = {'database': args.database, 'composite': args.composite, 'releaseNew': args.releaseNew, 'releaseOld': args.releaseOld, 'loose': args.loose, 'ignore': args.ignore, 'summary': args.summary}

    notes = makeNotes(argsdict)

    for line in notes.output:
        print line

if __name__ == '__main__':
    main()

