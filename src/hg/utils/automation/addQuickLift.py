#!/usr/bin/python3
########################################################################
### addQuickLift.py fromDb toDb path
###
### adds entry to quickLiftChain hgcentral table if the entry does not
###    yet exist
########################################################################

import subprocess
import sys

def runHgsql(sql):
    cmd = ["hgsql", "hgcentraltest", "-N", "-e", sql]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        sys.exit(result.returncode)
    return result.stdout.strip()

def main():
    if len(sys.argv) != 4:
        print(f"Usage: addQuickLift.py fromDb toDb path", file=sys.stderr)
        print(f"\nAdds entry to quickLiftChain hgcentral table if the entry does\nnot yet exist.", file=sys.stderr)
        sys.exit(255)

    fromDb, toDb, path = sys.argv[1], sys.argv[2], sys.argv[3]

    # Check if entry already exists
    sql = f"SELECT path FROM quickLiftChain WHERE fromDb='{fromDb}' AND toDb='{toDb}'"
    rows = runHgsql(sql).splitlines()
    if len(rows) > 1:
        print(f"ERROR: duplicate entries for {fromDb}.{toDb} found.")
        sys.exit(255)

    existingPath = rows[0] if rows else ""
    if existingPath:
        if existingPath == path:
            print(f"Entry {fromDb}.{toDb} already exists with identical path.")
            sys.exit(0)
        else:
            print(f"Error: entry for {fromDb}.{toDb} exists with different path: {existingPath} != {path}", file=sys.stderr)
            sys.exit(255)
    else:
        # Insert new entry
        sql = f"INSERT INTO quickLiftChain (fromDb, toDb, path) VALUES ('{fromDb}', '{toDb}', '{path}')"
        sqlResult = runHgsql(sql)

if __name__ == "__main__":
    main()
