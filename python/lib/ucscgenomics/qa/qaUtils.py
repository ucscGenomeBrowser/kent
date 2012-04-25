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
