#
# genbank configuration file parser object
#
# $Id: Config.py,v 1.1.2.2 2005/08/21 04:37:54 markd Exp $
#

import re, string

class Config(dict):
    "object to parse and contain the genbank.conf"

    # REs for parsing, which trim leading and training blanks
    ignoreRe = re.compile("^\s*(#.*)?$") # blank or comment
    keyValRe = re.compile("^\s*([^\s=]+)\s*=\s*(.*)\s*$")

    def _parseLine(self, fh, line):
        "parse a line from the conf file and key/value to object"
        if not Config.ignoreRe.match(line):
            m = Config.keyValRe.match(line)
            if m == None:
                raise Exception(fh.name + ":" + "can't parse conf line: " + line)
            self[m.group(1)] = m.group(2)

    def __init__(self, confFile):
        "read conf file into object"
        self.confFile = confFile
        fh = open(confFile)
        for line in fh:
            self._parseLine(fh, line)
        fh.close()

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
