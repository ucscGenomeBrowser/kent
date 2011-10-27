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

        return (filtermdb, revokedset, revokedfiles, atticset, supplementalset, sorted(errors))

    def __checkAlphaForDropped(self, status, type):
        errors=[]
        diff = set(self.oldMdb) -set(self.newMdb)
        for i in diff:
            errors.append("%s: %s missing from %s" % (type, i, status))
        return errors

    def __checkFilesForDropped(self):
        diff = set(self.oldReleaseFiles) - set(self.newReleaseFiles)
        return diff

    def checkTableStatus(self, status, state):
        errors=[]
        revokedset = set()
        (database, composite, loose) = (self.database, self.composite, self.loose)
        if state == 'new':
            (mdb, files, revokedset) = (self.newMdb, self.newReleaseFiles, self.revokedSet)
        elif state == 'old':
            (mdb, files) = (self.oldMdb, self.oldReleaseFiles)

### If MySQLdb ever gets installed ###

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


### END ###

        mdbobjectset = set(mdb.filter(lambda s: s['objType'] == 'table' and 'tableName' in s and 'attic' not in s, lambda s: s['metaObject'])) - revokedset
        mdbtableset = set(mdb.filter(lambda s: s['metaObject'] in mdbobjectset, lambda s: s['tableName']))
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
        if missingTableNames:
            for i in missingTableNames:
                errors.append("table: %s is type obj, but missing tableName field called by %s" % (i, status))
        missingFromDb = mdbtableset - sqltableset
        if missingFromDb:
            for i in missingFromDb:
                errors.append("table: %s table not found in Db called by %s" % (i, status))



        return (mdbtableset, revokedtableset, errors)

    def __checkGbdbFileStatus(self, i, set, errors, state):
        filelist = i['fileName'].split(',')
        for j in filelist:
            if os.path.isfile("%s/%s" % (self.gbdbPath, j)):
                set.add(j)
            else:
                cmd = "hgsql %s -e \"select fileName from (%s)\"" % (self.database, i['tableName'])
                p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
                cmdoutput = p.stdout.read()
                if os.path.isfile(cmdoutput.split("\n")[1]):
                    set.add(j)
                else:
                    errors.append("gbdb: %s%s does not exist in %s" % (state, j, self.gbdbPath))
        return set, errors

    def getGbdbFiles(self, state):
        revokedset = set()
        if state == 'new':
            (tableset, revokedset, mdb) = (self.newTableSet, self.revokedSet, self.newMdb)
        elif state == 'old':
            (tableset, mdb) = (self.oldTableSet, self.oldMdb)

        errors = []

        gbdbtableset = qa.getGbdbTables(self.database, tableset)

        revokedtableset = qa.getGbdbTables(self.database, revokedset)

        filestanzas = mdb.filter(lambda s: s['tableName'] in gbdbtableset, lambda s: s)
        revokedstanzas = mdb.filter(lambda s: s['tableName'] in revokedtableset, lambda s: s)

        gbdbfileset = set()
        revokedfileset = set()

        for i in filestanzas:
            (gbdbfileset, errors) = self.__checkGbdbFileStatus(i, gbdbfileset, errors, "")

        for i in revokedstanzas:
            (revokedfileset, errors) = self.__checkGbdbFileStatus(i, revokedfileset, errors, "revoked gbdb ")

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

    def __makeFileSizes(self, args, inlist):
        checklist = list()

        for i in inlist:
            checklist.append("%s/%s" % (self.releasePath, i))    

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

    def __printIter(self, set, path):
        output = []
        for i in sorted(set):
            if path:
                output.append("%s/%s" % (path, i))
            else:
                output.append("%s" % (i))
        return output


    def __printSize(self, size, output, totalsize, type):

        sizeGb = int(size/1024)
        if sizeGb > 1:
            output.append("%s: %d MB (%d GB)" % (type, size, sizeGb))
        else:
            output.append("%s: %d MB" % (type, size))

        totalsize = totalsize + size

        return (output, totalsize)

    def __printSection(self, new, untouched, revoked, all, title, path, summary):
        output = []
        removeline = "Revoked/Replaced/Renamed"
        totaline = "Total (New + Untouched + Revoked/Replaced/Renamed)"
        caps = title.upper()
        if title == "supplemental":
            removeline = "Removed"
            totaline = "Total"
            title = title + " files"
            caps = title.upper()
        elif title == 'gbdbs':
            caps = "GBDBS"
            title = "gbdb files"
        elif title == "download":
            title = title + " files"
            caps = title.upper()
        if all:
            output.append("\n")
            output.append("%s:" % caps)
            output.append("New: %s" % len(new))
            output.append("Untouched: %s" % len(untouched))
            output.append("%s: %s" % (removeline, len(revoked)))
            output.append("New + Untouched: %s" % len(new | untouched))
            output.append("%s: %s" % (totaline, len(all)))
        if all and not summary:
            output.append("")
            output.append("New %s (%s):" % (title.title(), len(new)))
            output.extend(self.__printIter(new, path))
            output.append("")
            output.append("Untouched %s (%s):" % (title.title(), len(untouched)))
            output.extend(self.__printIter(untouched, path))
            output.append("")
            output.append("%s %s (%s):" % (removeline, title.title(), len(revoked)))
            output.extend(self.__printIter(revoked, path))
        return output

    def __qaHeader(self, output, newTableSet, filesNoRevoke, newGbdbSet, newSupp, additionalList, revokedTables, revokedFiles, revokedGbdbs, pushFiles, pushGbdbs, args, c):
        output = []
        tableSize = self.__getTableSize()

        output.append("mkChangeNotes v2")
        if (args['releaseOld'] == "solo"):
            output.append("%s %s Release %s" % (args['database'], args['composite'], args['releaseNew']))
        else:
            output.append("%s %s Release %s vs Release %s" % (args['database'], args['composite'], args['releaseNew'], args['releaseOld']))
        output.append("")
        output.append("QA Count Summaries for Release %s:" % args['releaseNew'])
        output.append("Tables: %d" % int(len(newTableSet)))
        output.append("Files: %d" % int(len(filesNoRevoke)))
        output.append("Gbdbs: %d" % int(len(newGbdbSet)))
        output.append("Supplemental: %d" % int(len(newSupp)))
        output.append("Other: %d" % int(len(additionalList)))
        output.append("")
        output.append("REVOKED:")
        output.append("Tables: %s" % len(revokedTables))
        output.append("Files: %s" % len(revokedFiles))
        output.append("Gbdbs: %s" % len(revokedGbdbs))
        output.append("")
        output.append("Sizes of New:")

        totalsize = 0

        (output, totalsize) = self.__printSize(tableSize, output, totalsize, "Table")
        (output, totalsize) = self.__printSize(int(self.__makeFileSizes(args, pushFiles)), output, totalsize, "Files")
        (output, totalsize) = self.__printSize(int(self.__makeFileSizes(args, pushGbdbs)), output, totalsize, "Gbdbs")
        (output, totalsize) = self.__printSize(int(self.__makeFileSizes(args, newSupp)), output, totalsize, "Supplemental")
        (output, totalsize) = self.__printSize(int(self.__makeFileSizes(args, additionalList)), output, totalsize, "Other")
        (output, totalsize) = self.__printSize(totalsize, output, 0, "Total")

        return output

    def printReport(self, args, c):
        (totalFiles, newGbdbSet, newTableSet, additionalList, oldAdditionalList, oldTableSet, oldReleaseFiles, oldGbdbSet, atticSet, revokedFiles, revokedTableSet, revokedGbdbs, missingFiles, newSupplementalSet, oldSupplementalSet) = (self.totalFiles, self.newGbdbSet, self.newTableSet, self.additionalList, self.oldAdditionalList, self.oldTableSet, self.oldTotalFiles, self.oldGbdbSet, self.atticSet, self.revokedFiles, self.revokedTableSet, self.revokedGbdbs, self.missingFiles, self.newSupplementalSet, self.oldSupplementalSet)
        #the groups here need to be predefined, I just copied and pasted after working out what they were
        sep = "\n"
        output = []
        pushTables = set(sorted((self.newTableSet - self.oldTableSet)))
        pushFiles = set(sorted((self.totalFiles - self.oldTotalFiles)))
        pushGbdbs = set(sorted((self.newGbdbSet - self.oldGbdbSet)))
        filesNoRevoke = totalFiles - revokedFiles
        allTables = newTableSet | oldTableSet | revokedTableSet
        untouchedTables = oldTableSet & newTableSet
        allFiles = totalFiles | oldReleaseFiles | revokedFiles
        newFiles = pushFiles - revokedFiles
        untouchedFiles = (totalFiles & oldReleaseFiles) - revokedFiles
        allGbdbs = newGbdbSet | oldGbdbSet | revokedGbdbs
        untouchedGbdbs = (newGbdbSet & oldGbdbSet) - revokedGbdbs
        allSupp = newSupplementalSet | oldSupplementalSet
        newSupp = newSupplementalSet - oldSupplementalSet
        removedSupp = oldSupplementalSet - newSupplementalSet
        untouchedSupp = oldSupplementalSet & newSupplementalSet
        allOther = additionalList | oldAdditionalList
        removedOther = oldAdditionalList - additionalList


        output.extend(self.__qaHeader(output, newTableSet, filesNoRevoke, newGbdbSet, newSupp, additionalList, revokedTableSet, revokedFiles, revokedGbdbs, pushFiles, pushGbdbs, args, c))

        output.extend(self.__printSection(pushTables, untouchedTables, revokedTableSet, allTables, "tables", 0, args['summary']))
        output.extend(self.__printSection(newFiles, untouchedFiles, revokedFiles, allFiles, "download", self.releasePath, args['summary']))
        output.extend(self.__printSection(pushGbdbs, untouchedGbdbs, revokedGbdbs, allGbdbs, "gbdbs", self.gbdbPath, args['summary']))
        output.extend(self.__printSection(newSupp, untouchedSupp, removedSupp, allSupp, "supplemental", self.releasePath, args['summary']))

        self.newTables = set(pushTables)
        self.newFiles = set(self.__printIter(newFiles, self.releasePath))
        self.newGbdbs = set(self.__printIter(pushGbdbs, self.gbdbPath))
        self.newSupplemental = set(self.__printIter(newSupp, self.releasePath))
        self.newOthers = set(self.__printIter(additionalList, self.releasePath))

        otherprint = len(allOther)
        if otherprint:
            output.append("\n")
            output.append("OTHER FILES:")
            output.append("New: %s" % len(additionalList))
            output.append("Revoked/Replace: %s" % len(removedOther))
            output.append("Total: %s" % len(allOther))
        if otherprint and not args['summary']:
            output.append("")
            output.append("New Other Files (%s):" % len(additionalList))
            output.extend(sorted(list(self.newOthers)))
            output.append("")
            output.append("Revoked Other Files (%s):" % len(removedOther))
            output.extend(self.__printIter((removedOther), self.releasePath))
        output.append("\n")

        if len(missingFiles):
            output.append("Files that dropped between releases (%s):" % len(missingFiles))
            output.extend(self.__printIter(missingFiles, self.releasePath))
            output.append("\n")

        if not args['ignore']:
            output.append("No Errors")
        return output

    def __printSectionOne(self, output, set, title):
        output = []
        if set:
            output.append("%s (%s):" % (title, len(set)))
            output.extend(sorted(list(set)))
            output.append("\n")
        return output

    def printReportOne(self, args, c):
        (totalFiles, revokedFiles, newGbdbSet, revokedGbdbs, newTableSet, revokedTables, additionalList, atticSet, newSupplementalSet, tableSize) = (self.totalFiles, self.revokedFiles, self.newGbdbSet, self.revokedGbdbs, self.newTableSet, self.revokedTableSet, self.additionalList, self.atticSet, self.newSupplementalSet, self.tableSize)
        output = []
        newTables = newTableSet - revokedTables
        newFiles = totalFiles - revokedFiles
        newGbdbs = newGbdbSet - revokedGbdbs

        output.extend(self.__qaHeader(output, newTables, newFiles, newGbdbSet, newSupplementalSet, additionalList, revokedTables, revokedFiles, revokedGbdbs, totalFiles, newGbdbSet, args, c))
        self.newTables = set(newTables)
        self.newFiles = set(self.__printIter(newFiles, self.releasePath))
        self.newGbdbs = set(self.__printIter(newGbdbs, self.releasePath))
        self.newSupplemental = set(self.__printIter(newSupplementalSet, self.releasePath))
        self.newOthers = set(self.__printIter(additionalList, self.releasePath))

        if not args['summary']:
            output.append("")
            output.extend(self.__printSectionOne(output, self.newTables, "New Tables"))
            output.extend(self.__printSectionOne(output, self.newFiles, "New Download Files"))
            output.extend(self.__printSectionOne(output, self.newGbdbs, "New Gbdb Files"))
            output.extend(self.__printSectionOne(output, self.newSupplemental, "New Supplemental Files"))
            output.extend(self.__printSectionOne(output, self.newOthers, "New Other Files"))
            output.extend(self.__printSectionOne(output, self.__printIter(revokedTables, 0), "Revoked Tables"))
            output.extend(self.__printSectionOne(output, self.__printIter(revokedFiles, self.releasePath), "Revoked Files"))
            output.extend(self.__printSectionOne(output, self.__printIter(revokedGbdbs, self.gbdbPath), "Revoked Gbdbs"))
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

        self.releasePath = c.httpDownloadsPath + 'release' + args['releaseNew']
        self.gbdbPath = "/gbdb/%s/bbi" % args['database']
        if int(self.releaseNew) > 1 and str(self.releaseOld) != 'solo':

            self.newReleaseFiles = c.releases[int(self.releaseNew)-1]
            self.oldReleaseFiles = c.releases[int(self.releaseOld)-1]

            self.newMdb = c.alphaMetaDb
            self.oldMdb = c.publicMetaDb

            #make a list of missing files
            self.missingFiles = self.__checkFilesForDropped()
            
            #check if all files listed in release directories have associated metaDb entries
            (self.newMdb, self.revokedSet, self.revokedFiles, self.atticSet, self.newSupplementalSet, newFileErrors) = self.checkMetaDbForFiles("alpha metaDb", "new")
            (self.oldMdb, spam, eggs, ham, self.oldSupplementalSet, oldFileErrors) = self.checkMetaDbForFiles("public metaDb", "old")

            #checks to see that nothing has disappeared between public and alpha
            errors.extend(self.__checkAlphaForDropped("alpha metaDb", "stanza"))
            errors.extend(self.__checkMd5sums())

            #checks and gets tables that are present, also returns a revoked set of tables for new
            (self.newTableSet, self.revokedTableSet, newTableError) = self.checkTableStatus("alpha metaDb", "new")
            (self.oldTableSet, spam, oldTableError) = self.checkTableStatus("public metaDb", "old")

            #same as above except for gbdbs
            (self.newGbdbSet, self.revokedGbdbs, newGbdbError) = self.getGbdbFiles("new")
            (self.oldGbdbSet, eggs, oldGbdbError) = self.getGbdbFiles("old")

            #fill in the errors
            errors.extend(newFileErrors)
            errors.extend(oldFileErrors)
            errors.extend(newTableError)
            errors.extend(oldTableError)
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

            self.errors = errors
            #don't output.append(report unless ignore option is on or no errors
            if (not errors) or self.ignore:
                self.output = self.printReport(args, c)
            else:
                self.output = self.printErrors(errors)


        elif self.releaseOld == 'solo':

            self.newReleaseFiles = c.releases[int(self.releaseNew)-1]

            self.newMdb = c.alphaMetaDb

            (self.newMdb, self.revokedSet, self.revokedFiles, self.atticSet, self.newSupplementalSet, newFileErrors) = self.checkMetaDbForFiles("alpha metaDb", "new")

            (self.newTableSet, self.revokedTableSet, newTableError) = self.checkTableStatus("alpha metaDb", "new")

            self.tableSize = self.__getTableSize()

            (self.newGbdbSet, self.revokedGbdbs, newGbdbError) = self.getGbdbFiles("new")

            #collect errors
            errors.extend(newFileErrors)
            errors.extend(newTableError)
            errors.extend(newGbdbError)

            #set for easy operations
            totalFiles = set(self.newReleaseFiles)

            #clean out special fiels we don't push i.e. md5sum.history
            self.totalFiles = self.__cleanSpecialFiles(totalFiles)

            #makes list for additional files
            (self.oldTotalFiles, self.oldSupplementalSet) = (set(), set())
            (self.oldReleaseFiles, self.additionalList, self.oldAdditionalList, self.totalFiles) = self.__separateOutAdditional()
            self.errors = errors
            if (not errors) or self.ignore:
                self.output = self.printReportOne(args, c) 
            else:
                self.output = self.printErrors(errors)
