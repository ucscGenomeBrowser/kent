import sys, os

# require 2.4 or newer
if (sys.version_info[0] <= 2) and (sys.version_info[1] < 4):
    raise Exception("python 2.4 or newer required, using " + sys.version)

def gbAbsBinDir():
    "get the absolute path to the bin directory"
    return os.path.abspath(os.path.normpath(os.path.dirname(sys.argv[0])))

def gbSetupPath():
    "setup PATH to include gbRoot bin directories"
    absBinDir = gbAbsBinDir()
    mach = os.uname()[4]
    if mach == "i686":
        mach = "i386"
    machBinPath = absBinDir + "/" + mach
    if mach != "i386":
        machBinPath += ":" + absBinDir + "/i386" # use on x86_64 for now
    os.environ["PATH"] = machBinPath + ":" + os.environ["PATH"]

def gbSetupHgConf():
    "setup the HGDB_CONF environment variable as needed"
    # only set HGDB_CONF to one of the gbRoot/etc ones if we are user genbank
    if os.environ["USER"] == "genbank":
        host = os.uname()[1]
        if host == "hgwbeta":
            hgconf = "etc/.hg.mysqlbeta.conf"
        elif host == "hgnfs1":
            hgconf = "etc/.hg.mysqlrr.conf"
        else:
            hgconf = "etc/.hg.conf"
        os.environ["HGDB_CONF"] = os.path.abspath(hgconf)
