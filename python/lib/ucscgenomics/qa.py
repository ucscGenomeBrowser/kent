##!/hive/groups/encode/dcc/bin/python
import sys, os, re, argparse, subprocess, math
from ucscgenomics import ra, track

def getGbdbTables(database, tableset):
		sep = "','"
		tablestr = sep.join(tableset)
		tablestr = "'" + tablestr + "'"

		cmd = "hgsql %s -e \"select table_name from information_schema.columns where table_name in (%s) and column_name = 'fileName'\"" % (database, tablestr)
		p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
		output = p.stdout.read()

		gbdbtableset = set(output.split("\n")[1:-1])
        
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
	output = p.stdout.read()

	chrlist = set(output.split("\n")[1:-1])
	globalseen = set()
	localseen = dict()
	for i in notgbdbtablelist:
		counts = dict()
		cmd = "hgsql %s -e \"select chrom from %s\"" % (database, i)
		p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
		output = p.stdout.read()

		chrs = output.split("\n")[1:-1]
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
		print ""
		print i
		used = set()
		for j in sorted_nicely(tablecounts[i]):
			print "%s = %s" % (j, tablecounts[i][j])
		
		notused = chrlist - (localseen[i] | (chrlist - globalseen))
		if notused:
			print "Seen by others, but not used here:"
			for j in sorted_nicely(notused):
				print j
	print ""
	print "Not seen anywhere:"
	globalnotused = chrlist - globalseen
	for i in sorted_nicely(globalnotused):
		print i
	return tablecounts

def checkTableDescriptions(database, tables):
	tablelist = list()
	remain = set()
	for i in tables:
		tablelist.append("tableName = '%s'" % i)
		orsep = " OR "
		orstr = orsep.join(tablelist)
	cmd = "hgsql %s -e \"select tableName from tableDescriptions where %s\"" % (database, orstr)
	p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
	output = p.stdout.read()

	described = set(output.split("\n")[1:-1])
	remain = tables - described
	return
		
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
	#parser.add_argument('trackDb', help='The trackDb file to check')

	if len(sys.argv) == 1:
		parser.print_help()
		return

	args = parser.parse_args(sys.argv[1:])

	f = open(args.tableList, "r")
	lines = f.readlines()
	tables = set()
	for i in lines:
		tables.add(i.rstrip())

	
	tableCounts = countPerChrom(args.database, tables)
	noDescriptions = checkTableDescriptions(args.database, tables)


if __name__ == "__main__":
	main()