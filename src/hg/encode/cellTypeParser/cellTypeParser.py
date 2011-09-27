#!/usr/bin/env python

#
# cellType.py: parse any to-be-registered cell type from the wiki,
# and download any newly-approved cell type protocol documents.
#


import argparse
import base64
from BeautifulSoup import BeautifulSoup
import HTMLParser
import re
import string
import sys
import urllib2
from ucscgenomics.rafile.RaFile import *



def stripLeadingTrailingWhitespace(text):
    """Given a string, remove any leading or trailing whitespace"""
    text = re.sub("^([" + string.whitespace + "])+", "", text)
    text = re.sub("([" + string.whitespace + "])+$", "", text)
    return(text)

def getContents(field):
    """Given an HTML field, return the contents"""
    contents = stripLeadingTrailingWhitespace(field.contents[0])
    if len(contents) == 0:
        contents = "missing"
    return(contents)



def processOrderUrl(orderInfo):
    """Parse the orderInfo column. Return the vendorName, vendorId, and orderUrl"""
    if not orderInfo.has_key("href"):
        vendorName =  getContents(orderInfo)
        vendorId = "missing"
        orderUrl = "missing"
    else:
        orderUrl = orderInfo["href"]
        vendorData = orderInfo.text.split()
        vendorId = vendorData.pop()
        vendorName = ' '.join(vendorData)
    return((vendorName, vendorId, orderUrl))    
    

def processTermId(termInfo):
    """Parse the term ID column.  Return the termId and termUrl. """
    if not termInfo.has_key("href"):
        termId = termInfo.text
        termUrl = "missing"
    else:
        termId = termInfo.text
        termUrl = termInfo["href"]
    return((termId, termUrl))

    
#
# Process a cell type table entry, in which the order of the columns is
# (Cell Type, Description, Lineage, Karyotype, Sex, Tissue, Order URL, Term ID, 
#  Submitting Lab)
#   
def processCellTypeEntry(row, species, downloadsDirectory, noDownload,
                         username, password, wikiBaseUrl):
    cellData = row.findAll("td")
    term = getContents(cellData[0])
    if re.search("(Example)", term):
        return((None, False))
    else:
        #
        # Scrape the cells of the wiki row into a new RaStanza object
        stanza = RaStanza()
        stanza["term"] = term
        stanza["tag"] = re.sub("[-_\(\)]", "", term).upper()
        stanza["type"] = "Cell Line"
        stanza["tier"] = "3"
        stanza["organism"] = species 
        stanza["description"] = getContents(cellData[1])
        stanza["lineage"] = getContents(cellData[2])
        stanza["karyotype"] = getContents(cellData[3])
        stanza["sex"] = getContents(cellData[4])
        stanza["tissue"] = getContents(cellData[5])
        if len(cellData[6]) > 1:
            (stanza["vendorName"], stanza["vendorId"], 
             stanza["orderUrl"]) = processOrderUrl(cellData[6].contents[1])
        if len(cellData[7]) > 1:
            (stanza["termId"], 
             stanza["termUrl"]) = processTermId(cellData[7].contents[1])
        stanza["lab"] = getContents(cellData[8])

        #
        # Assemble the target name of the cell protocol document.  The naming
        # convention is <term>_<lab>_protocol.pdf, with any special characters
        # stripped from the term.
        protocolDocument = "%s_%s_protocol.pdf" \
            % (re.sub("[-_\(\)]", "", term), stanza["lab"]) 
        stanza["tag"] = stanza["tag"].upper()
        protocolDocument = re.sub("(\s)+", "", protocolDocument)
        stanza["protocol"] = "%s:%s" % (stanza["lab"], protocolDocument)
        #
        # Indicate whether or not the document (if any) is approved by the NHGRI.
        # If it's approved, and if the noDownload flag is false,
        # then download it into the target filename.  
        approved = False
        if re.search("^[Y|y]", getContents(cellData[10])):
            approved = True
            if noDownload == False:
                documentContents = getContents(cellData[9])
                if len(cellData[9].findAll("a")) != 0:
                    urlClauses = cellData[9].findAll("a")
                    if len(urlClauses) > 0:
                        if urlClauses[0].has_key("href"):
                            url = urlClauses[0]["href"]
                            doc = accessWiki(wikiBaseUrl + url, username, password)
                            if len(doc) > 0:
                                outputFilename = "%s/%s" % (downloadsDirectory, 
                                                            protocolDocument)
                                newDocFile = open(outputFilename, "wb")
                                newDocFile.write(doc)
                                newDocFile.close()
        return((stanza, approved))
                               

def accessWiki(url, username, password):
    """Read the indicated URL from the wiki page"""
    passmgr = urllib2.HTTPPasswordMgrWithDefaultRealm()
    base64string = base64.encodestring('%s:%s' % (username, password))[:-1]
    authheader =  "Basic %s" % base64string
    req = urllib2.Request(url)
    req.add_header("Authorization", authheader)
    try:
        handle=urllib2.urlopen(req)
        return(handle.read())
    except IOError, e:
        print "Fail!  Bad username or password?"
        return(None)



#
# Main code
#
defaultUsername = "encode"
defaultPassword = "human"
parser = argparse.ArgumentParser(description="Parse new cell type registrations from the ENCODE wiki and download protocol documents") 
parser.add_argument("-s", dest="species", default="human",
                    action="store", help="Species (default: human)")
parser.add_argument("-d", dest="downloadDirectory", default=".",
                    action="store",
                    help="Directory to download any documents into (default: '.'")
parser.add_argument("-f", dest="forcePrinting", default=False,
                    action="store_true",
                    help="Force printing of all stanzas, whether or not there's NHGRI approval (default: false)")
parser.add_argument("-n", dest="noDownload", default=False, 
                    action="store_true", 
                    help="Download no files (default: false)")
parser.add_argument("-u", dest="username", default=defaultUsername,
                    action="store", 
                    help="Username to access the wiki page (default: encode)")
parser.add_argument("-p", dest="password", default=defaultPassword,
                    action="store", 
                    help="Password to access the wiki page (default: human)")
args = parser.parse_args()

#
# Set up access to the wiki page
#
wikiBaseUrl = "http://encodewiki.ucsc.edu/"
cellTypePage = wikiBaseUrl + "EncodeDCC/index.php/Cell_lines"
passmgr = urllib2.HTTPPasswordMgrWithDefaultRealm()
base64string = base64.encodestring('%s:%s' % (args.username, 
                                              args.password))[:-1]
authheader =  "Basic %s" % base64string
req = urllib2.Request(cellTypePage)
req.add_header("Authorization", authheader)

try:
    handle=urllib2.urlopen(req)
except IOError, e:
    print "Fail!  Bad username or password?"
thepage = handle.read()
soup = BeautifulSoup(thepage)


#
# Look for the Tier 3 cell types table, the third table on the page.
# Once you reach the table, process each <td> line until an end of
# table tag is reached.
#
cellTypeTable = soup.findAll("table")[2]
skippedHeaderRow = False
for entry in cellTypeTable.findAll("tr"):
    if not skippedHeaderRow:
        skippedHeaderRow = True
    else:
        (stanza, approved) = processCellTypeEntry(entry, args.species,
                                                  args.downloadDirectory,
                                                  args.noDownload,
                                                  args.username,
                                                  args.password, 
                                                  wikiBaseUrl)
        if approved or args.forcePrinting:
            print stanza
