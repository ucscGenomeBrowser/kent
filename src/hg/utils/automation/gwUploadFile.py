#!/bin/env python
#
import os, sys, traceback, stat, re, getopt, string, inspect, mwclient

def usage():
    sys.stderr.write('''gwUploadFile.py -- given a file, load into genomewiki specified image name
usage:
    gwUploadFile.py [options] <file.bb> <imageName>
        load file.bb into genome wiki under image name: Image:imageName
options:
    --site=genomewiki.ucsc.edu -- use to specify a different site
    --debug  -- show full stack trace on error exits
    -h  -- displays this help message
    -V, --version -- displays version of this program

Note: requires a .gwLogin file for login name and password to genomewiki
      either in current directory ./.gwLogin or in your HOME/.gwLogin
      A single line file with two words: UserName passWd
      and it must be 600 permissions to keep it secret.\n''')

def loginCredentials():
    # return: loginName, passWd from either ./.gwLogin  or HOME/.gwLogin
    # verify the .gwLogin file has correct restrictive permissions
    home = os.getenv('HOME')
    loginName = "None"
    passWd = "None"
    gwLogin = ".gwLogin"
    try:
	fh = open(gwLogin, 'r')
    except IOError:
	gwLogin = home + "/.gwLogin"
	try:
	    fh = open(gwLogin, 'r')
	except IOError:
	    raise "ERROR: can not read ./.gwLogin or HOME/.gwLogin password file"
    mode = stat.S_IMODE(os.lstat(gwLogin)[stat.ST_MODE])
    for level in "GRP", "OTH":
	for perm in "R", "W", "X":
	    testMode = "S_I"+perm+level
	    if mode & getattr(stat,testMode):
		raise "ERROR: your .gwLogin file must be 600 permissions"
    for line in fh:
	if re.match("#", line):
	    continue
	chomp = line.rstrip('\r\n')
	loginName, passWd = chomp.split()
	break
    fh.close()
    return loginName, passWd

def parseOptions():
    # return: file, pageName, site
    try:
	opts, args = getopt.gnu_getopt(sys.argv[1:],
	    'hV', ['debug', 'help', 'version', 'site='])
    except Exception:
	usage()
	raise "ERROR: Unrecognized option"
    if len(args) != 2:
	usage()
	raise
    site = 'genomewiki.ucsc.edu'
    sys.tracebacklimit = 0
    for o, a in opts:
	if o in ("-h", "--help"):
	    usage()
	    raise
	elif o in ("-debug", "--debug"):
	    sys.tracebacklimit = 10
	    sys.stderr.write('# setting tracebacklimit to 10\n')
	elif o in ("-V", "--version"):
	    sys.stderr.write('gwUploadFile version 1.0')
	elif o in ("-site", "--site"):
	    site = a
	else:
	    assert False, "unrecognized option"
    file = args[0]
    pageName = args[1]
    try:
	fh = open(file, 'r')
    except Exception:
	raise "ERROR: can not read file: '%s'" % (file, )
    fh.close()
    return file, pageName, site

######################################################################
#
######################################################################
try:
    file, pageName, siteUrl = parseOptions()
    loginName, passWd = loginCredentials()
    sys.stderr.write("# loading file: %s\n" % (file, ))
    sys.stderr.write("# into Image name: %s\n" % (pageName, ))
    sys.stderr.write("# login name: %s\n" % (loginName, ))
    sys.stderr.write("# siteUrl: %s\n" % (siteUrl, ))
    sys.stderr.write("# traceBackLimit: %d\n" % (sys.tracebacklimit, ))
    site = mwclient.Site(siteUrl,path='/')
    site.login(loginName, passWd)
    try:
	fh = open(file, 'rb')
    except IOError:
	sys.stderr.write("ERROR: can not read specified file: %s" % (file, ))
	sys.exit(255)
    try:
	site.upload(fh, pageName, 'test upload', ignore=True)
    except:
	sys.stderr.write("ERROR: upload returned some kind of error")
	sys.exit(255)
    fh.close()
    # Printing imageusage information
    image = site.Images[pageName]
    sys.stderr.write('Image info:', image.imageinfo)
    sys.stderr.write('Image', image.name.encode('utf-8'), 'usage:')
    for page in image.imageusage():
	sys.stderr.write('Used:', page.name.encode('utf-8'), '; namespace', page.namespace)
    sys.exit(0)
except Exception, ex:
    sys.exit(255)
