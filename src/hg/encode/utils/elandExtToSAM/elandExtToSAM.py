#!/usr/bin/env python

# Converts from eland extended format to SAM format
#   eland extended ref: http://www.sequensysbio.com/pdf/dataformats.pdf
#   Illumina general ref: https://icom.illumina.com/Download/Summary/iA-zmp3loky4kSxXirC0hw
#   SAM ref: http://samtools.sourceforge.net/SAM1.pdf
#
# Additional sample input from:
#   http://nextgenseq.blogspot.com/2008/10/file-formats.html

import bz2
import gzip
import re
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
  print >> sys.stderr, "  elandExtToSAM.py eland_extended_file > SAM_filename"
  print >> sys.stderr, "    prints out SAM file to stdout"
  print >> sys.stderr
  print >> sys.stderr, "  elandExtToSAM.py eland_extended_file SAM_filename"
  print >> sys.stderr, "    prints out gzip-ed SAM file to SAM_filename.gz"
  print >> sys.stderr
  print >> sys.stderr, "  Note: eland_extended_file can be:"
  print >> sys.stderr, "         bzip2-ed, gzip-ed or plain/uncompressed"
  sys.exit(-1)
else:
  # Open up eland extended file handle
  elandExtFile = sys.argv[1]
  try:
    if elandExtFile.endswith('.bz2'):
      eland_f = bz2.BZ2File(elandExtFile, 'rb')
    elif elandExtFile.endswith('.gz'):
      eland_f = gzip.GzipFile(elandExtFile, 'rb')
    else:
      eland_f = open(elandExtFile, 'r')
  except:
    print >> sys.stderr, "Error: Can't open file '%s'" % elandExtFile
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

# Regex for match location
#  can either include chrom info (chr_pattern)
#  or the case where inherit chrom info from left (nochr_pattern)
chr_pattern = re.compile("^(.+)\.fa\:(\d+)([FR])([ACGT\d]+)$")
nochr_pattern = re.compile("^(\d+)([FR])([ACGT\d]+)$")

# Loop through file
for line in eland_f:
  line = line.rstrip()
  line_array = line.split()
  if len(line_array) != 4:
    print >> sys.stderr, "Error: Invalid line '%s'" % line
    sys.exit(-1)

  seq_id = line_array[0]
  if seq_id.startswith('>'):
    # Strip off leading '>'
    seq_id = seq_id.lstrip('>')
  seq = line_array[1]
  match_type = line_array[2]
  match_loc = line_array[3]

  seq_len = len(seq)

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

  # No indels present in eland_extended alignment format
  match_loc_array = match_loc.split(',')
  for match_info in match_loc_array:
    # Reset Defaults
    FLAG = 0
    POS = 0
    MAPQ = 0
    CIGAR = '*'
    SEQ = seq
    TAG = []
    if match_info == '-':
      FLAG |= 0x4 # Query sequence is unmapped
      if match_type == 'NM' or match_type == 'QC' or match_type == 'RM':
        # No matches as a result of
        #   NM - no match
        #   QC - too many Ns
        #   RM - repeat masked region
        pass
      else:
        # No reported alignments due to excessive number of hits
        match_type_array = match_type.split(':')
        TAG.append("H0:i:%d" % int(match_type_array[0]))
        TAG.append("H1:i:%d" % int(match_type_array[1]))
        TAG.append("H2:i:%d" % int(match_type_array[2]))
    else:
      CIGAR = "%dM" % seq_len
      MAPQ = 255 # Mapping quality is not available

      # Look for pattern with reference id
      m = chr_pattern.match(match_info)
      if m:
        RNAME = m.group(1)
        POS = int(m.group(2))
        strand = m.group(3)
        match_descriptor = m.group(4)
      else:
        # Look for pattern without reference id, inherit the RNAME
        m2 = nochr_pattern.match(match_info)
        if m2:
          POS = int(m2.group(1))
          strand = m2.group(2)
          match_descriptor = m2.group(3)
        else:
          print >> sys.stderr, "Error: Can't parse '%s'" % match_info
      if strand == 'F':
        pass
      elif strand == 'R':
        # Handle reverse complement case
        #   in SAM format everything is forward strand
        SEQ = rc(SEQ)
        FLAG |= 0x10 # strand of the query
      else:
        print >> sys.stderr, "Error: Invalid strand in '%s'" % match_info

    print_sam(output_f, QNAME, FLAG, RNAME, POS, MAPQ, CIGAR, MRNM, MPOS, ISIZE,
              SEQ, QUAL, TAG)

sys.exit(0)
