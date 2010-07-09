#!/hive/groups/recon/local/bin/python
# Requires Python 2.6, current default python on hgwdev is 2.4

"""CGI script that outputs the ENCODE status based upon a specified
field.
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

# Parse report file and return result in the proper format
#   for the Google Visualization API
def processReportFile (reportFile, statusLabel, keyIndex, norelease, species):
  hash = {}

  f = open(reportFile, "r")
  for line in f:
    line = line.rstrip()
    if line.startswith('Project'):
      continue

    splitArray = line.split('\t')
    keyLabel = splitArray[keyIndex]
    status = splitArray[8]
    assembly = splitArray[9]

    if status == 'revoked' or status == 'replaced':
      continue
    if norelease == 1 and status == 'released':
      continue
    if keyIndex == 5: 
      keyLabel=encodeReportLib.parseFreezeLabel(keyLabel)
    if species == 'all':
      pass
    else:
      if species == 'human' and assembly.startswith('hg'):
        pass
      elif species == 'mouse' and assembly.startswith('mm'):
        pass
      elif species == assembly:
        pass
      else:
        continue

    if not keyLabel in hash:
      hash[keyLabel] = {}
      for i in statusLabel:
        hash[keyLabel][i] = 0
    hash[keyLabel][status] += 1
  f.close()

  if keyIndex == 5:
    sortKey = encodeReportLib.orderFreezeDateLabels(hash.keys())
  else:
    sortKey = sorted(hash)
  # Populate dataArray with the contents of the matrix
  dataArray = []
  for labKey in sortKey:
    array = []
    array.append(labKey)
    for statusKey in statusLabel:
      array.append(hash[labKey][statusKey])
    dataArray.append(array)

  return dataArray

def main():
  form = cgi.FieldStorage()
  # CGI Variables
  #   key = project, lab, data, freeze, or species
  #     Display data based on the key variable
  #   norelease = 0 or 1
  #     0 = Output all data
  #     1 = Output only unreleased data
  #   species = human, mouse, all
  #     human = Output only human data
  #     mouse = Output only mouse data
  #     all   = Output all data

  keyField = form.getvalue('key')
  if keyField == None:
    keyField = 'project'
  norelease = form.getvalue('norelease')
  if norelease == None:
    norelease = 0
  norelease = int(norelease)
  species = form.getvalue('species')
  if species == None:
    species = 'all'

  switch = {'project':0, 'lab':1, 'data':2, 'freeze':5, 'status':8}
  titleTag = {'project':"Project", 'lab':"Lab", 'data':"Data_Type", 
              'freeze':"Freeze", 'status':"Status"}
  if keyField not in switch:
    keyField = 'project'
  keyIndex = switch[keyField]

  # Headers for the columns in the data matrix
  description = [(titleTag[keyField], "string")]
  fullLabel = ['released', 'reviewing', 'approved', 'displayed', 'downloads', 'loaded']
  statusLabel = []
  for label in fullLabel:
    if label == 'released' and norelease == 1:
      continue
    tmpDesc = [(label, 'number')]
    description += tmpDesc
    statusLabel.append(label)

  reportFile, currentDate = encodeReportLib.getRecentReport()
  matrix = processReportFile(reportFile, statusLabel, keyIndex, norelease, 
                             species)

  # Create the data table
  data_table = gviz_api.DataTable(description)
  data_table.LoadData(matrix)

  # Convert to JavaScript code
  jscode = data_table.ToJSCode("jscode_data")

  # Set variables for HTML output
  template_vars = {}
  template_vars['jscode'] = jscode
  template_vars['dateStamp'] = encodeReportLib.dateIntToDateStr(currentDate)
  template_vars['title'] = "ENCODE (%s) Status by %s" % (species, 
                                                         titleTag[keyField])
  template_vars['packageName'] = 'columnchart'
  template_vars['visClass'] = 'ColumnChart'
  template_vars['style'] = ""
  template_vars['species'] = species
  template_vars['keyField'] = keyField
  template_vars['norelease']= norelease

  # Set the chart specific configuration options
  chart_config = {}
  chart_config['isStacked'] = 'true'
  chart_config['legendFontSize'] = 16
  chart_config['width'] = 854
  chart_config['height'] = 480
  chart_config['titleX'] = titleTag[keyField]
  chart_config['titleY'] = "# of Submissions"
  chart_config['tooltipFontSize'] = 16
  chart_config['enableTooltip'] = 'true'
  colors = encodeReportLib.getColorArray(len(statusLabel))
  colors.reverse()
  chart_config['colors'] = colors

  template_vars['chart_config'] = json.dumps(chart_config)

  encodeReportLib.renderHtml(template_vars, 0, 1)

  return

if __name__ == '__main__':
  main()
  sys.exit(0)
