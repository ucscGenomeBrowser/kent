#
# genbank configuration file parser object
#
# $Id: Config.py,v 1.9 2006/06/01 01:15:09 markd Exp $
#

import re, string

class ConfigParser(object):
    "parse config file"

    # REs for parsing, which trim leading and training blanks
    ignoreRe = re.compile("^\s*(#.*)?$") # blank or comment
    keyValRe = re.compile("^\s*([^\s=]+)\s*=\s*(.*)\s*$")

    # RE for splitting line around variable reference
    varRe = re.compile("^([^\$]*)(\$*\{([^\}]+)\}(.*))?$")

    # RE for directing and parsing a database in a key, assumes if the
    # first word starts with letters, then has numbers, and optionally
    # more letters, it is a database.
    dbRe = re.compile("^([a-zA-Z]+[0-9]+[a-zA-Z]*)\\.")

    def __init__(self, conf):
        self.conf = conf
        self.fh = None
        self.lineNum = 0

    def _splitValue(self, value):
        """split a conf value substring at the first variable reference and
        return (beforeRef, varRef, afterRef).  If no more variable references,
        varRef and afterRef are undef"""
        m = ConfigParser.varRe.match(value)
        if m == None:
            raise Exception(self.fh.name + ":" + str(self.lineNum) + ": bug, can't split line ")
        (beforeRef, junk, varName, afterRef) = m.groups()
        return (beforeRef, varName, afterRef)

    def _expandVar(self, varName):
        val = self.conf.get("var." + varName)
        if val == None:
            raise Exception(self.fh.name + ":" + str(self.lineNum) + ": reference to undefined variable: " + varName)
        return val

    def _expandVars(self, value):
        "expand a value, replacing variable refererences with their value"
        expVal = ""
        while value != None:
            (beforeRef, varName, afterRef) = self._splitValue(value)
            expVal += beforeRef
            if varName != None:
                expVal += self._expandVar(varName)
            value = afterRef
            
        return expVal.strip()

    def _parseLine(self, line):
        "parse a line from the conf file and key/value to object"
        if not ConfigParser.ignoreRe.match(line):
            mvar = ConfigParser.keyValRe.match(line)
            if mvar == None:
                raise Exception(fh.name + ":" + "can't parse conf line: " + line)
            self.conf[mvar.group(1)] = self._expandVars(mvar.group(2))
            mdb = ConfigParser.dbRe.match(mvar.group(1))
            if mdb != None:
                self.conf.dbs.add(mdb.group(1))

    def parse(self, confFile):
        "parse conf file"
        self.fh = open(confFile)
        for line in self.fh:
            self.lineNum += 1
            self._parseLine(line)
        self.fh.close()
        

class Config(dict):
    "object to parse and contain the genbank.conf"

    def __init__(self, confFile):
        "read conf file into object"
        self.confFile = confFile
        self.dbs = set()
        parser= ConfigParser(self)
        parser.parse(confFile)

    def getStr(self, key):
        "get a configuration value, or error if not found"
        value = self.get(key)
        if value == None:
            raise Exception("no " + key + " in " + self.confFile);
        return value

    def getInt(self, key):
        "get an integer configuration value, or error if not found"
        return int(self.get(key))

    def getStrNo(self, key):
        """Get a configuration value, or error if not defined.  If the value
        'no', None is returned."""
        value = self.getConf(key);
        if value == "no":
            return None
        else:
            return value

    def getDbStrNone(self, db, key):
        """get a configuration value for a database, or the default, or None
        if neither are specified"""
        
        value = self.get(db + "." + key)
        if value == None:
            value = self.get("default." + key)
        return value

    def getDbWordsNone(self, db, key):
        """get a configuration value for a database, or the default, or None
        if neither are specified. split white-space separated words."""
        value = self.getDbStrNone(db, key)
        if value == None:
            return None;
        return string.split(value)

    def getDbStr(self, db, key):
        """get a configuration value for a database, or the default, or error
        if neither are specified"""
        value = self.getDbStrNone(db, key)
        if value == None:
            raise Exception("no " + db + "." + key + " or default." + key
                            + " in " + self.confFile)
        return value

    def getDbStrNo(self, db, key):
        """get a configuration value for a database, or the default, or an error
        if neither are specified.  If the values is no, None is returned."""
        value = self.getDbStr(db, key)
        if value == "no":
            return None
        else:
            return value

    def getvDbStrNo(self, db, *keys):
        """Combine keys into '.' separate key. Get a configuration value for a
        database, or the default, or an error if neither are specified.  If
        the values is empty or no, None is returned."""
        key = ".".join(keys)
        value = self.getDbStr(db, key)
        if value == "no":
            return None
        else:
            return value

    def getDbInt(self, db, key):
        """get a interger configuration value for a database, or the default, or error
        if neither are specified"""
        return int(self.getDbStr(db, key))

    def getDbIntNone(self, db, key):
        """get a interger configuration value for a database, or the default, or None
        if neither are specified"""
        val = self.getDbStrNone(db, key)
        if val != None:
            return int(val)
        else:
            return None

    def getDbIntDefault(self, db, key, default):
        """get a interger configuration value for a database, or the conf file default, or
        the supplied default"""
        val = self.getDbStrNone(db, key)
        if val != None:
            return int(val)
        else:
            return default

    def getDbBool(self, db, key):
        """get a boolean configuration value for a database, or the default,
        or error if neither are specified"""
        val = self.getDbStr(db, key)
        if val == "yes":
            return True
        elif val == "no":
            return False
        else:
            raise Exception("invalid value for " + db + "." + key + ": \""
                            + val + "\", expected \"yes\" or \"no\"")
                            
    def getvDbBool(self, db, *keys):
        """Combine keys into '.' separate key. Get a boolean configuration
        value for a database, or the default, or error if neither are
        specified"""
        return self.getDbBool(db, ".".join(keys))

    def getDbBoolNone(self, db, key):
        """get a boolean configuration value for a database, or the default, or None
        if neither are specified"""
        
        val = self.get(db + "." + key)
        if val == None:
            val = self.get("default." + key)
        if val == None:
            return None
        elif val == "yes":
            return True
        elif val == "no":
            return False
        else:
            raise Exception("invalid value for " + db + "." + key + ": \""
                            + val + "\", expected \"yes\" or \"no\"")

    def getvDbBoolNone(self, db, *keys):
        """Combine keys into '.' separate key, Get a boolean configuration
        value for a database, or the default, or None if neither are
        specified"""
        return self.getDbBoolNone(db, ".".join(keys))
