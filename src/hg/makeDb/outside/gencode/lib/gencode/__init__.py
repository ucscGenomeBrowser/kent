"""
Expects git@github.com:diekhans/pycbio.git
to be checkout in /hive/groups/browser/pycbio
and /hive/groups/browser/pycbio/lib on
sys.path
"""
import os
from .gencodeGtfParser import GtfParser
from .gencodeGff3Parser import Gff3Parser
from pycbio.sys import fileOps

def _getExt(gxfFile):
    if fileOps.isCompressed(gxfFile):
        return os.path.splitext(os.path.splitext(gxfFile)[0])[1]
    else:
        return os.path.splitext(gxfFile)[1]

def gencodeGxfParserFactory(gxfFile, ignoreUnknownAttrs=False):
    "get appropriate parser for file base on having .gtf or .gff3 extension"
    ext = _getExt(gxfFile)
    if ext == ".gtf":
        return GtfParser(gxfFile, ignoreUnknownAttrs=ignoreUnknownAttrs)
    elif ext == ".gff3":
        return Gff3Parser(gxfFile, ignoreUnknownAttrs=ignoreUnknownAttrs)
    else:
        raise Exception("expect file ending in .gtf or .gff3, possibly with compression extension: " + gxfFile)
