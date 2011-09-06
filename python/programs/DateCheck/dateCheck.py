#!/usr/bin/python

import sys
import datetime
sys.path.append('/cluster/home/wong/kent/python/lib/')

from ucscgenomics.rafile.RaFile import *

def indexhg18(rafile):
	objlist = {}
	rahg18 = RaFile(rafile)
	
	#structure is hg18expids[expId][view][replicate] = information dict
	#used for easy lookup of an equivalent hg18 object, it will only run through the list of expids first instead of all objects looking for a match
	hg18expids = {}
	
	for key in rahg18.keys():
		stanza = rahg18[key]
		if not ('replicate' in stanza):
			stanza['replicate'] = "NA"
			#print "hg18:%s" % (stanza['metaObject'])
		if 'expId' in stanza:
			info = {}
			info['replicate'] = stanza['replicate']
			
				
			info['dateSubmitted'] = stanza['dateSubmitted']
			info['dateUnrestricted'] = stanza['dateUnrestricted']
			info['metaObject'] = stanza['metaObject']
			if stanza['expId'] in hg18expids:
				expid = stanza['expId']
				rep = stanza['replicate']
				objview = stanza['view']
				if objview in hg18expids[expid]:
					
					hg18expids[expid][objview][rep] = info
				else:
					foo = {}
					foo[rep] = info
					hg18expids[expid][objview] = foo
			else:
				foo = {}
				bar = {}
				bar[stanza['replicate']] = info
				foo[stanza['view']] = bar
				hg18expids[stanza['expId']] = foo
	
# 	for key in hg18expids:
# 		print "%s:" % (key)
# 		views = hg18expids[key]
# 		for view in views:
# 			print "\t%s" % (view)
# 			objlist = views[view]
# 			for object in objlist:
# 				print "\t\t%s" % object
# 				for var in objlist[object]:
# 					print "\t\t\t%s = %s" % (var, objlist[object][var])
# 		print "\n"
	return(hg18expids)
def gethg19objects(rafile):
	ra = RaFile(rafile)
	objlist = {}
	for key in ra.keys():
		stanza = ra[key]
		objinfo = {}
		if not ('replicate' in stanza):
			stanza['replicate'] = "NA"
			#print "hg19:%s" % (stanza['metaObject'])
		if 'expId' in stanza:
			objinfo['expId'] = stanza['expId']
			objinfo['view'] = stanza['view']
			objinfo['replicate'] = stanza['replicate']
			if 'dateUnrestricted' in stanza:
				objinfo['dateUnrestricted'] = stanza['dateUnrestricted']
				objinfo['dateSubmitted'] = stanza['dateSubmitted']
			objlist[stanza['metaObject']] = objinfo
	return(objlist)

def main():
	
	
	hg18expids = indexhg18(sys.argv[2])
	hg19objects = gethg19objects(sys.argv[1])
	date = datetime.date
	today = str(date.today())
	print today	

	for key in hg19objects.keys():
		expid = hg19objects[key]['expId']
		rep = hg19objects[key]['replicate']
		view = hg19objects[key]['view']
		if expid in hg18expids:
			hg18dict = hg18expids[expid]
			if view in hg18dict:
				repdict = hg18dict[view]
				if rep in repdict:
					infodict = repdict[rep]
					setvars = ""
					vars = ""
					if infodict['dateSubmitted'] < hg19objects[key]['dateSubmitted']:
						print "\n"
						print "%s vs %s, dateSubmitted: %s vs %s" % (key, infodict['metaObject'], infodict['dateSubmitted'], hg19objects[key]['dateSubmitted'])
						setvars = "dateSubmitted=%s dateResubmitted=%s" % (infodict['dateSubmitted'], hg19objects[key]['dateSubmitted'])
						
					if infodict['dateUnrestricted'] < hg19objects[key]['dateSubmitted'] and hg19objects[key]['dateUnrestricted'] > today:
						print "%s vs %s, dateUnrestricted: %s vs %s" % (key, infodict['metaObject'], infodict['dateUnrestricted'], hg19objects[key]['dateUnrestricted'])	
						setvars = "%s dateUnrestricted=%s" % (setvars, infodict['dateUnrestricted'])

					if setvars:
						if rep == "NA":
							vars = "expId=%s view=%s" % (expid, view)
						else:
							vars = "expId=%s view=%s replicate=%s" % (expid, view, rep)
						print "mdbUpdate hg19 -vars=\"%s\" -setVars=\"%s\"" % (vars, setvars)						

if __name__ ==  "__main__":
	main()
