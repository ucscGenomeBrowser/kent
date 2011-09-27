#!/usr/bin/env python
#
# compareTableSizesFromDbSnoop.py: given two mysql database summaries created by
# dbSnoop, print a list comparing the sizes of tables that are found in both databases.
#
from argparse import ArgumentParser
import re

parser = ArgumentParser(description="""
compareTableSizesFromDbSnoop.py - compare sizes of the tables that are found in both of two different databases, given two dbSnoop output files
                        """,
                        epilog="the names of tables found in both databases and their respective sizes will be sent to stdout")
parser.add_argument("-f", "--first", dest="first", required=True,
                  help="the dbSnoop file from the first database")
parser.add_argument("-s", "--second", dest="second", required=True,
                  help="the dbSnoop file from the second database")
args = parser.parse_args()

#
# Build a hash with the table sizes from the second snoop file
tableSizesFirstDb = {};
firstFile = open(args.first)
readingTableSizeSummary = False
for line in firstFile:
    line = line.rstrip()
    if re.search("TABLE SIZE SUMMARY:", line):
        readingTableSizeSummary = True
    elif len(line) == 0:
        readingTableSizeSummary = False
    elif readingTableSizeSummary and not re.search("^#", line):
        tokens = line.split()
        tableSizesFirstDb[tokens[1]] = tokens[0]

#
# In the table size summary of the first snoop file, for every table
# that has an entry in the second hash, print out the table name,
# the size in the first DB, and the size in the second DB.
print "Table\t%s Size\t%s Size" % (args.first, args.second)
secondFile = open(args.second)
readingTableSizeSummary = False
for line in secondFile:
    line = line.rstrip()
    if re.search("TABLE SIZE SUMMARY:", line):
        readingTableSizeSummary = True
    elif len(line) == 0:
        readingTableSizeSummary = False
    elif readingTableSizeSummary and not re.search("^#", line):
        tokens = line.split()
        if tableSizesFirstDb.has_key(tokens[1]):
            print "%s\t%s\t%s" % (tokens[1], tableSizesFirstDb[tokens[1]], tokens[0])
