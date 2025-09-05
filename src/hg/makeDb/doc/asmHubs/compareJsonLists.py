#!/usr/bin/python3

import sys
import json
import urllib.request

def readJsonFromFileOrUrl(path):
    if path.startswith("http://") or path.startswith("https://"):
        with urllib.request.urlopen(path) as response:
            return json.load(response)
    else:
        with open(path, 'r') as f:
            return json.load(f)

def readAsmIdsFromJsonSource(source):
    j = readJsonFromFileOrUrl(source)
    return {item['asmId']: item.get('comName', '') for item in j.get('data', [])}

def main():
    if len(sys.argv) != 3:
        sys.stderr.write(f"Usage: {sys.argv[0]} fileOrUrl1 fileOrUrl2\n")
        sys.exit(1)

    path1, path2 = sys.argv[1], sys.argv[2]


    asmMap1 = readAsmIdsFromJsonSource(path1)
    asmMap2 = readAsmIdsFromJsonSource(path2)

    onlyIn1 = sorted(set(asmMap1.keys()) - set(asmMap2.keys()))
    onlyIn2 = sorted(set(asmMap2.keys()) - set(asmMap1.keys()))

    print("Only in file 1:", file=sys.stderr)
    for asmId in onlyIn1:
        print(f"{asmId}\t{asmMap1[asmId]}", file=sys.stderr)
        print(f"{asmId}\t{asmMap1[asmId]}")

    print("\nOnly in file 2:", file=sys.stderr)
    for asmId in onlyIn2:
        print(f"{asmId}\t{asmMap2[asmId]}", file=sys.stderr)
#        print(f"{asmId}\t{asmMap2[asmId]}")

    if not onlyIn1 and not onlyIn2:
        print("No differences found. All asmId entries are the same.")

if __name__ == "__main__":
    main()
