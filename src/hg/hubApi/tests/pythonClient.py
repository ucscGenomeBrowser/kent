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
endpoints = ["/list/publicHubs", "/list/ucscGenomes", "/list/hubGenomes",
    "/list/tracks", "/list/chromosomes", "/getData/sequence", "/getData/track"]
parameterList = ["hubUrl", "genome", "db", "track", "chrom", "start", "end",
    "maxItemsOutput", "trackLeavesOnly"]

def parseCommandLine():
    parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
        description="Test accessing UCSC Genome Browser API",
        epilog="Example usage:\npythonClient.py \"/getData/track\" "
        "\"track=gold;db=hg38;chrom=chrM;maxItemsOutput=2\"")
    parser.add_argument("endpoint", nargs='?',
        help="Endpoint string like \"/list/tracks\" or \"/getData/track/\"", type=str)
    parser.add_argument("parameters", nargs='?',
        help="Parameters to endpoints. semi-colon separated key=value formatted string, "
        "like \"db=hg38;chrom=chrM;maxItemsOutput=2\"",type=str)
    parser.add_argument("-p", "--pretty-print", action="store_true", default=False,
        help="Print json response with newlines")
    parser.add_argument("--show-endpoints", action="store_true", default=False,
        help="List the supported endpoints")
    parser.add_argument("--show-arguments", action="store_true", default=False,
        help="List the supported arguments")
    parser.add_argument("--debug", action="store_true", default=False,
        help="Print final URL of the request.")
    return parser

def main():
    cmd = parseCommandLine()
    args = cmd.parse_args()
    req =  serverName

    if len(sys.argv) == 1:
        cmd.print_help()
        sys.exit(0)

    # check for the show arguments first and exit early
    if args.show_endpoints:
        print("Currently supported endpoints:\n\t%s" % ("\n\t".join(endpoints)))
        sys.exit(0)
    if args.show_arguments:
        print("Currently supported parameters to endpoint functions:\n\t%s" %
            ("\n\t".join(parameterList)))
        sys.exit(0)

    # check endpoints
    if args.endpoint:
        if args.endpoint not in endpoints:
            sys.stderr.write("Error: Endpoint '%s' not supported.\n"
                "Currently supported endpoints are:\n\t%s\n\n" %
                (args.endpoint, "\n\t".join(endpoints)))
            cmd.print_help()
            sys.exit(1)
        req += args.endpoint

    # check params formatted correctly
    if args.parameters:
        params = args.parameters.split(";")
        for param in params:
            arg = param.split("=")[0]
            if arg == param or arg not in parameterList:
                sys.stderr.write("Unsupported parameter '%s'. "
                    "Currently supported parameters to endpoints:\n\t%s\n"
                    "Please format parameters as arg=val.\n\n" %
                    (arg, "\n\t".join(parameterList)))
                cmd.print_help()
                sys.exit(1)
        req += "?" + args.parameters

    # make http request and print the output
    data = None
    if args.debug:
        print("request URL: %s\n" % req)
    try:
        request = Request(req)
        response = urlopen(request)
        content = response.read()
        if content:
            data = json.loads(content)
    except HTTPError as e:
        if e.code == 400: # does this work for other status codes?
            data = json.loads(e.read())
            sys.stderr.write("Error: %s\n" % data["error"])
            sys.exit(1)
        else:
            sys.stderr.write("Request %s failed with HTTP status %s\nreason:%s\n" %
                (req, e.code, e.reason))
        sys.exit(1)

    if data:
        if args.pretty_print:
            print(json.dumps(data, indent=4, separators=(",",":")))
        else:
            print(json.dumps(data))

if __name__ == "__main__":
    main()
