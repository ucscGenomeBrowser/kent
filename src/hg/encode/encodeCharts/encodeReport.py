#!/hive/groups/recon/local/bin/python
# Requires Python 2.6, current default python on hgwdev is 2.4

"""CGI script that outputs the ENCODE report file as either a text file
or a Google Visualization Table.
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
  form = cgi.FieldStorage()

  # CGI variables:
  #   format = raw or pretty or google
  #     raw = Output file as plain text
  #     pretty = Output file as Google Visualization Table
  #     google = Google Data Source Protocol
  #   reportDate = 0, 1
  #     returns a JSON object of the latest date of the report file

  format = form.getvalue('format')
  if format == None:
    format = 'pretty'
  if format != 'pretty' and format != 'raw' and format != 'google':
    format = 'pretty'

  reportDate = form.getvalue('reportDate')
  if not reportDate:
    reportDate = 0
  reportDate = int(reportDate)

  callback = form.getvalue('callback')

  reportFile, currentDate = encodeReportLib.getRecentReport()

  if reportDate:
    jsonDate = {}
    jsonDate['currentDate'] = encodeReportLib.dateIntToDateStr(currentDate)
    print "Content-Type: application/json"
    print
    if not callback:
      outString = json.dumps(jsonDate)
    else:
      outString = callback + "(" + json.dumps(jsonDate) + ")"
    print outString
    return

  try:
    f = open(reportFile, "r")
  except:
    print >> sys.stderr, "Error: Can't open file '%s'" % f
    sys.exit(-1)

  if format == 'raw':
    # Output the file as raw text
    print "Content-Type: text/plain"
    print

    for line in f:
      line = line.rstrip()
      print line
    f.close()

    return

  elif format == 'pretty' or format == 'google':
    # Output the file as Google Visualization Table
    description = []
    dataMatrix = []
    line_idx = 0

    for line in f:
      line = line.rstrip()
      if line.startswith('Project') and line_idx == 0:
        # Parse the header line
        headers = line.split('\t')
        for header in headers:
          tmpDesc = (header, 'string')
          if header.endswith('Date'):
            tmpDesc = (header, 'date')
          description.append(tmpDesc)
      else:
        # Parse the data
        data = line.split('\t')
        data[6] = encodeReportLib.convertToDate(data[6])
        data[7] = encodeReportLib.convertToDate(data[7])
        dataMatrix.append(data)
      line_idx += 1
    f.close()

    data_table = gviz_api.DataTable(description)
    data_table.LoadData(dataMatrix)

    if format == 'google':
      print "Content-type: text/plain"
      print
      print data_table.ToJSonResponse(req_id=1)

      return

    jscode = data_table.ToJSCode("jscode_data")

    # Set variables for HTML output
    template_vars = {}
    template_vars['jscode'] = jscode
    template_vars['dateStamp'] = encodeReportLib.dateIntToDateStr(currentDate)
    template_vars['title'] = "ENCODE Report"
    template_vars['packageName'] = 'table'
    template_vars['visClass'] = 'Table'
    template_vars['style'] = ''

    # Set the chart specific configuration options
    chart_config = {}
    chart_config['page'] = 'enable'
    chart_config['pageSize'] = 15
    chart_config['pagingSymbols'] = {'prev': 'prev', 'next': 'next'};
    chart_config['showRowNumber'] = 'true'

    template_vars['chart_config'] = json.dumps(chart_config)

    encodeReportLib.renderHtml(template_vars, 0, 0)

    return

if __name__ == '__main__':
  main()
  sys.exit(0)
