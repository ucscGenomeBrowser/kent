import re
import subprocess
import datetime
import time
from ucscgenomics.qa import qaUtils

"""
    A collection of functions useful to the ENCODE QA Teams.
"""

def getGbdbTables(database, tableset):
    """ Remove tables that aren't pointers to Gbdb files."""
    sep = "','"
    tablestr = sep.join(tableset)
    tablestr = "'" + tablestr + "'"
    hgsqlOut = qaUtils.callHgsql(database, "select table_name from information_schema.columns where\
                                 table_name in ("+ tablestr + ") and column_name = 'fileName'")
    gbdbtableset = set(hgsqlOut.split())
    return gbdbtableset

def sorted_nicely(l):
    """ Sort the given iterable in the way that humans expect."""
    convert = lambda text: int(text) if text.isdigit() else text
    alphanum_key = lambda key: [ convert(c) for c in re.split('([0-9]+)', key) ]
    return sorted(l, key = alphanum_key)

def countPerChrom(database, tables):
    """ Count the amount of rows per chromosome."""
    notgbdbtablelist = tables - getGbdbTables(database, tables)
    tablecounts = dict()
    output = []
    globalseen = set()
    localseen = dict()

    hgsqlOut = qaUtils.callHgsql(database, "select chrom from chromInfo")
    chrlist = set(hgsqlOut.split())

    notPositionalTable = set()
    if not notgbdbtablelist:
        output.append("No tables to count chroms")
        output.append("")
        return (output, tablecounts)
    for i in notgbdbtablelist:
        counts = dict()

        cmd = "hgsql %s -e \"select chrom from %s\"" % (database, i)
        p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
        cmdoutput = p.stdout.read()

        chrs = cmdoutput.split("\n")[1:-1]

        localseen[i] = set()

        if not chrs:
            notPositionalTable.add(i)
            continue
        for j in chrs:
            globalseen.add(j)
            if counts.has_key(j):
                counts[j] = counts[j] + 1
            else:
                localseen[i].add(j)
                counts[j] = 1
        tablecounts[i] = counts

    for i in sorted(tablecounts):
        output.append(i)
        used = set()
        for j in sorted_nicely(tablecounts[i]):
            output.append("%s = %s" % (j, tablecounts[i][j]))

        notused = chrlist - (localseen[i] | (chrlist - globalseen))
        if notused:
            output.append("Seen by others, but not used here:")
            for j in sorted_nicely(notused):
                output.append(j)
        output.append("")
    globalnotused = chrlist - globalseen
    if globalnotused:
        output.append("Not seen anywhere:")
        for i in sorted_nicely(globalnotused):
            output.append(i)
    output.append("")
    if notPositionalTable:
        output.append("Not a positional table:")
        for i in notPositionalTable:
            output.append(i)
    return (output, tablecounts)

def checkTableDescriptions(database, tables):
    """ Check if each table has a description or not."""
    tablelist = list()
    missing = set()
    output = []
    orstr = ""
    for i in tables:
        tablelist.append("tableName = '%s'" % i)
        orsep = " OR "
        orstr = orsep.join(tablelist)
    hgsqlOut = qaUtils.callHgsql(database, "select tableName from tableDescriptions where " + orstr)
    described = set(hgsqlOut.split())
    missing = tables - described
    if missing:
        output.append("Tables missing a description:")
        for i in missing:
            output.append(i)
        output.append("")
    else:
        output.append("No tables missing a description")
        output.append("")
    return (output, missing)

def checkTableIndex(database, tables):
    """ Check if each table has an index or not."""
    notgbdbtablelist = tables - getGbdbTables(database, tables)
    tablelist = list()
    missing = set()
    output = []

    if not notgbdbtablelist:
        output.append("No tables require an index")
        output.append("")
        return (output, missing)

    for i in notgbdbtablelist:
        hgsqlOut = qaUtils.callHgsql(database, "show indexes from " + i)
        index = hgsqlOut.split()
        if index:
            pass
        else:
            missing.add(i)
    if missing:
        output.append("Tables missing an index:")
        for i in missing:
            output.append(i)
        output.append("")
    else:
        output.append("No missing indices")
        output.append("")

    return (output, missing)

def checkTableName(tables):
    """ Check if table name has an underscore or not."""
    bad = set()
    output = []
    for i in tables:
        if re.search('.*_.*', i):
            bad.add(i)
    if bad:
        output.append("These tables have underscores in the name")
        for i in bad:
            output.append(i)
        output.append("")
    else:
        output.append("No malformed table names")
        output.append("")
    return (output, bad)

def checkLabels(trackDb):
    """ Check if long and short labels are too long, are duplicated or are auto-generated."""
    f = open(trackDb, "r")
    lines = f.readlines()
    seenlabel = dict()
    output = []
    toolong = list()
    p1 = re.compile('^\s*longLabel\s+(.*)$')
    p2 = re.compile('^\s*shortLabel\s+(.*)$')
    p3 = re.compile('^\s*#.*$')
    for i in lines:
        m1 = p1.match(i)
        m2 = p2.match(i)
        m3 = p3.match(i)
        if m3:
            continue
        if m1:
            if seenlabel.has_key(m1.group(1)):
                seenlabel[m1.group(1)] = seenlabel[m1.group(1)] + 1
            else:
                seenlabel[m1.group(1)] = 1
            if re.search('autogenerated', m1.group(1)):
                toolong.append([m1.group(1), -1])
                output.append("longLabel '%s' is still autogenerated, please tell the wrangler to fix this" % m1.group(1))
            if len(m1.group(1)) > 80:
                toolong.append([m1.group(1), len(m1.group(1))])
                output.append("longLabel '%s' is too long: %s" % (m1.group(1), len(m1.group(1))))
        if m2:
            #short labels are allowed to repeat
            #if seenlabel.has_key(m2.group(1)):
                #seenlabel[m2.group(1)] = seenlabel[m2.group(1)] + 1
            #else:
                #seenlabel[m2.group(1)] = 1
            if len(m2.group(1)) > 17:
                toolong.append([m2.group(1), len(m2.group(1))])
                output.append("shortLabel '%s' is too long: %s" % (m2.group(1), len(m2.group(1))))
    for i in seenlabel:
        if seenlabel[i] > 1:
            output.append("%s label seen more than once: %s" % (i, seenlabel[i]))

    if output:
        output.insert(0,"Label errors:")
        output.append("")
    else:
        output.append("No labels are incorrect")
        output.append("")

    return (output, toolong)


def checkTableCoords(database, tables):
    """Runs checkTableCoords externally against a set of tables, timeout is 10 seconds"""
    notgbdbtablelist = tables - getGbdbTables(database, tables)
    results = []
    output = []

    if not notgbdbtablelist:
        output.append("No tables have coordinates")
        output.append("")
        return (output, results)


    timeout = 20
    for i in sorted(notgbdbtablelist):
        start = datetime.datetime.now()
        cmd = "checkTableCoords %s %s" % (database, i)
        p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
        killed = 0
        while p.poll() is None:
            time.sleep(0.1)
            now = datetime.datetime.now()
            if (now - start).seconds > timeout:
                p.kill()
                killed = 1
        if not killed:
            cmdoutput = p.stdout.read()
            cmderr = p.stderr.read()

            if cmdoutput:
                results.append(cmdoutput)
            if cmderr:
                results.append(cmderr)
        elif killed:
            results.append("Process timeout after %d seconds, for table: %s" % (timeout, i))
            results.append("You might want to manually run: '%s'" % cmd)
            results.append("")

    if results:
        output.append("These tables have coordinate errors:")
        for i in results:
            output.append(i)
    else:
        output.append("No coordinate errors")
        output.append("")
    return (output, results)

def positionalTblCheck(database, tables):
    notgbdbtablelist = tables - getGbdbTables(database, tables)


    results = []
    output = []

    if not notgbdbtablelist:
        output.append("No tables are positional")
        output.append("")
        return (output, results)

    for i in notgbdbtablelist:
        cmd = "positionalTblCheck %s %s" % (database, i)
        p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
        cmdoutput = p.stdout.read()
        cmderr = p.stderr.read()
        if cmdoutput:
            results.append(cmdoutput)
        if cmderr:
            results.append(cmderr)
    if results:
        p = re.compile('(.*)does not appear to be a positional table')
        outResults = list()
        nonPositional = list()
        for i in results:
            m = p.search(i)
            if m:
                nonPositional.append(m.group(1))
            else:
                outResults.append(i)

        output.append("These tables have position errors:")
        for i in outResults:
            output.append(i)
        if nonPositional:
            output.append("These tables are non-positional:")
            for i in nonPositional:
                output.append(i)
        output.append("")
    else:
        output.append("No position errors")
        output.append("")
    return (output, results)
