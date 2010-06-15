#!/hive/groups/recon/local/bin/python
# Requires Python 2.6, current default python on hgwdev is 2.4

"""CGI script that outputs the ENCODE report file filtered
for the June 2010 Freeze as a Google JSON Query Response
"""

import cgi, cgitb
import datetime
import json
import sys

# Import local modules found in "/hive/groups/encode/dcc/charts"
sys.path.append("/hive/groups/encode/dcc/charts")
import encodeReportLib
import gviz_api

__author__  = "Bernard Suh"
__email__   = "bsuh@soe.ucsc.edu"
__version__ = "1.0.0"

cgitb.enable()

def main():
  reportFile, currentDate = encodeReportLib.getRecentReport()

  # Output the file as Google JSON Query Response
  description = []
  dataMatrix = []
  line_idx = 0
  hash = {}

  f = open(reportFile, "r")
  for line in f:
    line = line.rstrip()
    splitArray = line.split('\t')

    project = splitArray[0]
    freeze = splitArray[5]
    assembly = splitArray[9]

    if line.startswith('Project') and line_idx == 0:
      pass
    else:
      # Parse the data

      if assembly != 'hg19' or freeze != 'Jun-2010':
        continue

      if not project in hash:
        hash[project] = 0
      hash[project] += 1

      data = line.split('\t')
    line_idx += 1
  f.close()

  for key in hash.keys():
    array = []
    array.append(key)
    array.append(hash[key])
    dataMatrix.append(array)

  projectHeader = ('Project', 'string')
  countHeader = ('Count', 'number')
  description.append(projectHeader)
  description.append(countHeader)

  data_table = gviz_api.DataTable(description)
  data_table.LoadData(dataMatrix)

  print "Content-type: text/plain"
  print
  print data_table.ToJSonResponse(req_id=1)

  return

if __name__ == '__main__':
  main()
  sys.exit(0)
