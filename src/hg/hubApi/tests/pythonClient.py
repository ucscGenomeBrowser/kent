#!/bin/env python

"""
This code is heavily inspired by the Ensembl python client
"""
import argparse
import json
import sys

# support python2 and 3
try:
    from urllib.parse import urlparse, urlencode
    from urllib.request import urlopen, Request
    from urllib.error import HTTPError
except ImportError:
    from urlparse import urlparse
    from urllib import urlencode
    from urllib2 import urlopen, Request, HTTPError

serverName = "https://api-test.gi.ucsc.edu"
endpoints = ["list", "getData"]
parameterList = ["hubUrl", "genome", "db", "track", "chrom", "start", "end", "maxItemsOutput", "trackLeavesOnly"]

def parseCommandLine():
    parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter, description="Test accessing UCSC Genome Browser API",
        epilog="Example usage:\npythonClient.py \"/getData/track\" \"track=gold;db=hg38;chrom=chrM;maxItemsOutput=2\"")
    parser.add_argument("endpoint", help="endpoint string like \"/list/tracks\" or \"/getData/track/\"", type=str)
    parser.add_argument("parameters", help="semi-colon separated, required and/or optional endpoint argument(s) like \"db=hg38;chrom=chrM;maxItemsOutput=2\"",type=str)
    parser.add_argument("-p", "--pretty-print", action="store_true", default=False)
    if len(sys.argv) <= 2:
        parser.print_help()
        sys.exit(1)
    else:
        return parser.parse_args()

def main():
    args = parseCommandLine()

    # check endpoints
    if not args.endpoint.startswith("/list") and not args.endpoint.startswith("/getData"):
        raise Exception("The only supported endpoints are '/list' and '/getData'\n")

    #  check params formatted specifically
    params = args.parameters.split(";")
    for param in params:
        arg = param.split("=")[0]
        if arg == param or arg not in parameterList:
            raise Exception("Unsupported parameter %s. Please format parameters as arg=val.\n" % arg)

    # make http request and print the output
    data = None
    req = serverName + args.endpoint + "?" + args.parameters
    print("trying request: %s\n" % req)
    try:
        request = Request(req)
        response = urlopen(request)
        content = response.read()
        if content:
            data = json.loads(content)
    except HTTPError as e:
        raise Exception("Request %s failed with HTTP status %s" % (req, e.code))
    if args.pretty_print:
        print(json.dumps(data, indent=4, separators=(",",":")))
    else:
        print(json.dumps(data))

if __name__ == "__main__":
    main()
