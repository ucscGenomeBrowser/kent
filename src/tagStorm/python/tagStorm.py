# this file is for python2.7, not python3

import sys
import string
from collections import defaultdict, OrderedDict
import operator, logging

"""
    This library provides a function "tagStormFromFile" to parse tagStorm files to
    a list of nested dictionaries.  The key "children" has a special meaning,
    it points to dicts beneath the current dict, reflecting the tagStorm indentation.

    The library preserves the order of the tags by using the OrderedDict() data type,
    instead of normal dictionaries.

    This source file can also be called from the command line and be used as a
    JSON/Yaml converter or for testing/debugging:

    python tagStorm.py test json
    - outputs built-in test document to stdout in JSON format

    python tagStorm.py test.txt json
    - outputs test.txt to stdout in JSON format

    python tagStorm.py test.txt yaml
    - outputs test.txt to stdout in YAML format

    python tagStorm.py test.txt tab
    - outputs test.txt to stdout in tab-sep format

    python tagStorm.py test.txt index
    - writes test.txt to the sqlite database test.txt.sqlite

    python tagStorm.py test.txt query grand*,child3 child3=3
    - outputs dictionaries of the stanzas where child3=3, only outputs the fields that start with
      'grand' and those that are called 'child3'

"""

def parseTagLine(line):
    " given a tagStorm line, return (indent, key, val) "
    strippedLine = line.lstrip(" ")
    indent = len(line) - len(strippedLine)
    strippedLine = strippedLine.rstrip() # remove trailing white space. Is this covered by the tagStorm spec?
    key, val = string.split(strippedLine, " ", maxsplit=1)
    return indent, key, val

def nextStanza(inFile):
    " parse all tags into a dict, up to the next newline or EOF. Returns dict and indent level. indent=None means EOF. "
    stanza = OrderedDict() # lets keep the order of the tags for now
    #stanza = dict()
    indent = None
    while True:
        line = inFile.readline()
        if line == "": # "" = EOF
            return stanza, indent
        if line.strip(" ") == "\n":
            return stanza, indent
        if line.lstrip().startswith("#"):
            continue
        indent, key, val = parseTagLine(line)
        stanza[key] = val

def addStanzas(inFile, stanzas, lastIndent):
    " recursively parse stanzas from file and add to the stanzas list "
    while True:
        oldPos = inFile.tell()
        stanza, indent = nextStanza(inFile)

        if indent==None: # EOF
            return

        elif indent == lastIndent:
            stanzas.append(stanza)

        elif indent > lastIndent:
            subStanzas = [stanza]
            addStanzas(inFile, subStanzas, indent)
            stanzas[-1]["children"] = subStanzas

        elif indent < lastIndent:
            inFile.seek(oldPos) # go back to the start of the line again
            return

def tagStormFromFile(inFile):
    """ parse tag storm file. Return as a list of nested dicts, special key is  'children' in the dicts """
    stanzas = []
    addStanzas(inFile, stanzas, 0)
    return stanzas

def tagStormToJson(inFile, indent=None):
    " return JSON string for a tagStorm file "
    import json
    return json.dumps(tagStormFromFile(inFile), indent=indent)

def tagStormToYaml(inFile):
    " return yaml string for a tagStorm file "
    import yaml
    return yaml.dump_all(tagStormFromFile(inFile))

def mergeChild(parent, child):
    " merge child into parent and return a copy "
    parCopy = dict(parent.items())
    for key, val in child.items():
        if key!="children":
            parCopy[key] = val
    return parCopy

def tagsToRows(tags, currDict, rows):
    " recursively collect data into currDict, and append to rows when we have a full row "
    for stanza in tags:
        if "children" in stanza:
            newDict = mergeChild(currDict, stanza)
            tagsToRows(stanza["children"], newDict, rows)
        else:
            newDict = mergeChild(currDict, stanza)
            rows.append(newDict)

def tagStormToRows(inFile):
    " yield rows as rows. returns (list of fields, list of rows)"
    tags = tagStormFromFile(inFile)

    rowDicts = []
    tagsToRows(tags, {}, rowDicts)

    # create a dict tagName -> count how often it appears
    allTags = defaultdict(int)
    for d in rowDicts:
        for key in d:
            allTags[key] += 1

    tagCounts = allTags.items()
    tagCounts.sort( key = operator.itemgetter(1), reverse=True )
    sortedTags = [x for (x,y) in tagCounts]

    rows = []
    for rowDict in rowDicts:
        # convert dictionaries to namedtuples
        row = []
        for key in sortedTags:
            if key not in rowDict:
                row.append("")
            else:
                row.append(rowDict[key])
        rows.append(row)

    return sortedTags, rows

def tagStormToTabLines(ifh):
    " convert tagStorm file to tab-separated lines. yields strings, one per line. "
    headers, rows = tagStormToRows(ifh)
    yield "\t".join(headers)
    for r in rows:
        yield "\t".join(r)

def tagStormToTab(ifh):
    " convert tagStorm file to tab-separated lines. yields strings, one per line. "
    lines = tagStormToTabLines(ifh)
    return "\n".join(lines)

def tagStormToLines(tags, level):
    " recursively convert a tagStorm list of nested dicts back to text. returns a list of strings, one per line. "
    lines = []
    for stanza in tags:
        for key, val in stanza.items():
            if key!="children":
                indent = "".join([" "]*(4*level))
                line = indent+key+" "+val
                lines.append(line)
        lines.append("")

        if "children" in stanza:
            lines.extend(tagStormToLines(stanza["children"], level+1))

    return lines

def tagStormWrite(tags, outFile):
    " convert tagStorm list of dicts back to text and write to outFile "
    outFile.write( "\n".join(tagStormToLines(tags, 0)) )

def tagStormToSqlite(ifh, dbFname):
    " write a tagStorm file as rows to a sqlite database "
    import sqlite3
    fieldNames, rows = tagStormToRows(ifh)
    conn = sqlite3.connect(dbFname)
    cur = conn.cursor()
    cur.execute("DROP TABLE IF EXISTS tagStorm;")

    sqlFields = ["%s text"%fieldName for fieldName in fieldNames]
    sqlStr = ",".join(sqlFields)
    cur.execute("CREATE TABLE tagStorm (%s);" % sqlStr)

    qMarks = ["?"]*len(fieldNames)
    sqlStr = ",".join(qMarks)
    query = "INSERT into tagStorm values (%s)" % sqlStr

    count = 0
    for row in rows:
        cur.execute(query, row)
        count += 1

    # create indeces, one per field
    for i, fieldName in enumerate(fieldNames):
        cur.execute("CREATE INDEX index%d ON tagStorm (%s)" % (i, fieldName))

    conn.commit()

    print("Wrote %d rows to %s" % (count, dbFname))

def _dict_factory(cursor, row):
    " convert sqlite rows to dicts, removing empty columns "
    d = {}
    for idx, col in enumerate(cursor.description):
        if row[idx]!="":
            d[col[0]] = str(row[idx])
    return d

def sqliteFieldNames(cur, table):
    " return a list with the field names of a table "
    cur.execute("PRAGMA table_info('%s')" % table)
    rows = cur.fetchall()
    names = []
    for r in rows:
        names.append(r["name"])
    return names

def findSelectedFields(fieldSearchList, fieldNames):
    """ fieldSearchList is a list of fields, potentially with wild cards. fieldNames is the 
    real list of field names. Returns a list of all fields that match the SearchList.
    """
    prefixes = []
    exactMatches = []
    for f in fieldSearchList:
        if f.endswith("*"):
            prefixes.append(f.rstrip("*"))
        else:
            exactMatches.append(f)

    fieldsShown = []
    for f in fieldNames:
        if f in exactMatches:
            fieldsShown.append(f)
            continue

        for pf in prefixes:
            if f.startswith(pf):
                fieldsShown.append(f)
                break
    return fieldsShown

def filterDict(d, keys):
    " return a dict with only the keys from d "
    newD = {}
    for k in keys:
        if k in d:
            newD[k] = d[k]
    return newD

def tagStormQuerySqlite(dbFname, fields, whereExpr):
    """ run whereExpr over the tagStorm table in an sqlite db, return only the fields in fieldSearchList.
    fieldSearchList is a list of fields, potentially with wildcards, e.g. a* selects all fields that start with a
    """
    import sqlite3
    conn = sqlite3.connect(dbFname)
    conn.row_factory = _dict_factory # return dicts from db, not just lists
    cur = conn.cursor()

    fieldSearchList = fields.split(",")
    fieldNames = sqliteFieldNames(cur, "tagStorm")
    selFields = findSelectedFields(fieldSearchList, fieldNames)

    rows = cur.execute("SELECT * from tagStorm WHERE %s" % whereExpr)
    for row in rows:
        print(filterDict(row, selFields))

def getTest():
    " return a test tag storm file as a string "
    return \
"""#
grandfather a
grandmother b
        
        mother1 1
        dad2 2

                child3 3

        mother3 3
        dad4 4

                child5 5

grandfather 6
grandmother 7"""

if __name__=="__main__":
    # main - only used when the library is called from the command line, 
    # e.g. with 'python tagStorm.py'
    inFname = sys.argv[1]
    out = sys.argv[2]
    #logging.basicConfig(level=1)

    if inFname=="test":
        import StringIO
        #py3k: from io import StringIO
        ifh = StringIO.StringIO(getTest()) # make a string looke like an opened file, like Java's StringReader
    else:
        ifh = open(inFname)

    if out=="json":
        print(tagStormToJson(ifh, indent=4))
    elif out=="yaml":
        print(tagStormToYaml(ifh))
    elif out=="tab":
        print(tagStormToTab(ifh))
    elif out=="tagStorm":
        tags = tagStormFromFile(ifh)
        tagStormWrite(tags, sys.stdout)
    elif out=="index":
        dbFname = inFname+".sqlite"
        tagStormToSqlite(ifh, dbFname)
    elif out=="query":
        fields = sys.argv[3]
        where = sys.argv[4]
        dbFname = inFname+".sqlite"
        tagStormQuerySqlite(dbFname, fields, where)
    else:
        raise Exception("Unknown command %s" % out)
