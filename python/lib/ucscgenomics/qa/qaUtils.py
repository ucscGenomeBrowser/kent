import subprocess
import pipes

def callHgsql(database, command):
    """ Run hgsql command using subprocess, return stdout data if no error."""
    cmd = ["hgsql", database, "-Ne", command]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    cmdout, cmderr = p.communicate()
    if p.returncode != 0:
        # keep command arguments nicely quoted
        cmdstr = " ".join([pipes.quote(arg) for arg in cmd])
        raise Exception("Error from: " + cmdstr + ": " + cmderr)
    return cmdout

def getTrackType(database, table):
    """ Use tdbQuery to get the track type associated with this table, if any.
    Returns None on split tables."""
    cmd = ["tdbQuery", "select type from " + database + " where track='" + table +
           "' or table='" + table + "'"]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    cmdout, cmderr = p.communicate()
    if p.returncode != 0:
        # keep command arguments nicely quoted
        cmdstr = " ".join([pipes.quote(arg) for arg in cmd])
        raise Exception("Error from: " + cmdstr + ": " + cmder)
    if cmdout:
        tableType = cmdout.split()[1]
    else:
        tableType = None
    return tableType

def tableExists(db, table):
    """True if the table exists in the given database."""
    sqlResult = callHgsql(db, "show tables like '" + table + "'")
    return sqlResult.strip() == table

def checkTablesExist(db, tables):
    """Raises an exception if any table in the tables list does not exist in the database."""
    for name in tables:
        if not tableExists(db, name):
            raise Exception("Table " + name + " in " + db + " does not exist")

def runCommand(command, stdout=None, stderr=None):
    """Runs command in subprocess and raises exception if return code is not 0."""
    p = subprocess.Popen(command, stdout=stdout, stderr=stderr)
    p.wait()
    if p.returncode != 0:
        cmdstr = " ".join([pipes.quote(arg) for arg in command])
        raise Exception("Error from: " + cmdstr)
