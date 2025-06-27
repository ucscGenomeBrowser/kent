# Library functions for genome browser CGI scripts written in Python 3

# Because this library is loaded for every CGI execution, only a
# fairly minimal set of functions is implemented here, e.g. hg.conf parsing,
# bottleneck, cart loading, mysql queries.

# The cart is currently read-only. More work is needed to allow writing a cart.

# General rules for CGI in Python:
# - never insert values into SQL queries. Write %s in the query and provide the
#   arguments to sqlQuery as a list.  
# - never print incoming HTTP argument as raw text. Run it through cgi.escape to 
#   destroy javascript code in them.

try:
    import pymysql.cursors
except:
    print("Installation error - could not load pymysql for Python. Please tell your system administrator to run " \
        "one of these commands as root: 'pip install pymysql'. Sometimes, pip is called 'pip3'.")
    exit(0)

# Imports from the Python 3 standard library
# This is a CGI, not a WSGI - minimize global imports. Each library import can take up to 20msecs.
import os, cgi, sys, logging, time

from os.path import join, isfile, normpath, abspath, dirname, isdir, splitext
from collections import namedtuple

from http import cookies
import urllib.request

# activate debugging output output only on dev
import platform
if "hgwdev" in platform.node():
    import cgitb
    cgitb.enable()

# debug level: a number. the higher, the more debug info is printed
# to see most debug messages, set to 1
# another way to change this variable is by setting the URL variable "debug" to 1
verboseLevel = 0

cgiArgs = None

# like in the kent tree, we keep track of whether we have already output the content-type line
contentLineDone = False

# show the bot delay warning message before other printing is done?
doWarnBot = False
# current bot delay in milliseconds
botDelayMsecs = 0

# two global variables: the first is the botDelay limit after which the page is slowed down and a warning is shown
# the second is the limit after which the page is not shown anymore
botDelayWarn = 1500
botDelayBlock = 3000

jksqlTrace = False

forceUnicode = False

def warn(format, *args):
    print (format % args)

def errAbort(msg, status=None, headers = None):
    " show msg and abort. Like errAbort.c "
    printContentType(status=status, headers=headers)
    print(msg)
    exit(0)

def debug(level, msg):
    " output debug message with a given verbosity level "
    if verboseLevel >= level:
        printContentType()
        print(msg+"<br>")
        sys.stdout.flush()
 
def parseConf(fname):
    " parse a hg.conf style file, return as dict key -> value (all strings) "
    conf = {}
    for line in open(fname):
        line = line.strip()
        if line.startswith("#"):
            continue
        elif line.startswith("include "):
            inclFname = line.split()[1]
            absFname = normpath(join(dirname(fname), inclFname))
            if os.path.isfile(absFname):
                inclDict = parseConf(absFname)
                conf.update(inclDict)
        elif "=" in line: # string search for "="
            key, value = line.split("=",1)
            conf[key] = value
    return conf


# cache of hg.conf contents
hgConf = None

def parseHgConf():
    """ return hg.conf as dict key:value. """
    global hgConf
    if hgConf is not None:
        return hgConf

    hgConf = dict() # python dict = hash table

    confDir = os.path.dirname(__file__) # look for hg.conf in parent dir
    fname = os.path.join(confDir, "..", "hg.conf")
    hgConf = parseConf(fname)

    if cfgOptionBoolean("JKSQL_TRACE"):
        global jksqlTrace
        jksqlTrace = True
    
    return hgConf

def cfgOption(name, default=None):
    " return hg.conf option or default "
    global hgConf

    if not hgConf:
        parseHgConf()

    return hgConf.get(name, default)

def cfgOptionBoolean(name, default=False):
    " return True if option is set to 1, on or true, or default if not set "
    val = hgConf.get(name, default) in [True, "on", "1", "true"]
    return val

def sqlConnect(db, host=None, user=None, passwd=None):
    """ connect to sql server specified in hg.conf with given db. Like jksql.c. """
    cfg = parseHgConf()
    if host==None:
        host, user, passwd = cfg["db.host"], cfg["db.user"], cfg["db.password"]
    conn = pymysql.connect(host=host, user=user, passwd=passwd, db=db, charset="utf8")

    # we will need this info later
    conn.failoverConn = None
    conn.db = db
    conn.host = host
    return conn

def sqlTableExists(conn, table):
    " return True if table exists. Like jksql.c "
    query = "SHOW TABLES LIKE %s"
    sqlQueryExists(conn, query, table)

def sqlQueryExists(conn, query, args=None):
    " return true if query returns a result. Like jksql.c. No caching for now, unlike hdb.c. "
    cursor = conn.cursor()
    rows = cursor.execute(query, args)
    row = cursor.fetchone()

    res = (row!=None)
    cursor.close()
    return res

def _sqlConnectFailover(conn):
    " connect the failover connection of a connection "
    cfg = parseHgConf()
    if "slow-db.host" not in cfg:
        return

    host, user, passwd = cfg["slow-db.host"], cfg["slow-db.user"], cfg["slow-db.password"]
    sys.stderr.write("SQL_CONNECT 0 %s %s %s\n" % (host, user, passwd))
    db = conn.db
    failoverConn = pymysql.connect(host=host, user=user, passwd=passwd, db=db)
    conn.failoverConn = failoverConn

def _timeDeltaSeconds(time1, time2):
    " convert time difference to total seconds for python 2.6 "
    td = time1 - time2
    return (td.microseconds + (td.seconds + td.days * 24 * 3600) * 10**6) / 10**6

def byteToUnicode(rows):
    " makes sure that all fields are of type 'str' and no single field is a byte string "
    newRows = []
    for row in rows:
        newRow = []
        for field in row:
            if isinstance(field, bytes):
                field = field.decode("utf8")
            newRow.append(field)
        newRows.append(newRow)

    return newRows

def sqlQuery(conn, query, args=None):
    """ Return all rows for query, placeholders can be used, args is a list to
    replace placeholders, to prevent Mysql injection.  Never do replacement
    with %s yourself, unless the value is coming from inside the program. This
    is called a "parameterized query". There is only %s, %d does not work.

    example:
    query = "SELECT contents FROM table WHERE id=%(id)s and name=%(name)s;"
    rows = sqlQuery(conn, query, {"id":1234, "name":"hiram"})
    """
    cursor = conn.cursor()

    if jksqlTrace:
        from datetime import datetime
        sys.stderr.write("SQL_QUERY 0 %s %s %s %s\n" % (conn.host, conn.db, query, args))
        startTime = datetime.now()

    try:
        rows = cursor.execute(query, args)

        if jksqlTrace:
            timeDiff = _timeDeltaSeconds(datetime.now(), startTime)
            sys.stderr.write("SQL_TIME 0 %s %s %.3f\n" % (conn.host, conn.db, timeDiff))

    except pymysql.Error as errObj:
        # on table not found, try the secondary mysql connection, "slow-db" in hg.conf
        errCode, errDesc = errObj.args
        if errCode!=1146: # "table not found" error
            raise

        if conn.failoverConn == None:
            _sqlConnectFailover(conn)

        if not conn.failoverConn:
            raise

        # stay compatible with the jksql.c JKSQL_TRACE output format
        if jksqlTrace:
            sys.stderr.write("SQL_FAILOVER 0 %s %s db -> slow-db | %s %s\n" % \
                    (conn.host, conn.db, query, args))
        cursor = conn.failoverConn.cursor()
        rows = cursor.execute(query, args)

    if jksqlTrace:
        startTime = datetime.now()

    data = cursor.fetchall()
    cursor.close()

    if jksqlTrace:
        timeDiff = _timeDeltaSeconds(datetime.now(), startTime)
        sys.stderr.write("SQL_FETCH 0 %s %s %.3f\n" % (conn.host, conn.db, timeDiff))

    colNames = [desc[0] for desc in cursor.description]
    Rec = namedtuple("MysqlRow", colNames)

    if forceUnicode:
        data = byteToUnicode(data)

    recs = [Rec(*row) for row in data]

    return recs

def htmlPageEnd(oldJquery=False):
    " close html body/page "
    print("</body>")
    print("</html>")

def printMenuBar(oldJquery=False):
    baseDir = "../"
    " print the menubar. Mostly copied from src/hg/hgMenuBar.c "

    print ("<noscript><div class='noscript'><div class='noscript-inner'><p><b>JavaScript is disabled in your web browser</b></p>")
    print ("<p>You must have JavaScript enabled in your web browser to use the Genome Browser</p></div></div></noscript>\n")

    menuPath = "../htdocs/inc/globalNavBar.inc"
    navBarStr = open(menuPath, "r").read()
    print (navBarStr)

    # fixup old menubar, copied from hgGtexTrackSettings, for now
    print("<link rel='stylesheet' href='../style/gbAfterMenu.css' type='text/css'>")
    print("<link rel='stylesheet' href='../style/gb.css'>")
    print("<link rel='stylesheet' href='../style/hgGtexTrackSettings.css'>")

    print("<div class='container-fluid'>")

def printHgcHeader(assembly, shortLabel, longLabel, addGoButton=True, infoUrl="#INFO_SECTION", infoMouseOver="Jump to the track description"):
    " copied from hgGtexTrackSettings, uses bootstrap styling "
    #print("<form action='../cgi-bin/hgTracks' name='MAIN_FORM' method=POST>")

    print("<a name='TRACK_TOP'></a>")
    print("<div class='row gbTrackTitleBanner'>")
    print("<div class='col-md-10'>")

    if assembly is not None:
        print("<span class='gbTrackName'>")
        print(shortLabel)
        print("<span class='gbAssembly'>%s</span>" % assembly)
        print("</span>")

    print("<span class='gbTrackTitle'>%s</span>" % longLabel)

    print("<a href='%s' title='%s'>" % (infoUrl, infoMouseOver))

    print("<span class='gbIconSmall fa-stack'>")
    print("<i class='gbBlueDarkColor fa fa-circle fa-stack-2x'></i>")
    print("<i class='gbWhiteColor fa fa-info fa-stack-1x'></i>")
    print("</span></a>")
    print("</div>")
    if addGoButton:
        print("<div class='col-md-2 text-right'>")
        print("<div class='gbButtonGoContainer' title='Go to the Genome Browser'>")
        print("<div class='gbButtonGo'>GO</div>")
        print("<i class='gbIconGo fa fa-play fa-2x'></i>")
        print("</div>")
        print("</div>")
    print("</div>")

    print("<!-- Track Configuration Panels -->")
    print("<div class='row'>")
    print("<div class='col-md-12'>")

def printHgcSection(name, rightSideContent, id=None):
    " print a section header for the hgc page "
    print("<!-- Configuration panel -->")
    if id is None:
        print("<div class='row gbSectionBanner'>")
    else:
        print("<div class='row gbSectionBanner' id='%s'>" % id)
    print("<div class='col-md-10'>%s</div>" % name)
    print("<div class='col-md-2 text-right'>")
    print(rightSideContent)
    print("</div>")
    print("</div>")

def getGbHeader():
    " Python cannot include files with the preprocessor, so manually copied kent/src/hg/lib/gbHeader.h for now "
    return """<!DOCTYPE html>
<html>
<head>
    %s
    <meta http-equiv="Content-Type" CONTENT="text/html;CHARSET=iso-8859-1">
    <meta name="description" content="UCSC Genome Browser">
    <meta name="keywords" content="genome, genome browser, human genome, genome assembly, Blat, UCSC, bioinformatics, gene prediction, SNP, SNPs, EST, mRNA, mouse genome, rat genome, chicken genome, chimp genome, dog genome, fruitfly genome, zebrafish genome, xenopus, frog genome, rhesus, opossum, danio rerio genome, drosophila, fugu, yeast, ciona,  comparative genomics, human variation, hapMap">
    <title>UCSC %s</title>
    <link href='https://fonts.googleapis.com/css?family=Oswald:700|Lato:700,700italic,300,400,400italic' rel='stylesheet' type='text/css'>
    <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/font-awesome/4.5.0/css/font-awesome.min.css">
    <link rel='stylesheet' href='https://ajax.googleapis.com/ajax/libs/jqueryui/1.12.1/themes/smoothness/jquery-ui.css' type='text/css'>
    <link rel='stylesheet' href='../style/nice_menu.css' type='text/css'>
    <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css">

<!-- BEGIN added for gene interactions page -->
<script src="https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"></script>
<script src="//code.jquery.com/ui/1.11.0/jquery-ui.min.js"></script>
<script nonce='%s'> /*** Handle jQuery plugin naming conflict between jQuery UI and Bootstrap ***/
$.widget.bridge('uibutton', $.ui.button); $.widget.bridge('uitooltip', $.ui.tooltip);
</script>
<script type='text/javascript' src='../js/jquery.plugins.js'></script>
<script src="//maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.js"></script>
<script nonce='%s'>
$.fn.bsTooltip = $.fn.tooltip.noConflict();
</script>
<!-- END added for gene interactions page -->
</head>
    """

def webStartGbNoBanner(prefix, title):
    " output the <head> part, largely copied from web.c / webStartGbNoBanner "
    print (getGbHeader() % (prefix, title, getNonce(), getNonce()))

def runCmd(cmd, mustRun=True):
    " wrapper around system() that prints error messages. cmd preferably a list, not just a string. "
    import subprocess
    ret = subprocess.call(cmd)
    if ret!=0 and mustRun:
        errAbort("Could not run command %s" % cmd)
    return ret

def printContentType(contType="text/html", status=None, fname=None, headers=None):
    """
    print the HTTP Content-type header line with an optional file name for downloads.
    Also optionally prints the bot delay note. The argument 'status' must be an int.
    """
    global contentLineDone
    if not contentLineDone:
        contentLineDone = True
        print("Content-type: %s; charset=utf-8" % contType)

        if status:
            if status==400:
                print("Status: 400 Bad Request")
            elif status==429:
                print("Status: 429 Too Many Requests")
            else:
                raise Exception("Unknown status code, please update hgLib.py")

        if fname is not None:
            print("Content-Disposition: attachment; filename=%s" % fname)

        if headers:
            for key, val in headers.items():
                print("%s: %s" % (key, val))

        print()  # this newline is essential, it means: end of header lines

    if doWarnBot:
        print ("<div style='background-color:yellow; border:2px solid black'>")
        print ("We have a suspicion that you are an automated web bot software, not a real user. ")
        print ("To keep our site fast for other users, we have slowed down this page. ")
        print ("The slowdown will gradually disappear. ")
        print ("If you think this is a mistake, please contact us at genome-www@soe.ucsc.edu. ")
        print ("Also note that all data for hgGeneGraph can be obtained through our public MySQL server and")
        print ("all our software source code is available and can be installed locally onto your own computer. ")
        print ("If you are unsure how to use these resources, do not hesitate to contact us.")
        print ("</div>")


def botDelayTime(host, port, botString):
    " contact UCSC-style bottleneck server to get current delay time. From hg/lib/botDelay.c:botDelayTime()"
    # send ip address
    import socket
    s =  socket.socket()
    s.connect((host, int(port)))
    msg = botString
    d = chr(len(msg))+msg
    s.send(d.encode("utf8"))

    # read delay time as ASCII chars
    expLen = ord(s.recv(1).decode("utf8"))
    totalLen = 0
    buf = list()
    while True:
        resp = s.recv(1024).decode("utf8")
        buf.append(resp)
        totalLen+= len(resp)
        if totalLen==expLen:
            break
    return int("".join(buf))

# global variable, only used by findCookieData, local use without explicit 'global' will trigger a Python error
cookieData = None

def findCookieData(cookieName):
    " return value of cookie or None is not set, port of lib/cheapcgi.c:findCookieData "
    global cookieData
    if not cookieData:
        if "HTTP_COOKIE" in os.environ:
            cookieData = cookies.SimpleCookie(os.environ["HTTP_COOKIE"])
        else:
            cookieData = {}

    # unlike cheapcgi, Python does not even allow duplicate cookies, so no need to handle this case
    cookie = cookieData.get(cookieName)
    if cookie:
        return cookie.value

    return None

def getCookieUser():
    " port of lib/botDelay.c:getCookieUser: get hguid cookie value  "
    user = None
    centralCookie = cfgOption("central.cookie", default="hguid")

    if centralCookie:
        user = findCookieData(centralCookie)

    return user

def showCookieError():
    " output error message if cookie not found "
    print("Content-type: text/html\n\n")
    print("<html><body>")
    print("Sorry, the gene interactions viewer requires that you visit the genome browser first once, to defend against bots. ")
    print("<a href='hgTracks'>Click here</a> to visit the genome browser, then come back to this page.")
    print("</body></html>")
    sys.exit(0)

def getBotCheckString(ip, fraction):
    " port of lib/botDelay.c:getBotCheckString: compose user.ip fraction for bot check  "
    userId = getCookieUser()

    if not userId:
        showCookieError()

    botCheckString = "uid%s %f" % (userId, fraction)

    return botCheckString

def hgBotDelay(fraction=1.0, useBytes=None, botCheckString=None):
    """
    Implement bottleneck delay, get bottleneck server from hg.conf.
    This behaves similar to the function src/hg/lib/botDelay.c:hgBotDelay
    It does not use the hgsid, currently it always uses the IP address.
    Using the hgsid makes little sense. It is more lenient than the C version.

    If useBytes is set, use only the first x bytes of the IP address. This helps
    block bots that all use similar IP addresses, at the risk of blocking
    entire institutes.
    """
    global hgConf
    global doWarnBot
    global botDelayMsecs

    ip = os.environ.get("REMOTE_ADDR")
    if not ip: # skip if not called from Apache
        return
    if useBytes is not None and ip.count(".")==3: # do not do this for ip6 addresses
        ip = ".".join(ip.split(".")[:useBytes])

    host = cfgOption("bottleneck.host")
    port = cfgOption("bottleneck.port")

    if not host or not port or not ip:
        return

    warnMsg = None
    if botCheckString is None:
        botCheckString = getBotCheckString(ip, fraction)
    else:
        warnMsg = "Too many parallel requests for this CGI program. Please wait for a while and try this page again. If the problem persists, "
        "please email us at genome@soe.ucsc.edu."

    millis = botDelayTime(host, port, botCheckString)
    debug(1, "Bottleneck delay: %d msecs" % millis)
    botDelayMsecs = millis

    if millis > (botDelayBlock/fraction):
        # retry-after time factor 10 is based on the example in the bottleneck help message
        sys.stderr.write("hgLib.py hogExit\n")
        printContentType(status=429, headers={"Retry-after" : str(millis / 200)})
        print("<html><head></head><body>")
        if warnMsg:
            print(warnMsg)
            print(millis)
            print(botDelayBlock)
            print(fraction)
        else:
            print("<b>Too many HTTP requests and not enough delay between them.</b><p> "
            "Your IP has been blocked to keep this website responsive for other users. "
            "Please contact genome-www@soe.ucsc.edu to unblock your IP address, especially if you were just browsing our site and are not running a bot,"
            "We can help you obtain the data you need without web crawling.<p>")
        print("</html>")
        sys.exit(0)

    if millis > (botDelayWarn/fraction):
        printContentType(status=429, headers={"Retry-after" : str(millis / 1000.0)})
        time.sleep(millis/1000.0)
        doWarnBot = True # = show warning message later in printContentType()

def parseRa(text):
    " Parse ra-style string and return as dict name -> value "
    import string
    lines = text.split("\n")
    data = dict()
    for l in lines:
        if len(l)==0:
            continue
        key, val = l.split(" ", maxsplit=1)
        data[key] = val
    return data

def lineFileNextRow(inFile):
    """
    parses tab-sep file with headers in first line. Yields collection.namedtuples.
    Strips "#"-prefix from header line.
    Cannot parse headers with non-alpha characters and fields that are not ASCII. Code
    for these cases is commented out, for performance reasons.
    """
    import gzip, string
    if isinstance(inFile, str):
        if inFile.endswith(".gz"):
            fh = gzip.open(inFile, 'rb')
        else:
            fh = open(inFile)
    else:
        fh = inFile

    line1 = fh.readline()
    line1 = line1.rstrip("\n").lstrip("#")
    headers = line1.split("\t")

    Record = namedtuple('tsvRec', headers)
    for line in fh:
        if line.startswith("#"):
            continue
        line = line.rstrip("\n")
        fields = line.split("\t", maxsplit=len(headers)-1)
        try:
            rec = Record(*fields)
        except Exception as msg:
            logging.error("Exception occured while parsing line, %s" % msg)
            logging.error("Filename %s" % fh.name)
            logging.error("Line was: %s" % line)
            logging.error("Does number of fields match headers?")
            logging.error("Headers are: %s" % headers)
            raise Exception("header count: %d != field count: %d wrong field count in line %s" % (len(headers), len(fields), line))
        yield rec

def parseDict(fname):
    """ Parse text file in format key<tab>value<newline> and return as dict key->val.
    Does not abort on duplicate keys, for performance reasons.
    """
    import gzip
    d = {}

    if fname.endswith(".gz"):
        fh = gzip.open(fname)
    else:
        fh = open(fname)

    for line in fh:
        key, val = line.rstrip("\n").split("\t")
        d[key] = val
    return d

def gbdbReplace(fname, hgConfSetting):
    " hdb.c: replace /gbdb/ in fname with hgConfSetting "
    if not fname.startswith("/gbdb/"):
        return None

    gbdbLoc = hgConf.get(hgConfSetting)
    if gbdbLoc is None:
        return None

    return fname.replace("/gbdb/", gbdbLoc)

def getUdcCacheDir():
    " return the udc cache dir and create it if it doesn't exist "
    udcDir = hgConf.get("udc.cacheDir", "/tmp/udcCache")
    if not isdir(udcDir):
        os.makedirs(udcDir)
    return udcDir

def hGbdbReplace(fname):
    " hdb.c : get the local filename on disk or construct a URL to a /gbdb file and return it "
    if not isfile(fname):
    # try using gbdbLoc1
        fname2 = gbdbReplace(fname, "gbdbLoc1")
        if fname2 and isfile(fname2):
            return fname2

    if not isfile(fname):
    # try using gbdbLoc2, which can be a URL
        fname2 = gbdbReplace(fname, "gbdbLoc2")
        return fname2
    return fname

def netUrlOpen(url):
    " net.c: open a URL and return a file object "
    import errno
    from socket import error as SocketError

    # let our webservers know that we are not a Firefox
    opener = urllib.request.build_opener()
    opener.addheaders = [('User-Agent', 'Genome Browser pyLib/hgLib.py:netUrlOpen()')]

    resp = None
    for x in range(5): # limit number of retries
      try:
        resp = opener.open(url)
        break
      except SocketError as e:
        if e.errno != errno.ECONNRESET:
          raise # re-raise any other error
        time.sleep(1)
    return resp

def readSmallFile(fname):
    """ read a small file, usually from /gbdb/, entirely into memory and return lines.
    If the file doesn't exist, try gbdbLoc1, then gbdbLoc2
    and keep a local copy if only found on gbdbLoc2.

    This is similar but not the same as the UDC system in the kent C code, but
    for small files and a complete read over it, the UDC system may be overkill
    for Python.
    """
    fname = hGbdbReplace(fname)

    if fname.startswith("http"):
        # download to local disk; local filename is the hash of the URL
        import hashlib
        udcCacheDir = getUdcCacheDir()
        m = hashlib.md5()
        m.update(fname)
        fileExt = splitext(fname)[-1]
        tmpFname = join(udcCacheDir, "hgLibCache_"+m.hexdigest()+"."+fileExt)
        if not isfile(tmpFname):
            data = netUrlOpen(fname).read()
            fh = open(tmpFname, "w")
            fh.write(data)
            fh.close()
        fname = tmpFname

    if fname.endswith(".gz"):
        import gzip
        fh = gzip.open(fname)
    else:
        fh = open(fname)

    lines = fh.read().splitlines()
    return lines

def cgiString(name, default=None):
    " get named cgi variable as a string, like lib/cheapcgi.c "
    val = cgiArgs.getfirst(name, default=default)
    return val

def cgiGetAll():
    return cgiArgs

def makeRandomKey(numBits=128+33):
    " copied line-by-line from kent/src/lib/htmlshell.c:makeRandomKey "
    import base64
    numBytes = (numBits + 7) / 8  # round up to nearest whole byte.
    numBytes = int(((numBytes+2)/3)*3) # round up to the nearest multiple of 3 to avoid equals-char padding in base64 output
    f = open("/dev/urandom", "rb") # open random system device for read-only access.
    binaryString = f.read(numBytes)
    f.close()
    return base64.b64encode(binaryString, b"Aa").decode("latin1")  # replace + and / with characters that are URL-friendly.


# ============ Nonce and CSP functions =============

nonce = None;

def getNonce():
    " make nonce one-use-per-page "
    global nonce
    if nonce:
        return nonce
    nonce = makeRandomKey(128+33) # at least 128 bits of protection, 33 for the world population size.
    return nonce

def getNoncePolicy():
    " get nonce policy clause "
    return "'nonce-" + getNonce() + "'" 

def getCspPolicyString():
    " get the policy string "
    # example "default-src 'self'; child-src 'none'; object-src 'none'"
    policy = ""
    policy += "default-src *;"

    '''
    # more secure method not used yet 
    policy += "default-src 'self';"
    policy += "  child-src 'self';"
    '''

    policy += " script-src 'self' blob:"
    # Trick for backwards compatibility with browsers that understand CSP1 but not nonces (CSP2).
    policy += " 'unsafe-inline'"
    # For browsers that DO understand nonces and CSP2, they ignore 'unsafe-inline' in script if nonce is present.
    policy += " " + getNoncePolicy()

    # used by hgIntegrator jsHelper and others
    policy += " code.jquery.com/jquery-1.9.1.min.js"
    policy += " code.jquery.com/jquery-1.12.3.min.js"
    policy += " code.jquery.com/ui/1.10.3/jquery-ui.min.js"
    policy += " code.jquery.com/ui/1.11.0/jquery-ui.min.js"
    policy += " code.jquery.com/ui/1.12.1/jquery-ui.js"

    policy += " www.google-analytics.com/analytics.js" # used by google analytics
    policy += " www.googletagmanager.com/gtag/js"

    #cirm cdw lib and web browse
    policy += " www.samsarin.com/project/dagre-d3/latest/dagre-d3.js"

    policy += " cdnjs.cloudflare.com/ajax/libs/bowser/1.6.1/bowser.min.js"
    policy += " cdnjs.cloudflare.com/ajax/libs/d3/3.4.4/d3.min.js"
    policy += " cdnjs.cloudflare.com/ajax/libs/jquery/1.12.1/jquery.min.js"
    policy += " cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/jstree.min.js"
    policy += " cdnjs.cloudflare.com/ajax/libs/jstree/3.3.4/jstree.min.js"
    policy += " cdnjs.cloudflare.com/ajax/libs/jstree/3.3.7/jstree.min.js"
    policy += " cdnjs.cloudflare.com/ajax/libs/popper.js/1.12.9/umd/popper.min.js"


    policy += " login.persona.org/include.js"
    # expMatrix
    policy += " ajax.googleapis.com/ajax/libs/jquery/1.11.0/jquery.min.js"
    policy += " ajax.googleapis.com/ajax/libs/jquery/1.11.3/jquery.min.js"
    policy += " ajax.googleapis.com/ajax/libs/jquery/1.12.0/jquery.min.js"
    policy += " ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"
    policy += " ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"

    policy += " d3js.org/d3.v3.min.js"
    # jsHelper
    policy += " cdn.datatables.net/1.10.12/js/jquery.dataTables.min.js"

    # hgGeneGraph

    policy += " maxcdn.bootstrapcdn.com/bootstrap/3.3.5/js/bootstrap.min.js"
    policy += " maxcdn.bootstrapcdn.com/bootstrap/3.3.6/js/bootstrap.min.js"
    policy += " maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.js"
    policy += " maxcdn.bootstrapcdn.com/bootstrap/3.4.1/js/bootstrap.min.js"
    policy += " maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js"


    policy += " cdn.rawgit.com/jedfoster/Readmore.js/master/readmore.min.js"

    policy += ";"

    policy += " style-src * 'unsafe-inline';"

    '''
    # more secure method not used yet 
    policy += " style-src 'self' 'unsafe-inline'"
    policy += " code.jquery.com"         # used by hgIntegrator
    policy += " netdna.bootstrapcdn.com" # used by hgIntegrator
    policy += " fonts.googleapis.com"    # used by hgGateway
    policy += " maxcdn.bootstrapcdn.com" # used by hgGateway
    policy += ";"
    '''

    # The data: protocol is used by popular browser extensions.
    # It seems to be safe and it is too bad that it must be explicitly included.
    policy += " font-src * data:;"

    '''
    /* more secure method not used yet 
    policy += " font-src 'self'"
    policy += " netdna.bootstrapcdn.com" # used by hgIntegrator
    policy += " maxcdn.bootstrapcdn.com" # used by hgGateway
    policy += " fonts.gstatic.com"       # used by hgGateway
    policy += ";"

    dyStringAppend(policy, " object-src 'none';");

    '''

    policy += " img-src * data:;"  

    '''
    # more secure method not used yet 
    policy += " img-src 'self'"
    # used by hgGene for modbaseimages in hg/hgc/lowelab.c hg/protein/lib/domains.c hg/hgGene/domains.c
    policy += " modbase.compbio.ucsf.edu");  
    policy += " hgwdev.soe.ucsc.edu"); # used by visiGene
    policy += " genome.ucsc.edu");     # used by visiGene
    policy += " code.jquery.com");          # used by hgIntegrator
    policy += " www.google-analytics.com"); # used by google analytics
    policy += " stats.g.doubleclick.net");  # used by google analytics
    policy += ";"
    '''

    return policy

def getCspMetaString(policy):
    " get the policy string as an html header meta tag "
    # use double quotes around policy because it contains single-quoted values.
    return "<meta http-equiv='Content-Security-Policy' content=\"" + policy + "\">\n" 

def getCspMetaResponseHeader(policy):
    " get the policy string as an http response header "
    return "Content-Security-Policy: " + policy + "\n"

def getCspMetaHeader():
    " return meta CSP header string "
    return getCspMetaString(getCspPolicyString())

#============ javascript inline-separation routines ===============

'''
// One of the main services that CSP (Content Security Policy) provides
// is protecting from reflected and stored XSS attacks by disabling all inline javacript,
// both in script tags, and in inline event handlers.  The separated javascript 
// can be either added back to the end of the html page with a nonce or sha hashid,
// or it can be saved to a temp file in trash and then included as a non-inline, off-page .js.
'''

jsInlineLines = ""

def jsInline(javascript):
    " Add javascript text to output file or memory structure "
    global jsInlineLines
    jsInlineLines += javascript

def jsInlineF(format, *args):
    " Add javascript text to output file or memory structure "
    jsInline(format % args)

jsInlineFinishCalled = False;

def jsInlineFinish():
    " finish outputting accumulated inline javascript "
    global jsInlineFinishCalled
    global jsInlineLines
    if jsInlineFinishCalled:
        # jsInlineFinish can be called multiple times when generating framesets or genomeSpace.
        warn("jsInlineFinish() called already.")
    print("<script type='text/javascript' nonce='%s'>\n%s</script>\n" % (getNonce(), jsInlineLines))
    jsInlineLines = ""
    jsInlineFinishCalled = True

def jsInlineReset():
    " used by genomeSpace to repeatedly output multiple pages to stdout "
    global jsInlineFinishCalled
    jsInlineFinishCalled = False

jsEvents = [ 
"abort",
"activate",
"afterprint",
"afterupdate",
"beforeactivate",
"beforecopy",
"beforecut",
"beforedeactivate",
"beforeeditfocus",
"beforepaste",
"beforeprint",
"beforeunload",
"beforeupdate",
"blur",
"bounce",
"cellchange",
"change",
"click",
"contextmenu",
"controlselect",
"copy",
"cut",
"dataavailable",
"datasetchanged",
"datasetcomplete",
"dblclick",
"deactivate",
"drag",
"dragend",
"dragenter",
"dragleave",
"dragover",
"dragstart",
"drop",
"error",
"errorupdate",
"filterchange",
"finish",
"focus",
"focusin",
"focusout",
"hashchange",
"help",
"input",
"keydown",
"keypress",
"keyup",
"load",
"losecapture",
"message",
"mousedown",
"mouseenter",
"mouseleave",
"mousemove",
"mouseout",
"mouseover",
"mouseup",
"mousewheel",
"move",
"moveend",
"movestart",
"offline",
"line",
"online",
"paste",
"propertychange",
"readystatechange",
"reset",
"resize",
"resizeend",
"resizestart",
"rowenter",
"rowexit",
"rowsdelete",
"rowsinserted",
"scroll",
"search",
"select",
"selectionchange",
"selectstart",
"start",
"stop",
"submit",
"unload"
 ]

jsEventDic = None

def findJsEvent(event):
    " see if it is in the list "
    global jsEventDic
    # init event dic
    if jsEventDic == None:
        jsEventDic = {}
    for w in jsEvents:
        jsEventDic[w] = True
    if jsEventDic[event]:
        return True
    return False

def checkValidEvent(event):
    " check if it is lowercase and a known valid event name "
    # TODO GALT
    temp = event.lower()
    if temp != event:
        warn("jsInline: javascript event %s should be given in lower-case", event)
    event = temp; 
    if not findJsEvent(event):
        warn("jsInline: unknown javascript event %s", event)

def jsOnEventById(eventName, idText, jsText):
    " Add js mapping for inline event "
    checkValidEvent(eventName)
    jsInlineF("document.getElementById('%s').on%s = function(event) {if (!event) {event=window.event}; %s};\n", idText, eventName, jsText)

def jsOnEventByIdF(eventName, idText, format, *args):
    " Add js mapping for inline event with printf formatting "
    checkValidEvent(eventName)
    jsInlineF("document.getElementById('%s').on%s = function(event) {if (!event) {event=window.event}; ", idText, eventName)
    jsInlineF(format, *args)
    jsInlineF("};\n")

#============ END of javascript inline-separation routines ===============

def cartDbLoadFromId(conn, table, cartId, oldCart):
    " Like src/hg/lib/cart.c, opens cart table and parses cart contents given a cartId of the format 123123_csctac "
    if cartId==None:
        return {}
    cartFields = cartId.split("_")
    if len(cartFields)!=2:
        errAbort("Could not parse identifier %s for cart table %s" % (cgi.escape(cartId), table))
    idStr, secureId = cartFields

    query = "SELECT contents FROM "+table+" WHERE id=%(id)s and sessionKey=%(sessionKey)s"
    rows = sqlQuery(conn, query, {"id":idStr, "sessionKey":secureId})
    if len(rows)==0:
        # invalid cart ID
        return None

    cartList = urllib.parse.parse_qs(rows[0][0])

    # by default, python returns a dict with key -> list of vals. We need only the first one
    for key, vals in cartList.items():
        oldCart[key] =vals[0]
    return oldCart

centralConn = None

def hConnectCentral():
    " similar to src/hg/lib/hdb.c:hConnectCentral. We use a much simpler cache, because we usually read all rows into memory. "
    global centralConn
    if centralConn:
        return centralConn

    centralDb = cfgOption("central.db")
    if centralDb is None:
        errAbort("Could not find central.db in hg.conf. Installation error.")

    centralUser = cfgOption("central.user")
    if centralUser is None:
        errAbort("Could not find central.user in hg.conf. Installation error.")

    centralPwd = cfgOption("central.password")
    if centralPwd is None:
        errAbort("Could not find central.password in hg.conf. Installation error.")

    centralHost = cfgOption("central.host")
    if centralHost is None:
        errAbort("Could not find central.host in hg.conf. Installation error.")

    conn = sqlConnect(centralDb, host=centralHost, user=centralUser, passwd=centralPwd)

    centralConn = conn
    return conn

def cartNew(conn, table):
    " create a new cart and return ID "
    sessionKey = makeRandomKey()
    sqlQuery(conn, "INSERT %s VALUES(0,'',0,now(),now(),0,%s" % (table, sessionKey));
    sqlLastId = conn.insert_id()
    newId = "%d_%s" % (sqlLastId, sessionKey)
    return newId

def cartAndCookieSimple():
    """ Make the cart from the user cookie (user settings) and the hgsid CGI parameter (session settings, e.g. a browser tab).
        This is somewhat similar to cartAndCookie from hg/lib/cart.c
        Two important differences: this cart does not add all CGI arguments automatically.
        It also does not run cart.c:cartJustify, so track priorities are not applied.
        Also, if there is no hgsid parameter or no cookie, we do not create a new cart.
    """
    # no cgiApoptosis yet - maybe needed in the future. see cart.c / cartNew

    hguid = getCookieUser()
    hgsid = cgiString("hgsid")

    conn = hConnectCentral()

    cart = {}
    userInfo = cartDbLoadFromId(conn, "userDb", hguid, cart)
    if userInfo is None:
        # invalid cookie hguid
        showCookieError()

    sessionInfo = cartDbLoadFromId(conn, "sessionDb", hgsid, cart)
    if sessionInfo is None:
        # tolerate invalid hgsid
        sessionInfo = {}
    return cart

def cartString(cart, default=None):
    " Get a string from the cart. For better readability for programmers used to the C code. "
    return cart.get(cart, default)

def cgiSetup(bottleneckFraction=1.0, useBytes=None, botCheckString=None):
    """ do the usual browser CGI setup: parse the hg.conf file, parse the CGI
    variables, get the cart, do bottleneck delay. Returns the cart.

    This is not part of the C code (though it maybe should).
    """
    parseHgConf()
    global cgiArgs
    cgiArgs = cgi.FieldStorage() # Python has built-in cgiSpoof support: sys.argv[1] is the query string if run from the command line

    if cgiString("debug"):
        global verboseLevel
        verboseLevel = int(cgiString("debug"))

    hgBotDelay(fraction=bottleneckFraction, useBytes=useBytes, botCheckString=botCheckString)

    cart = cartAndCookieSimple()
    return cart

#if __file__=="__main__":
    #pass
