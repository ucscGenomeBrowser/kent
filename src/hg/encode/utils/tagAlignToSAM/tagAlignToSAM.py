#!/usr/bin/env python

# Converts from tagAlign format to SAM format
#   tagAlign file format ref: 
#     http://encodewiki.ucsc.edu/EncodeDCC/index.php/File_Formats
#   SAM file format ref: 
#     http://samtools.sourceforge.net/SAM1.pdf

import bz2
import gzip
import string
import sys

# Output variables in SAM tab-delimited format
def print_sam (fh, qname, flag, rname, pos, mapq, cigar, mrnm, mpos, isize, 
               seq, qual, tag):
  if len(tag) != 0:
    # We have tags to output
    tagstring = '\t'.join(tag)
    output = "%s\t%d\t%s\t%d\t%d\t%s\t%s\t%d\t%d\t%s\t%s\t%s" % (qname,
     flag, rname, pos, mapq, cigar, mrnm, mpos, isize, seq, qual, tagstring)
  else:
    output = "%s\t%d\t%s\t%d\t%d\t%s\t%s\t%d\t%d\t%s\t%s" % (qname,
     flag, rname, pos, mapq, cigar, mrnm, mpos, isize, seq, qual)
  fh.write(output + "\n")

# Reverse complement sequence
def rc (sequence):
  return sequence.translate(string.maketrans("ACGTacgt", "TGCAtgca"))[::-1]

if (len(sys.argv) < 2):
  print >> sys.stderr, "Usage:"
  print >> sys.stderr, "  tagAlignToSAM.py tagAlign_file > SAM_filename"
  print >> sys.stderr, "    prints out SAM file to stdout"
  print >> sys.stderr
  print >> sys.stderr, "  tagAlignToSAM.py tagAlign_file SAM_filename"
  print >> sys.stderr, "    prints out gzip-ed SAM file to SAM_filename.gz"
  print >> sys.stderr
  print >> sys.stderr, "  Note: tagAlign_file can be:"
  print >> sys.stderr, "         bzip2-ed, gzip-ed or plain/uncompressed"
  sys.exit(-1)
else:
  # Open up eland extended file handle
  tagAlignFile = sys.argv[1]
  try:
    if tagAlignFile.endswith('.bz2'):
      tagAlign_f = bz2.BZ2File(tagAlignFile, 'rb')
    elif tagAlignFile.endswith('.gz'):
      tagAlign_f = gzip.GzipFile(tagAlignFile, 'rb')
    else:
      tagAlign_f = open(tagAlignFile, 'r')
  except:
    print >> sys.stderr, "Error: Can't open file '%s'" % tagAlignFile
    sys.exit(-1)

  if (len(sys.argv) == 3):
    outputFile = sys.argv[2]
    if not outputFile.endswith('.gz'):
      outputFile += ".gz"
    try:
      output_f = gzip.GzipFile(outputFile, 'wb', 6)
    except:
      print >> sys.stderr, "Error: Can't open file '%s'" % outputFile
      sys.exit(-1)
  else:
    output_f = sys.stdout

line_idx = 0

for line in tagAlign_f:
  line = line.rstrip()
  line_array = line.split()
  if len(line_array) != 6:
    print >> sys.stderr, "Error: Invalid line '%s'" % line
    sys.exit(-1)

  chrom = line_array[0]
  chromStart = int(line_array[1])
  chromEnd = int(line_array[2])
  seq = line_array[3]
  score = int(line_array[4])
  strand = line_array[5]

  if (len(seq) != chromEnd-chromStart):
    print >> sys.stderr, "Error: Invalid line '%s' [length difference]" % line
    sys.exit(-1)

  seq_len = len(seq)

  # Construct a identifier
  seq_id = "%s:%d:%s:%d" % (chrom, chromStart+1, strand, line_idx)

  # Defaults (assume no matches and no paired ends)
  QNAME = seq_id
  FLAG = 0
  RNAME = '*'
  POS = 0
  MAPQ = 0
  CIGAR = '*'
  MRNM = '*'
  MPOS = 0
  ISIZE = 0
  SEQ = seq
  QUAL = '*'
  TAG = []

  # Set the SAM variables based on tagAlign
  RNAME = chrom
  POS = chromStart + 1 # Adjust for zero-base
  CIGAR = "%dM" % seq_len
  if strand == '+':
    pass
  elif strand == '-':
    # Handle reverse complement case
    #   in SAM format everything is forward strand
    SEQ = rc(SEQ)
    FLAG |= 0x10 # strand of the query
  else:
    print >> sys.stderr, "Error: Invalid strand in '%s'" % line

  # Note: Add code here to change the reported MAPQ value
  # From the SAM file format spec:
  #   MAPQ = 0 indicates map pos query is not available
  #   MAPQ = 255 indicates map quality not available
  #   MAPQ = [0, 255]
  # However the tagAlign score was largely undefined
  #   Typically integers in the range of 0-1000
  #   But many labs/groups have utilized this field in different ways
  MAPQ = 255

  print_sam(output_f, QNAME, FLAG, RNAME, POS, MAPQ, CIGAR, MRNM, MPOS, ISIZE,
            SEQ, QUAL, TAG)

  line_idx += 1

sys.exit(0)
