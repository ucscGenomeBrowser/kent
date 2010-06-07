#!/hive/groups/recon/local/bin/python
# Requires Python 2.6, current default python on hgwdev is 2.4

"""CGI script that outputs rows from the report file after
filtering based on specified keyfields and values.
"""

import cgi, cgitb
import sys

# Import local modules found in "/hive/groups/encode/dcc/charts"
sys.path.append("/hive/groups/encode/dcc/charts")
import encodeReportLib

__author__  = "Bernard Suh"
__email__   = "bsuh@soe.ucsc.edu"
__version__ = "1.0.0"

cgitb.enable()

def main():
  form = cgi.FieldStorage()
  # CGI variables:
  #   key       = project, lab, data, freeze, or status
  #   value     = values based on "key"
  #   status    = loaded, displayed, approved, reviewing or released
  #   species   = human, mouse or all
  #   norelease = 0 or 1

  keyField = form.getvalue('key')
  if not keyField:
    keyField = 'project'

  keyValue = form.getvalue('value')
  if not keyValue:
    keyValue = 'HudsonAlpha'
  if keyValue == "post Jan-2010":
    keyValue = "post ENCODE Jan 2010 Freeze"

  status = form.getvalue('status')
  if not status:
    status = 'released'

  species = form.getvalue('species')
  if not species:
    species = 'all'

  norelease = form.getvalue('norelease')
  if not norelease:
    norelease = 0
  norelease = int(norelease)

  switch = {'project':0, 'lab':1, 'data':2, 'freeze':5, 'status':8}
  titleTag = {'project':"Project", 'lab':"Lab", 'data':"Data_Type", 
              'freeze':"Freeze", 'status':"Status"}

  keyIndex = switch[keyField]

  reportFile, date = encodeReportLib.getRecentReport()

  try:
    f = open(reportFile, "r")
  except:
    print >> sys.stderr, "Error: Can't open file '%s'" % f
    sys.exit(-1)

  print "Content-type: text/plain"
  print 

  rowCount = 0
  for line in f:
    line = line.rstrip()
    splitArray = line.split('\t')
    assembly = splitArray[9]

    if splitArray[8] == 'revoked' or status == 'replaced':
      continue
    if norelease == 1 and status == 'released':
      continue
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

    if splitArray[keyIndex] == keyValue and splitArray[8] == status:
      print line
      rowCount += 1
  f.close()

  print
  print "%d rows" % rowCount
  print "key = %s, value = %s, status = %s, species = %s, norelease = %d" % (keyField, keyValue, status, species, norelease)

if __name__ == '__main__':
  main()
  sys.exit(0)
