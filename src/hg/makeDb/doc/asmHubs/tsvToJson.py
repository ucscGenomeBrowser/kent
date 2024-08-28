#!/cluster/software/bin/python3

import os
import sys
import re
import site
import json

####################################################################
### this is kinda like an environment setting, it gets everything
### into a UTF-8 reading mode
####################################################################
def setUtf8Encoding():
    """
    Set UTF-8 encoding for stdin, stdout, and stderr in Python.
    """
    if sys.stdout.encoding != 'utf-8':
        sys.stdout = open(sys.stdout.fileno(), mode='w', encoding='utf-8', buffering=1)
    if sys.stderr.encoding != 'utf-8':
        sys.stderr = open(sys.stderr.fileno(), mode='w', encoding='utf-8', buffering=1)


### given a path name to an asmRpt, read the file and get out the names
### we need
def extractNames(asmRpt, hapX):
    asmType = "asmType"
    sciName = "sciName"
    yearDate = "yearDate"
    isolate = ""
    cultivar = ""
    orgName = "orgName"
    order = ""
    extraStrings = ""
    identical = False
    refSeqAcc = None
    genBankAcc = None
    taxId = 0
    try:
      readFile = open(asmRpt, 'r', encoding='utf-8')
      for line in readFile:
        line = line.rstrip()
        if line.startswith('#'):
          if "Assembly type:" in line:
            pat = re.compile(r'.*bly type:\s+', re.IGNORECASE)
            asmType = pat.sub('', line)
            if re.search('alternate-pseudohaplotype', asmType):
              asmType = " alternate hap"
            elif re.search('principal pseudo', asmType):
              asmType = " primary hap"
            else:
              asmType = hapX
          elif "taxid:" in line.lower():
            words = line.split()
            taxId = words[-1]
          elif "assemblies identical:" in line:
            words = line.split()
            if "yes" in words[-1]:
               identical = True
          elif "refseq assembly accession:" in line.lower():
            words = line.split()
            refSeqAcc = words[-1]
          elif "genbank assembly accession:" in line.lower():
            words = line.split()
            genBankAcc = words[-1]
          elif "Organism name:" in line:
            saveOrgName = line
            pat = re.compile(r'.*\(')
            orgName = pat.sub('', line)
            pat = re.compile(r'[()\[\]+*]')
            orgName = pat.sub('', orgName)
            pat = re.compile(r'\?')
            orgName = pat.sub('', orgName)
            pat = re.compile(r'.*ism name:\s+')
            orgName = pat.sub('', orgName)
           

            pat = re.compile(r'.*ganism name:\s+', re.IGNORECASE)
            sciName = pat.sub('', line)
            pat = re.compile(r'\s+\(.*\)$')
            sciName = pat.sub('', sciName)
            pat = re.compile(r'[()\[\]+*]')
            sciName = pat.sub('', sciName)
            pat = re.compile(r'\?')
            sciName = pat.sub(' ', sciName)

            pat = r'kinetoplastids|firmicutes|proteobacteria|high G|enterobacteria|agent of'
            if re.search(pat, orgName):
              orgName = sciName
            else:
              pat = r'bugs|crustaceans|nematodes|flatworm|ascomycete|basidiomycete|budding|microsporidian|smut|fungi|eukaryotes|flies|beetles|mosquitos|bees|moths|sponges|^mites|ticks|^comb|jellies|jellyfishes|chitons|bivalves|bony fishes|birds|eudicots|snakes|bats|tunicates|tsetse fly'
              if re.search(pat, orgName):
                order = orgName.split()[0]
                if re.search('budding', order):
                  order = "budding yeast"
                elif re.search('smut', order):
                  order = "smut fungi"
                elif re.search('bony', order):
                  order = "bony fish"
                elif re.search('ascomycete', order):
                  order = "ascomycetes"
                elif re.search('eudicots', order):
                  order = "eudicot"
                elif re.search('birds', order):
                  order = "bird"
                elif re.search('snakes', order):
                  order = "snake"
                elif re.search('crustaceans', order):
                  order = "crustacean"
                elif re.search('mosquitos', order):
                  order = "mosquito"
                elif re.search('mites', order):
                  order = "mite/tick"
                elif re.search('comb', order):
                  order = "comb jelly"
                elif re.search('jellyfishes', order):
                  order = "jellyfish"
                elif re.search('chitons', order):
                  order = "chiton"
                elif re.search('bivalves', order):
                  order = "bivalve"
                elif re.search('beetles', order):
                  order = "beetle"
                elif re.search('bees', order):
                  order = "bee"
                elif re.search('bats', order):
                  order = "bat"
                elif re.search('moths', order):
                  order = "moth"
                elif re.search('sponges', order):
                  order = "sponge"
                elif re.search('flatworms', order):
                  order = "flatworm"
                elif re.search('nematodes', order):
                  order = "nematode"
                elif re.search('basidiomycete', order):
                  order = "basidiomycetes"
                words = sciName.split()
                restWords = " ".join(words[1:])
                if re.search("eukaryotes", orgName):
                  orgName = words[0][0].upper() + "." + restWords
                elif re.search("flies", orgName):
                  orgName = "fly " + words[0][0].upper() + "." + restWords
                elif re.search("tsetse", orgName):
                  orgName = "tsetse fly " + words[0][0].upper() + "." + restWords
                elif re.search("tunicates", orgName):
                  orgName = "tunicate " + words[0][0].upper() + "." + restWords
                else:
                  orgName = order + " " + words[0][0].upper() + "." + restWords
              elif re.search("viruses", orgName):
                orgName = saveOrgName
                pat = re.compile(r'.*ism name:\s+')
                orgName = pat.sub('', orgName)
                pat = re.compile(r'\s+\(.*\)$')
                orgName = pat.sub('', orgName)

          elif "Date:" in line:
            words = line.split()
            pat = re.compile(r'-.*')
            yearDate = pat.sub('', words[-1])
          elif "Isolate:" in line:
            pat = re.compile(r'.*solate:\s+')
            isolate = pat.sub('', line)
          elif "Infraspecific name:" in line:
            pat = re.compile(r'.*cultivar=|.*ecotype=|.*strain=|.*breed=')
            cultivar = pat.sub('', line)
        else:
          break
      readFile.close()
      if len(isolate) and len(cultivar):
        extraStrings = f"{cultivar} {isolate}{asmType} {yearDate}"
      elif len(isolate):
        extraStrings = f"{isolate}{asmType} {yearDate}"
      elif len(cultivar):
        extraStrings = f"{cultivar}{asmType} {yearDate}"
      if len(extraStrings) < 1:
        pat = re.compile(r'^ +')
        extraStrings = pat.sub('', f"{asmType} {yearDate}")
      
      return asmType, sciName, orgName, yearDate, isolate, cultivar, extraStrings, genBankAcc, refSeqAcc, identical, taxId
    except FileNotFoundError:
      print(f"Error: File '{asmRpt}' not found.", file=sys.stderr)
      sys.exit(1)

### inFh is a file handle to a list of assembly identifiers,
###  might be a multi column file, ignore anything after the first column
def processList(inFh):
    outStr = f"taxId\tasmId\tgenBankAcc\trefSeqAcc\tidentical\tsciName\tcomName"
    print(outStr, file=sys.stderr)
    schema = [
        {"name": "taxId", "type": "integer"},
        {"name": "asmId", "type": "string"},
        {"name": "genBank", "type": "string"},
        {"name": "refSeq", "type": "string"},
        {"name": "identical", "type": "boolean"},
        {"name": "sciName", "type": "string"},
        {"name": "comName", "type": "string"},
        {"name": "ucscBrowser", "type": "string"},
    ]
    jsonSchema = {
      "columns": [[obj["name"], obj["type"]] for obj in schema]
    }
##    jsonOut = json.dumps(jsonSchema)
##    print(jsonOut)
    ### accumulating array rows
    dataOut = []
    ncbiSrc = "/hive/data/outside/ncbi/genomes"
    for lineIn in inFh:
      #  rstrip() # eliminates new line at end of string
      splitByTab = lineIn.rstrip().split("\t")
      asmId = splitByTab[0]
      comName = splitByTab[1]
      if asmId.startswith('#'):	# ignore comment lines
        print(f"{asmId}", file=sys.stderr)
        continue
      asmId = asmId.split(' ',1)[0]	# only the first column
      p = asmId.split('_', 2)	# split on _ maximum of three results in list
      accession = p[0] + '_' + p[1]
      nPart = p[1]
      by3 = [nPart[i:i+3] for i in range(0, len(nPart), 3)]
      gcX = p[0]
      d0 = by3[0]
      d1 = by3[1]
      d2 = by3[2]
      asmName = "na"
      if (len(p) == 3):
        asmName = p[2]
#      print(f"{accession}\t{asmName}\t{asmId}")
      srcDir = f"{ncbiSrc}/{gcX}/{d0}/{d1}/{d2}/{asmId}"
      asmRpt = f"{srcDir}/{asmId}_assembly_report.txt"
#      print(f"{asmRpt}", file=sys.stderr)
      if not os.path.isfile(asmRpt):
        print(f"{asmId}\tmissing '{asmRpt}'", file=sys.stderr)
        continue
      asmNameAbbrev = asmName
      hapX = ""
      abbrevPattern = r'hap1|hap2|alternate_haplotype|primary_haplotype'
      if re.search(abbrevPattern, asmNameAbbrev):
        hapX = asmNameAbbrev
        ab = asmNameAbbrev.replace('.hap1','')
        asmNameAbbrev = ab.replace('.hap2','')
        ab = asmNameAbbrev.replace('_alternate_haplotype','')
        asmNameAbbrev = ab.replace('_primary_haplotype','')
        hx = asmNameAbbrev + '.'
        ab = hapX.replace(hx,'')
        hapX = ab
      asmType, sciName, orgName, yearDate, isolate, cultivar, extraStrings, genBankAcc, refSeqAcc, identical, taxId = extractNames(asmRpt, hapX)
      if len(extraStrings):
        pat = re.compile(r'[()\[\]+*]')
        extraStrings = pat.sub('', extraStrings)
        pat = re.compile(r'\?')
        extraStrings = pat.sub(' ', extraStrings)
        extraStrings = re.sub(re.escape(asmNameAbbrev) + ' ', '', extraStrings)
        extraStrings = re.sub(r'\s+', ' ', extraStrings)
        words = extraStrings.split()
        orgList = orgName.split()
        orgName = " ".join([orgWord for orgWord in orgList if orgWord not in words])
        orgName = re.sub(r'=|\s+$|^\s+', '', orgName)
        orgName = re.sub(r'\s+', ' ', orgName).strip()
        outStr = f"{taxId}\t{asmId}\t{genBankAcc}\t{refSeqAcc}\t{identical}\t{sciName}\t{comName}"

        ucscBrowser = f"https://genome.ucsc.edu/h/{accession}"
        rowData = {
          "taxId": int(taxId),
          "asmId": asmId,
          "genBank": genBankAcc,
          "refSeq": refSeqAcc,
          "identical": identical,
          "sciName": sciName,
          "comName": comName,
          "ucscBrowser": ucscBrowser,
        }
        dataOut.append(rowData)
        print(outStr, file=sys.stderr)

    dataJson = {
      "data": [[obj["taxId"], obj["asmId"], obj["genBank"], obj["refSeq"], obj["identical"], obj["sciName"], obj["comName"], obj["ucscBrowser"]] for obj in dataOut]
    }
#    schemaPlusData = schema + dataOut
    schemaPlusData = {
      "columns": schema,
      "data": dataOut,
    }
    jsonOut = json.dumps(schemaPlusData)
    print(jsonOut)

#      print(f"asmType: '{asmType}', {sciName}, {orgName}, {yearDate}, {isolate}, {cultivar} {extraStrings}")

def main():
    site.ENABLE_USER_SITE = False
    if len(sys.argv) != 2:
#        print(f"sys.path: {sys.path}")
        print("Usage: ./commonNames.py <filename|stdin>", file=sys.stderr)
        print("e.g.: ./commonNames.py some.asmId.list", file=sys.stderr)
        print("    where some.asmId.list is a simple list of NCBI assembly ids", file=sys.stderr)
        print("    will look up the common names for each ID from the assembly_report files", file=sys.stderr)
        sys.exit(1)

    # Ensure stdout and stderr use UTF-8 encoding
    setUtf8Encoding()

    listFile = sys.argv[1]

    if listFile == 'stdin':
        fileIn = sys.stdin
    else:
        try:
          fileIn = open(listFile, 'r')
        except FileNotFoundError:
          print(f"Error: File '{listFile}' not found.", file=sys.stderr)
          sys.exit(1)

    processList(fileIn)

if __name__ == "__main__":
    main()
