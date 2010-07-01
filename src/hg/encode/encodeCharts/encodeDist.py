#!/hive/groups/recon/local/bin/python
# Requires Python 2.6, current default python on hgwdev is 2.4

"""CGI script that outputs the amount of time until release for
ENCODE submissions based on a specified keyfield.
"""

import cgi, cgitb
import datetime
import json
import operator
import re
import sys

# Import local modules found in "/hive/groups/encode/dcc/charts"
sys.path.append("/hive/groups/encode/dcc/charts")
import encodeReportLib
import gviz_api

__author__  = "Bernard Suh"
__email__   = "bsuh@soe.ucsc.edu"
__version__ = "1.0.0"

cgitb.enable()

# Parse report file and return result in the proper format
#   for the Google Visualization API
def processReportFile (reportFile, keyIndex):
  hash = {}
  labelHash = {}

  f = open(reportFile, "r")
  for line in f:
    line = line.rstrip()
    if line.startswith('Project'):
      continue

    splitArray = line.split('\t')
    keyLabel = splitArray[keyIndex]
    startDate = splitArray[6]
    endDate = splitArray[7]
    status = splitArray[8]

    if keyIndex == 8 and (status == 'revoked' or status == 'replaced'):
      continue

    if keyIndex == 5:
      keyLabel = encodeReportLib.parseFreezeLabel(keyLabel)

    # Convert dates into ints
    submitDate = encodeReportLib.convertDate(startDate)
    releaseDate = encodeReportLib.convertDate(endDate)

    if status == 'released':
      if not isinstance(submitDate, int) or not isinstance(releaseDate, int):
        print >> sys.stderr, "Error: Invalid date: %s" % line
      else:
        if not keyLabel in labelHash:
          labelHash[keyLabel] = 0
        labelHash[keyLabel] += 1

        deltaTime = encodeReportLib.dateIntToObj(releaseDate) - encodeReportLib.dateIntToObj(submitDate)
        days = deltaTime.days
        if (days == 0):
          days = 1
        weeks = days / 7
        if (days % 7 != 0):
          # Adjust by one except when number of days is exactly divisible by 7
          weeks += 1
        if not weeks in hash:
          hash[weeks] = {}
        if not keyLabel in hash[weeks]:
          hash[weeks][keyLabel] = 0
        hash[weeks][keyLabel] += 1
  f.close()

  if keyIndex == 5:
    labels = encodeReportLib.orderFreezeDateLabels(labelHash.keys())
  else:
    tmpLabels = sorted(labelHash.iteritems(), key=operator.itemgetter(1), 
                       reverse=True)
    labels = []
    for i in tmpLabels:
      labels.append(i[0])

  maxWeek = max(hash)
  for i in xrange(1, maxWeek+1):
    if not i in hash:
       hash[i] = {}
    for label in labels:
      if not label in hash[i]:
        hash[i][label] = 0

  # Populate dataArray with the contents of the matrix
  dataArray = []
  for key in sorted(hash):
    array = []
    array.append(key)
    for label in labels:
      array.append(hash[key][label])
    dataArray.append(array)

  return dataArray, labels

def main():
  form = cgi.FieldStorage()
  # CGI variables:
  #   key = project, lab, data, freeze, status

  keyField = form.getvalue('key')
  if not keyField:
    keyField = 'project'

  switch = {'project':0, 'lab':1, 'data':2, 'freeze':5, 'status':8 }
  titleTag = {'project':"Project", 'lab':"Lab", 'data':"Data_Type", 
              'freeze':"Freeze", 'status':"Status" }
  if keyField not in switch:
    keyField = 'project'
  keyIndex = switch[keyField]

  reportFile, currentDate = encodeReportLib.getRecentReport()
  matrix, labels = processReportFile(reportFile, keyIndex)

  # Headers for the columns in the data matrix
  description = [("Time", "string")]
  for label in labels:
    tmpDesc = [(label, 'number')]
    description += tmpDesc

  # Create the data table
  data_table = gviz_api.DataTable(description)
  data_table.LoadData(matrix)

  # Convert to JavaScript code
  jscode = data_table.ToJSCode("jscode_data")

  # Set variables for HTML output
  template_vars = {}
  template_vars['jscode'] = jscode
  template_vars['dateStamp'] = encodeReportLib.dateIntToDateStr(currentDate)
  template_vars['title'] = "ENCODE Amount of Time Until Release by %s" % titleTag[keyField]
  template_vars['packageName'] = 'columnchart'
  template_vars['visClass'] = 'ColumnChart'
  template_vars['style'] = ""

  # Set the chart specific configuration options
  chart_config = {}
  chart_config['isStacked'] = 'true'
  chart_config['legendFontSize'] = 16
  chart_config['width'] = 1200
  chart_config['height'] = 480
  chart_config['legend'] = 'bottom'
  chart_config['titleX'] = '# of Weeks'
  chart_config['titleY'] = '# of Submissions'
  chart_config['tooltipFontSize'] = 16

  if (keyField == 'freeze'):
    chart_config['colors'] = encodeReportLib.getColorArray(len(labels))

  template_vars['chart_config'] = json.dumps(chart_config)

  encodeReportLib.renderHtml(template_vars, 1, 0)

  return

if __name__ == '__main__':
  main()
  sys.exit(0)
