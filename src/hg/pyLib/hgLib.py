# Library functions for genome browser CGI scripts written in Python 2.7

# Because this library is loaded for every CGI execution, only a
# fairly minimal set of functions is implemented here, e.g. hg.conf parsing,
# bottleneck, cart loading, mysql queries.

# The cart is currently read-only. More work is needed to allow writing a cart.

# General rules for CGI in Python:
# - never insert values into SQL queries. Write %s in the query and provide the
#   arguments to sqlQuery as a list.  
# - never print incoming HTTP argument as raw text. Run it through cgi.escape to 
#   destroy javascript code in them.

# Non-standard imports. They need to be installed on the machine. 
# We provide a pre-compiled library as part of our cgi-bin distribution as a
# fallback in the "pyLib" directory. The idea of having pyLib be the last
# directory in sys.path is that the system MySQLdb takes precedence.
try:
    import MySQLdb
except:
    print "Installation error - could not load MySQLdb for Python. Please tell your system administrator to run " \
        "one of these commands as root: 'yum install MySQL-python', 'apt-get install python-mysqldb' or 'pip install MySQL-python'."
    exit(0)

# Imports from the Python 2.7 standard library
# Please minimize global imports. Each library import can take up to 20msecs.
import os, cgi, sys, logging

from os.path import join, isfile, normpath, abspath, dirname
from collections import namedtuple

# activate debugging output output only on dev
import platform
if "hgwdev" in platform.node():
    import cgitb
    cgitb.enable()

# debug level: a number. the higher, the more debug info is printed
verboseLevel = None

cgiArgs = None

# like in the kent tree, we keep track of whether we have already output the content-type line
contentLineDone = False

jksqlTrace = False

def errAbort(msg):
    " show msg and abort. Like errAbort.c "
    if not contentLineDone:
        printContentType()
    print msg
    exit(0)

def debug(level, msg):
    " output debug message with a given verbosity level "
    if level >= verboseLevel:
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
            key, value = line.split("=")
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
    conn = MySQLdb.connect(host=host, user=user, passwd=passwd, db=db)

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
    failoverConn = MySQLdb.connect(host=host, user=user, passwd=passwd, db=db)
    conn.failoverConn = failoverConn

def _timeDeltaSeconds(time1, time2):
    " convert time difference to total seconds for python 2.6 "
    td = time1 - time2
    return (td.microseconds + (td.seconds + td.days * 24 * 3600) * 10**6) / 10**6

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

    except MySQLdb.Error, errObj:
        # on table not found, try the secondary mysql connection, "slow-db" in hg.conf
        errCode, errDesc = errObj
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
    recs = [Rec(*row) for row in data]
    return recs

def htmlPageEnd(oldJquery=False):
    " close html body/page "
    print "</body>"
    print "</html>"

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
<script> /*** Handle jQuery plugin naming conflict between jQuery UI and Bootstrap ***/
$.widget.bridge('uibutton', $.ui.button); $.widget.bridge('uitooltip', $.ui.tooltip);
</script>
<script type='text/javascript' src='../js/jquery.plugins.js'></script>
<script src="//maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.js"></script>
<script>
$.fn.bsTooltip = $.fn.tooltip.noConflict();
</script>
<!-- END added for gene interactions page -->
</head>
    """

def webStartGbNoBanner(prefix, title):
    " output the <head> part, largely copied from web.c / webStartGbNoBanner "
    print (getGbHeader() % (prefix, title))

def runCmd(cmd, mustRun=True):
    " wrapper around system() that prints error messages. cmd preferably a list, not just a string. "
    import subprocess
    ret = subprocess.call(cmd)
    if ret!=0 and mustRun:
        errAbort("Could not run command %s" % cmd)
    return ret

def printContentType(contType="text/html", fname=None):
    " print the HTTP Content-type header line with an optional file name "
    global contentLineDone
    contentLineDone = True
    print("Content-type: %s; charset=utf-8" % contType)
    if fname is not None:
        print("Content-Disposition: attachment; filename=%s" % fname)
    print

def queryBottleneck(host, port, ip):
    " contact UCSC-style bottleneck server to get current delay time. From hg/lib/botDelay.c "
    # send ip address
    import socket
    s =  socket.socket()
    s.connect((host, int(port)))
    msg = ip
    d = chr(len(msg))+msg
    s.send(d)

    # read delay time
    expLen = ord(s.recv(1))
    totalLen = 0
    buf = list()
    while True:
        resp = s.recv(1024)
        buf.append(resp)
        totalLen+= len(resp)
        if totalLen==expLen:
            break
    return int("".join(buf))

def hgBotDelay():
    """
    Implement bottleneck delay, get bottleneck server from hg.conf.
    This behaves similar to the function src/hg/lib/botDelay.c:hgBotDelay
    It does not use the hgsid, currently it always uses the IP address.
    Using the hgsid makes little sense. It is more lenient than that C version.
    """
    import time
    if "DOCUMENT_ROOT" not in os.environ: # skip if not called from Apache
        return
    global hgConf
    hgConf = parseHgConf()
    if "bottleneck.host" not in hgConf:
        return
    ip = os.environ["REMOTE_ADDR"]
    delay = queryBottleneck(hgConf["bottleneck.host"], hgConf["bottleneck.port"], ip)
    if delay>10000:
        time.sleep(delay/1000.0)
    if delay>20000:
        errAbort("Too many queries. Your IP has been blocked. Please contact genome-www@soe.ucsc.edu to unblock your IP address.")
        sys.exit(0)

def parseRa(text):
    " Parse ra-style string and return as dict name -> value "
    import string
    lines = text.split("\n")
    data = dict()
    for l in lines:
        if len(l)==0:
            continue
        key, val = string.split(l, " ", maxsplit=1)
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
    #headers = [re.sub("[^a-zA-Z0-9_]","_", h) for h in headers]
    #headers = [x if x!="" else "noName" for x in headers]

    #filtHeads = []
    #for h in headers:
        #if h[0].isdigit():
            #filtHeads.append("x"+h)
        #else:
            #filtHeads.append(h)
    #headers = filtHeads

    Record = namedtuple('tsvRec', headers)
    for line in fh:
        if line.startswith("#"):
            continue
        #line = line.decode("latin1")
        line = line.rstrip("\n")
        fields = string.split(line, "\t", maxsplit=len(headers)-1)
        try:
            rec = Record(*fields)
        except Exception, msg:
            logging.error("Exception occured while parsing line, %s" % msg)
            logging.error("Filename %s" % fh.name)
            logging.error("Line was: %s" % line)
            logging.error("Does number of fields match headers?")
            logging.error("Headers are: %s" % headers)
            raise Exception("header count: %d != field count: %d wrong field count in line %s" % (len(headers), len(fields), line))
        yield rec

def parseDict(fname):
    """ Parse text file in format key<tab>value<newline> and return as dict key->val.
    Does not abort on duplicate keys, for performance reasons. """
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
    numBytes = ((numBytes+2)/3)*3 # round up to the nearest multiple of 3 to avoid equals-char padding in base64 output
    f = open("/dev/urandom", "r") # open random system device for read-only access.
    binaryString = f.read(numBytes)
    f.close()
    return base64.b64encode(binaryString, "Aa") # replace + and / with characters that are URL-friendly.

def cartDbLoadFromId(conn, table, cartId, oldCart):
    " Like src/hg/lib/cart.c, opens cart table and parses cart contents given a cartId of the format 123123_csctac "
    import urlparse

    if cartId==None:
        return {}
    cartFields = cartId.split("_")
    if len(cartFields)!=2:
        errAbort("Could not parse identifier %s for cart table %s" % (cgi.escape(cartId), table))
    idStr, secureId = cartFields

    query = "SELECT contents FROM "+table+" WHERE id=%(id)s and sessionKey=%(sessionKey)s"
    rows = sqlQuery(conn, query, {"id":idStr, "sessionKey":secureId})
    if len(rows)==0:
        # silently ignore invalid cart IDs for now. Future code may want to generate a new cart.
        return {}

    cartList = urlparse.parse_qs(rows[0][0])

    # by default, python returns a dict with key -> list of vals. We need only the first one
    for key, vals in cartList.iteritems():
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
    import Cookie

    # no cgiApoptosis yet - maybe needed in the future. see cart.c / cartNew

    if "HTTP_COOKIE" in os.environ:
        cookies = Cookie.SimpleCookie(os.environ["HTTP_COOKIE"])
        cookieName = cfgOption("central.cookie", "hguid")
        hguid = cookies.get(cookieName)
        if hguid is not None:
            hguid = hguid.value # cookies have values and other attributes. We only need the value
    else:
        hguid = None

    hgsid = cgiString("hgsid")

    conn = hConnectCentral()

    cart = {}
    userInfo = cartDbLoadFromId(conn, "userDb", hguid, cart)
    sessionInfo = cartDbLoadFromId(conn, "sessionDb", hgsid, cart)
    return cart

def cartString(cart, default=None):
    " Get a string from the cart. For better readability for programmers used to the C code. "
    return cart.get(cart, default)

def cgiSetup():
    """ do the usual browser CGI setup: parse the hg.conf file, parse the CGI
    variables, get the cart, do bottleneck delay. Returns the cart.

    This is not part of the C code (though it maybe should).
    """
    parseHgConf()
    global cgiArgs
    cgiArgs = cgi.FieldStorage() # Python has built-in cgiSpoof support: sys.argv[1] is the query string if run from the command line

    hgBotDelay()

    if cgiString("debug"):
        global verboseLevel
        verboseLevel = int(cgiString("debug"))

    cart = cartAndCookieSimple()
    return cart

#if __file__=="__main__":
    #pass
