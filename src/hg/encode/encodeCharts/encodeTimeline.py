#!/hive/groups/recon/local/bin/python

import cgitb
import datetime
import os
import re
import sys

import gviz_api

cgitb.enable()

# Directory containing the report files
reportDir = "/hive/groups/encode/dcc/reports"

# File listing the important events and dates
importantDatesFile = "/hive/groups/encode/dcc/charts/important.dates.tab"

# Given the directory of reports, find the latest report
# Return the filename of the latest report
def getRecentReport (reportDir):
  # Regex for the report file
  pattern = re.compile("newreport\.(\d{4})\-(\d{2})\-(\d{2})\.dcc\.txt")

  # Scan the report directory and find the most recent report
  currentDate = 19010101
  currentFile = "NULL"

  try:
    dirList = os.listdir(reportDir)
  except:
    print >> sys.stderr, "Error: Can't open dir '%s'" % reportDir
    sys.exit(-1)

  for f in dirList:
    m = pattern.match(f)
    if m:
      # Convert date into an int
      date = int(m.group(1)) * 10000 + int(m.group(2)) * 100 + int(m.group(3))
      if date > currentDate:
        # Update the current latest date
        currentDate = date
        currentFile = f

  if currentFile == "NULL":
    print >> sys.stderr, "Error: Can't find a report file in dir '%s'" % reportDir
    sys.exit(-1)

  return currentFile, currentDate

# Read and parse the important dates file
# Return a dict where key = event date and value = event label
def readImportantDatesFile (file):

  importantDateHash = {}

  try:
    f = open(file, "r")
  except:
    print >> sys.stderr, "Error: Can't open file '%s'" % file
    sys.exit(-1)
  for line in f:
    line = line.rstrip()
    if line.startswith('#'):
      continue

    (date, text) = line.split('\t')
    importantDateHash[int(date)] = text

  return importantDateHash

# Convert dates into the int format YYYYMMDD
def convertDate (d):

  # Convert MM/DD/YY
  pattern = re.compile("(\d{2})\/(\d{2})\/(\d{2})")
  m = pattern.match(d)
  if m:
    dateNum = 20000000 + int(m.group(3)) * 10000 + int(m.group(1)) * 100 + int(m.group(2))
    return dateNum

  # Convert YYYY-MM-DD
  pattern = re.compile("(\d{4})\-(\d{2})\-(\d{2})")
  m = pattern.match(d)
  if m:
    dateNum = int(m.group(1)) * 10000 + int(m.group(2)) * 100 + int(m.group(3))
    return dateNum

  return d

# Parse report file and return result in the proper format
#   for the Google Visualization API
def getDataArray (reportDir, importantDatesFile):

  importantDateHash = readImportantDatesFile(importantDatesFile)

  submitHash = {}
  releaseHash = {}
  currentFile, currentDate = getRecentReport(reportDir)
  fullFilePath = reportDir + "/" + currentFile

  try:
    f = open(fullFilePath, "r")
  except:
    print >> sys.stderr, "Error: Can't open file '%s'" % f
    sys.exit(-1)

  print >> sys.stderr, "Parsing file: %s" % (fullFilePath)
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
    submitDate = convertDate(startDate)
    releaseDate = convertDate(endDate)

    # Accumulate dates in hash
    if isinstance(submitDate, int):
      if not submitDate in submitHash:
        submitHash[submitDate] = 0
      submitHash[submitDate] += 1

    if isinstance(releaseDate, int):
      if not releaseDate in releaseHash:
        releaseHash[releaseDate] = 0
      releaseHash[releaseDate] += 1

  # Get the union of all possible dates
  unionDates = set.union(set(submitHash.keys()), set(releaseHash.keys()), set(importantDateHash.keys()))

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

#    print "%d\t%d\t%d\t%d\t%d\t%s" % (date, releaseValue, releaseSum, submitValue, submitSum, annotText)
    # Single row of data
    array = []
    array.append(datetime.date(int(dateString[0:4]), int(dateString[4:6]), int(dateString[6:8])))
    array.append(releaseValue)
    array.append(releaseSum)
    array.append(submitValue)
    array.append(submitSum)
    array.append(annotText)
    dataArray.append(array)

  return dataArray, currentDate

def main():
  # Headers for the columns in the data matrix
  description = [("date", "date"), ("release", "number"), ("release_cumul", "number"), ("submit", "number"), ("submit_cumul", "number"), ("events", "string") ]

  # Create the data table 
  data_table = gviz_api.DataTable(description)

  # Create and load the matrix
  matrix, reportDate = getDataArray(reportDir, importantDatesFile)
  data_table.LoadData(matrix)

  reportDate = str(reportDate)
  reportDateObj = datetime.date(int(reportDate[0:4]), int(reportDate[4:6]), int(reportDate[6:8]))
  dateStamp = reportDateObj.strftime("%b %d, %Y")

  # Convert to JavaScript code
  jscode = data_table.ToJSCode("jscode_data")

  # Commented out but could serve this page dynamically
  print "Content-type: text/html"
  print

  # Print out the webpage
  print page_template % vars()

  return

# The html template. Will be filled in by string subs
page_template = """
<html>
  <head>
    <script type='text/javascript' src='http://www.google.com/jsapi'></script>
    <script type='text/javascript'>
      google.load('visualization', '1', {'packages':['annotatedtimeline']});

      google.setOnLoadCallback(drawChart);
      function drawChart() {
        %(jscode)s

        var chart = new google.visualization.AnnotatedTimeLine(document.getElementById('chart_div'));
        chart.draw(jscode_data, {displayAnnotations: true, displayAnnotationsFilter: true, fill:25, thickness:3, annotationsWidth: 15});
      }
    </script>
    <title>ENCODE Cumulative Submit and Release Timeline</title>
  </head>

  <body>
    <h2>ENCODE Cumulative Submit and Release Timeline <br><font size="-1">(Report Date: %(dateStamp)s)</font></h2>
    <div id='chart_div' style='width: 854px; height: 480px;'></div>
  </body>
</html>
"""

if __name__ == '__main__':
  main()
  sys.exit(0)
