##!/hive/groups/encode/dcc/bin/python
import sys, os, re, argparse, subprocess, math
from ucscgenomics import ra, track

def getGbdbTables(database, tableset):
		sep = "','"
		tablestr = sep.join(tableset)
		tablestr = "'" + tablestr + "'"

		cmd = "hgsql %s -e \"select table_name from information_schema.columns where table_name in (%s) and column_name = 'fileName'\"" % (database, tablestr)
		p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
		cmdoutput = p.stdout.read()

		gbdbtableset = set(cmdoutput.split("\n")[1:-1])
        
		return gbdbtableset

def sorted_nicely(l): 
    """ Sort the given iterable in the way that humans expect.""" 
    convert = lambda text: int(text) if text.isdigit() else text 
    alphanum_key = lambda key: [ convert(c) for c in re.split('([0-9]+)', key) ] 
    return sorted(l, key = alphanum_key)

def countPerChrom(database, tables):
	notgbdbtablelist = tables - getGbdbTables(database, tables)
	tablecounts = dict()
	cmd = "hgsql %s -e \"select chrom from chromInfo\"" % database
	p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
	cmdoutput = p.stdout.read()

	chrlist = set(cmdoutput.split("\n")[1:-1])
	globalseen = set()
	localseen = dict()
	output = []
	if not tables:
		output.append("No Tables to count")
		output.append("")
		return (output, tablecounts)
	for i in notgbdbtablelist:
		counts = dict()
		cmd = "hgsql %s -e \"select chrom from %s\"" % (database, i)
		p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
		cmdoutput = p.stdout.read()

		chrs = cmdoutput.split("\n")[1:-1]
		localseen[i] = set()
		
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
	return (output, tablecounts)

def checkTableDescriptions(database, tables):
	tablelist = list()
	missing = set()
	output = []
	for i in tables:
		tablelist.append("tableName = '%s'" % i)
		orsep = " OR "
		orstr = orsep.join(tablelist)
	cmd = "hgsql %s -e \"select tableName from tableDescriptions where %s\"" % (database, orstr)
	p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
	cmdoutput = p.stdout.read()

	described = set(cmdoutput.split("\n")[1:-1])
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
	notgbdbtablelist = tables - getGbdbTables(database, tables)
	tablelist = list()
	missing = set()
	output = []

	for i in notgbdbtablelist:
		cmd = "hgsql %s -e \"show indexes from %s\"" % (database, i)
		p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
		cmdoutput = p.stdout.read()
		
		index = cmdoutput.split("\n")[1:-1]
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
	
	
def checkTableName(database, tables):
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
	f = open(trackDb, "r")
	lines = f.readlines()
	seenlabel = dict()
	output = []
	toolong = list()
	p1 = re.compile('^\s+longLabel\s+(.*)$')
	p2 = re.compile('^\s+shortLabel\s+(.*)$')
	for i in lines:
		m1 = p1.match(i)
		m2 = p2.match(i)
		if m1:
			if seenlabel.has_key(m1.group(1)):
				seenlabel[m1.group(1)] = seenlabel[m1.group(1)] + 1
			else:
				seenlabel[m1.group(1)] = 1
			if len(m1.group(1)) > 80:
				toolong.append([m1.group(1), len(m1.group(1))])
				output.append("longLabel '%s' is too long: %s" % (m1.group(1), len(m1.group(1))))
		if m2:
			#short labels are allowed to repeat
# 			if seenlabel.has_key(m2.group(1)):
# 				seenlabel[m2.group(1)] = seenlabel[m2.group(1)] + 1
# 			else:
# 				seenlabel[m2.group(1)] = 1
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

def main():

	parser = argparse.ArgumentParser(
        prog='qaChecks',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description='A series of checks for QA',
        epilog=
    """Examples:

qaChecks hg19 tableList
qaChecks hg19 tableList /path/to/trackDb.ra
qaChecks hg19 tableList ~/kent/src/hg/makeDb/trackDb/human/hg19/wgEncodeSydhTfbs.new.ra

    """
        )
	parser.add_argument('database', help='The database, typically hg19 or mm9')
	parser.add_argument('tableList', help='The file containing a list of tables')
	parser.add_argument('trackDb', help='The trackDb file to check')

	if len(sys.argv) == 1:
		parser.print_help()
		return

	args = parser.parse_args(sys.argv[1:])

	f = open(args.tableList, "r")
	lines = f.readlines()
	tables = set()
	for i in lines:
		tables.add(i.rstrip())

	output = []

	(tableDescOutput, noDescription) = checkTableDescriptions(args.database, tables)
	output.extend(tableDescOutput)

	(tableIndexOut, missingIndex) = checkTableIndex(args.database, tables)
	output.extend(tableIndexOut)
	
	(tableNameOut, badTableNames) = checkTableName(args.database, tables)
	output.extend(tableNameOut)
	
	(labelOut, badLabels) = checkLabels(args.trackDb)
	output.extend(labelOut)

	(countChromOut, tableCounts) = countPerChrom(args.database, tables)
	output.extend(countChromOut)

	for i in output:
		print i
	

if __name__ == "__main__":
	main()