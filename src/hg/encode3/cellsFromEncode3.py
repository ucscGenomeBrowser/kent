#!/usr/bin/env python2

# List all ENCODE cell types in ENCODE2 CV and retrieve ENCODE3 fields of interest
#
# Here are the URL's for ENCODE2 and ENCODE3:
# 1) List all ENCODE2 cell types: 
#       http://genome.ucsc.edu/cgi-bin/hgEncodeApi?cmd=cv&type=cellType
# 2) Retrieve info from ENCODE2 CV for a cell type term: 
#       http://genome.ucsc.edu/cgi-bin/hgEncodeApi?cmd=cv&type=cellType&term=K562
# 3) Retrieve info from ENCODE3 for an ENCODE2 cell type term:
#       http://www.encodeproject.org/search/?type=biosample&dbxrefs=UCSC-ENCODE-cv:K562&format=json&frame=object

import requests, json

# get list of all ENCODE2 cells
URL = "http://genome.ucsc.edu/cgi-bin/hgEncodeApi?cmd=cv&type=cellType"
response = requests.get(URL)
cells = response.json()

# print headers
e2Header = ['e2-term', 'e2-lineage', 'e2-tissue', 'e2-karyotype', 'e2-vendorId', 
                'e2-vendorName', 'e2-termId']
e3Header = ['e3-type', 'e3-developmental', 'e3-organ', 'e3-donor', 'e3-sex', 'e3-age', 
                'e3-health', 'e3-termId', 'e3-url', 'e3-description']
print "\t".join(e2Header),"\t",
print "\t".join(e3Header)

# get ENCODE2 and ENCODE3 info for each cell type
cells.sort(key=lambda cell: cell['term'])
for cell in cells:
    if cell['organism'] != 'human':
        continue

    # print ENCODE2 info
    encode2 = [cell['term'], cell['lineage'], cell['tissue'], cell['karyotype'],
                cell['vendorId'], cell['vendorName'], cell['termId']]
    #cell['termUrl'], cell['orderUrl'],
    print "\t".join(encode2), "\t",

    # get ENCODE3 info
    URL = "http://www.encodeproject.org/search/?type=biosample&dbxrefs=UCSC-ENCODE-cv:" \
                + cell['term'] \
                + "&format=json&frame=object"
    response = requests.get(URL)
    if response.status_code == requests.codes.ok:
        e3Resp = response.json()
        if '@graph' in e3Resp and len(e3Resp['@graph']) > 0:
            e3Cell = e3Resp['@graph'][0]
            encode3 = [e3Cell['biosample_type']]
            encode3.append("unknown" if len(e3Cell['developmental_slims']) == 0 else \
                               e3Cell['developmental_slims'][0])
            encode3.append("unknown" if len(e3Cell['organ_slims']) == 0 else \
                               e3Cell['organ_slims'][0])
            encode3.append("unknown" if 'donor' not in e3Cell else e3Cell['donor'])
            encode3.append("unknown" if 'sex' not in e3Cell else e3Cell['sex'])
            encode3.append("unknown" if 'age' not in e3Cell else e3Cell['age'])
            encode3.append("unknown" if 'health_status' not in e3Cell else e3Cell['health_status'])
            encode3 += [e3Cell['biosample_term_id'], e3Cell['description']]
                        # e3Cell['url'], 
            print "\t".join(encode3), 
    print "\n"

