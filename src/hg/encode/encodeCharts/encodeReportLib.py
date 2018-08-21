"""Module that handles shared functions used by the ENCODE Report Charts set
of CGI scripts.
"""

import datetime
import os
import re
import sys

import gviz_api

__author__  = "Bernard Suh"
__email__   = "bsuh@soe.ucsc.edu"
__version__ = "1.0.0"

# Given the directory of reports, find the latest report
# Return the filename of the latest report
def getRecentReport (reportDir = "/hive/groups/encode/dcc/reports"):
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

  # Try to use the file 'latest' rather than doing the linear file scan
  # Note: The 'latest' file is often one day off
  try:
    latestFile = os.path.join(reportDir, 'latest')
    realFile = os.readlink(latestFile)
    baseName = os.path.basename(realFile)
    m = pattern.match(baseName)
    if m:
      date = int(m.group(1)) * 10000 + int(m.group(2)) * 100 + int(m.group(3))
      currentFile = realFile
      currentDate = date
  except:
    currentFile = 'NULL'

  # Perform linear scan for most recent filename
  if currentFile == 'NULL':
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

  currentFile = os.path.join(reportDir, currentFile)

  return currentFile, currentDate

# Read and parse the important dates file
# Return a dict where key = event date and value = event label
def readImportantDatesFile (currentDate):
  file = "/hive/groups/encode/dcc/charts/important.dates.tab"

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
    date = int(date)
    if date > currentDate:
      continue
    importantDateHash[date] = text

  return importantDateHash

# Convert stringdates into the datetime objects (or None if can't parse)
def convertToDate (d):

  # Convert MM/DD/YY
  try:
    pyDateObj = datetime.datetime.strptime(d, "%m/%d/%y").date()
  except:
    pass
  else:
    return pyDateObj

  try:
    pyDateObj= datetime.datetime.strptime(d, "%Y-%m-%d").date()
  except:
    pass
  else:
    return pyDateObj

  return None

# Convert dates into Python datetime objects
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

# Converts a date int into a Python date object
def dateIntToObj (date):
  stringDate = str(date)
  dateObj = datetime.date(int(stringDate[0:4]), int(stringDate[4:6]), int(stringDate[6:8]))
  return dateObj

# Converts a date int into a pretty formatted string
def dateIntToDateStr (date):
  dateObj = dateIntToObj(date)
  return dateObj.strftime("%b %d, %Y")

# Standard HTML template for ENCODE Report charts
def renderHtml(template_vars, mouseover, clickable):

  template_vars['mouseJS'] = """
  """
  if mouseover:
    template_vars['mouseJS'] = """
          google.visualization.events.addListener(viz, 'onmouseover', vizMouseOver);
          google.visualization.events.addListener(viz, 'onmouseout', vizMouseOut);
 
          function vizMouseOver(e) {
            viz.setSelection([e]);
          }
          function vizMouseOut(e) {
            viz.setSelection([{'row':null, 'column':null}]);
          }
    """
  if clickable:
    template_vars['mouseJS'] = """
          google.visualization.events.addListener(viz, 'onmouseover', vizMouseOver);
          google.visualization.events.addListener(viz, 'onmouseout', vizMouseOut);
 
          function vizMouseOver(e) {
            viz.setSelection([e]);
          }
          function vizMouseOut(e) {
            viz.setSelection([{'row':null, 'column':null}]);
          }

          google.visualization.events.addListener(viz, 'select', clickBlock);

          function clickBlock() {
            var selection = viz.getSelection();
            var item = selection[0];
 
            var urlString = "http://genecats.soe.ucsc.edu/cgi-bin/ENCODE/encodeReportFilter.py?" 
              + "key=%s"
              + "&value=" + jscode_data.getValue(item.row, 0)
              + "&status=" + jscode_data.getColumnLabel(item.column)
              + "&species=%s"
              + "&norelease=%s"
            ;
 
            window.open(urlString);
          }
  """ % (template_vars['keyField'], template_vars['species'], template_vars['norelease'])

  # The html template. Will be filled in by string subs
  # Added <!DOCTYPE> tag to fix IE8 issues
  page_template = """
  <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
  "http://www.w3.org/TR/html4/strict.dtd">

  <html>
    <head>
      <script type='text/javascript' src='http://www.google.com/jsapi'></script>
      <script type='text/javascript'>
        google.load('visualization', '1', {'packages':['%(packageName)s']});

        google.setOnLoadCallback(drawVisualization);
        function drawVisualization() {
          %(jscode)s
          var viz = new google.visualization.%(visClass)s(document.getElementById('viz_div'));
          viz.draw(jscode_data, %(chart_config)s);
          %(mouseJS)s
        }
      </script>
      <title>%(title)s</title>
    </head>

    <body>
      <h2>%(title)s<br><font size="-1">(Report Date: %(dateStamp)s)</font></h2>
      <div id='viz_div' %(style)s></div>
    </body>
  </html>
  """

  # Commented out but could serve this page dynamically
  print "Content-type: text/html"
  print

  # Print out the webpage
  print page_template % template_vars

  return

# Sort the freeze dates
def orderFreezeDateLabels (dateList):
  dateHash = {}
  pattern = re.compile("(post)?\s*(.+)\-(\d{4})")

  for i in dateList:
    m = pattern.match(i)
    if m:
      day = 1
      if m.group(1):
        day += 1
      datestring = "%s %d, %s" % (m.group(2), day, m.group(3))
      d = int(datetime.datetime.strptime(datestring, "%b %d, %Y").strftime("%Y%m%d"))
      dateHash[d] = i

  sortList = []
  for i in sorted(dateHash):
    sortList.append(dateHash[i])

  return sortList

# Convert freeze to standard format if non-standard
#  standard is Month-Year (Jun-2010)
#  non-standard example is "post ENCODE June 2010"
def parseFreezeLabel (freeze):
  pattern = re.compile("post ENCODE (.+) (\d{4}) Freeze")
  m = pattern.match(freeze)
  if m:
    return "post %.3s-%s" % (m.group(1), m.group(2))
  else:
    return freeze

def getColorArray (size):
  # Can't specify "orange" since IE8 doesn't support CSS2.1
  colors = ['red', '#ffa500', 'yellow', 'green', 'blue', 'purple', 'fuchsia']
  colors_len = len(colors)
  i = 0
  new = []
  while (i<size):
    leftover = size - i
    if (leftover > colors_len):
      new.extend(colors)
      i += colors_len
    else:
      new.extend(colors[0:leftover])
      break
  return new

