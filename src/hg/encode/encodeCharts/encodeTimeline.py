#!/hive/groups/recon/local/bin/python
# Requires Python 2.6, current default python on hgwdev is 2.4

"""CGI script that outputs the timeline of ENCODE submissions and
releases as a Google Visualization Annotated Timeline.
"""

import cgitb
import datetime
import json
import sys

# Import local modules found in "/hive/groups/encode/dcc/charts"
sys.path.append("/hive/groups/encode/dcc/charts")
import gviz_api
import encodeReportLib

__author__  = "Bernard Suh"
__email__   = "bsuh@soe.ucsc.edu"
__version__ = "1.0.0"

cgitb.enable()

# Parse report file and return result in the proper format
#   for the Google Visualization API
def processReportFile (reportFile, currentDate):

  importantDateHash = encodeReportLib.readImportantDatesFile(currentDate)

  submitHash = {}
  releaseHash = {}

  try:
    f = open(reportFile, "r")
  except:
    print >> sys.stderr, "Error: Can't open file '%s'" % f
    sys.exit(-1)

  print >> sys.stderr, "Parsing file: %s" % (reportFile)
  for line in f:
    line = line.rstrip()
    if (line.startswith('Project')):
      # Skip the header line
      continue

    # The submit and release date are in fields 6 and 7
    splitArray = line.split('\t')
    startDate = splitArray[6]
    endDate = splitArray[7]

    # Convert dates into ints
    submitDate = encodeReportLib.convertDate(startDate)
    releaseDate = encodeReportLib.convertDate(endDate)

    # Accumulate dates in hash
    if isinstance(submitDate, int):
      if not submitDate in submitHash:
        submitHash[submitDate] = 0
      submitHash[submitDate] += 1

    if isinstance(releaseDate, int):
      if not releaseDate in releaseHash:
        releaseHash[releaseDate] = 0
      releaseHash[releaseDate] += 1
  f.close()

  # Get the union of all possible dates
  unionDates = set.union(set(submitHash.keys()), 
                         set(releaseHash.keys()), 
                         set(importantDateHash.keys()))

  submitValue = 0
  submitSum = 0
  releaseValue = 0
  releaseSum = 0

  # Populate dataArray with the contents of the data matrix
  dataArray = []
  for date in sorted(unionDates):
    dateString = str(date)

    submitValue = 0
    if date in submitHash:
      submitValue = submitHash[date]

    releaseValue = 0
    if date in releaseHash:
      releaseValue = releaseHash[date]

    submitSum += submitValue
    releaseSum += releaseValue

    annotText = ""
    if date in importantDateHash:
      annotText = importantDateHash[date]

    # Single row of data
    array = []
    array.append(datetime.date(int(dateString[0:4]), int(dateString[4:6]), 
                               int(dateString[6:8])))
    array.append(releaseValue)
    array.append(releaseSum)
    array.append(submitValue)
    array.append(submitSum)
    array.append(annotText)
    dataArray.append(array)

  return dataArray

def main():
  # Headers for the columns in the data matrix
  description = [ ("date", "date"), ("release", "number"), 
                  ("release_cumul", "number"), ("submit", "number"), 
                  ("submit_cumul", "number"), ("events", "string") ]

  # Create the data table 
  data_table = gviz_api.DataTable(description)

  currentFile, currentDate = encodeReportLib.getRecentReport()

  # Create and load the matrix
  matrix = processReportFile(currentFile, currentDate)
  data_table.LoadData(matrix)

  # Convert to JavaScript code
  jscode = data_table.ToJSCode("jscode_data")

  # Set variables for HTML output
  template_vars = {}
  template_vars['jscode'] = jscode
  template_vars['dateStamp'] = encodeReportLib.dateIntToDateStr(currentDate)
  template_vars['title'] = "ENCODE Cumulative Release and Submit Timeline"
  template_vars['packageName'] = 'annotatedtimeline'
  template_vars['visClass'] = 'AnnotatedTimeLine'
  template_vars['style'] = 'style="width:854px; height:480px"'

  # Set the chart specific configuration options
  chart_config = {}
  chart_config['annotationsWidth'] = 15
  chart_config['displayAnnotations'] = 'true'
  chart_config['displayAnnotationsFilter'] = 'true'
  chart_config['fill'] = 25
  chart_config['thickness'] = 3
  chart_config['width'] = 854
  chart_config['height'] = 480
  template_vars['chart_config'] = json.dumps(chart_config)

  encodeReportLib.renderHtml(template_vars, 0, 0)

  return

if __name__ == '__main__':
  main()
  sys.exit(0)
