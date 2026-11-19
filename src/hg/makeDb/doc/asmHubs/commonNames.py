#!/cluster/software/bin/python3

import os
import sys
import re
import site
from collections import Counter

### NCBI group names given in place of a common name, e.g.:
###   Hordeum bogdanii (monocots)
### these become: monocot H.bogdanii
groupLabels = {
  "flowering plants": "flowering plant",
  "monocots": "monocot",
  "magnoliids": "magnoliid",
  "mosses": "moss",
  "liverworts": "liverwort",
  "hornworts": "hornwort",
  "ferns": "fern",
  "club-mosses": "club-moss",
  "conifers": "conifer",
  "green algae": "green algae",
  "red algae": "red algae",
  # both forms appear for the same species, one label for both
  "chytrids": "chytrids",
  "chytrids & allies": "chytrids",
  # group names isGroupName() can not tell from a common name
  "mosquito": "mosquito",
  "brown algae": "brown algae",
  "grasshoppers, crickets & katydids": "grasshoppers",
  "animals": "",
}

### an NCBI group name given in place of a common name is lower case
### plural: sea cucumbers, tardigrades, butterflies
### a common name is singular: giant kelp, dry rot fungus, chicken-of-the-woods
def isGroupName(name):
    words = name.split()
    if len(words) < 1 or name != name.lower() or "'" in name:
      return False
    last = words[-1]
    # even-toed ungulates is a group, chicken-of-the-woods is not
    if '-' in last:
      return False
    return last.endswith('s') and not last.endswith(('ss', 'us', 'is'))

### the sciName words after the genus, a hybrid second genus is abbreviated
### the same as the first: floridae x Branchiostoma japonicum
###   -> floridae x B.japonicum
def restOfName(words):
    return re.sub(r'(?<= x )([A-Z])[a-z]+ (?=[a-z])', r'\1.', ' '.join(words[1:]))

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
            # keep +, it is the mating type: Podospora anserina S mat+
            pat = re.compile(r'[()\[\]*]')
            sciName = pat.sub('', sciName)
            pat = re.compile(r'\?')
            sciName = pat.sub(' ', sciName)

            pat = r'agent of|aquificales|archaea|bacteria|chlamydias|crenarchaeotes|euryarchaeotes|kinetoplastids|firmicutes|mycoplasmas|planctomycetes|proteobacteria|high G|enterobacteria|s__|spirochetes|verrucomicrobia|thermotogales'
            # a quoted parenthetical is a historical synonym, not a common name:
            #   Brucella suis 1330 ("Organism resembling Bacillus abortus" Traum 1914)
            if re.search(pat, orgName) or '"' in orgName:
              orgName = sciName
            elif orgName in groupLabels:
              words = sciName.split()
              orgName = f"{groupLabels[orgName]} {words[0][0].upper()}.{restOfName(words)}".strip()
            else:
              pat = r'apicomplexans|ants|bacteria|bryozoans|bugs|ciliates|crustaceans|diatoms|hydrozoans|dinoflagellates|gastropods|hemichordates|nematodes|flatworms?|pelagophytes|scorpions|ascomycetes?|basidiomycetes?|budding|microsporidians?|smut|fungi|eukaryotes|flies|beetles|mosquitos|bees|moths|sponges|^mites|ticks|^comb|jellies|jellyfishes|chitons|bivalves|bony fishes|birds|eudicots|snakes|bats|tunicates|tsetse fly|cellular slime molds|stony corals'
              # whole words only, 'ants' must not match 'flowering plants'
              # nor 'birds' match 'birds-eye primrose'
              if re.search(r"(?<![\w'-])(?:" + pat + r")(?![\w'-])", orgName):
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
                restWords = restOfName(words)
                if re.search("eukaryotes", orgName):
                  orgName = words[0][0].upper() + "." + restWords
                elif re.search("apicomplexans", orgName):
                  orgName = "apicomplexans " + words[0][0].upper() + "." + restWords
                elif re.search("ants", orgName):
                  orgName = "ant " + words[0][0].upper() + "." + restWords
                elif re.search("dinoflagellates", orgName):
                  orgName = "dinoflagellates " + words[0][0].upper() + "." + restWords
                elif re.search("\bbacteria\b", orgName):
                  orgName = "bacteria " + words[0][0].upper() + "." + restWords
                elif re.search("hemichordates", orgName):
                  orgName = "hemichordates " + words[0][0].upper() + "." + restWords
                elif re.search("bryozoans", orgName):
                  orgName = "bryozoans " + words[0][0].upper() + "." + restWords
                elif re.search("ciliates", orgName):
                  orgName = "ciliates " + words[0][0].upper() + "." + restWords
                elif re.search("diatoms", orgName):
                  orgName = "diatoms " + words[0][0].upper() + "." + restWords
                elif re.search("hydrozoans", orgName):
                  orgName = "hydrozoans " + words[0][0].upper() + "." + restWords
                elif re.search("gastropods", orgName):
                  orgName = "gastropods " + words[0][0].upper() + "." + restWords
                elif re.search("pelagophytes", orgName):
                  orgName = "pelagophytes " + words[0][0].upper() + "." + restWords
                elif re.search("scorpions", orgName):
                  orgName = "scorpions " + words[0][0].upper() + "." + restWords
                elif re.search("flies", orgName):
                  orgName = "fly " + words[0][0].upper() + "." + restWords
                elif re.search("tsetse", orgName):
                  orgName = "tsetse fly " + words[0][0].upper() + "." + restWords
                elif re.search("cellular slime mold", orgName):
                  orgName = "cellular slime mold " + words[0][0].upper() + "." + restWords
                elif re.search("stony corals", orgName):
                  orgName = "stony coral " + words[0][0].upper() + "." + restWords
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
              elif isGroupName(orgName):
                words = sciName.split()
                orgName = f"{orgName} {words[0][0].upper()}.{restOfName(words)}"

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
            # submitter text inside the value: strain=type strain:CBS 4417
            cultivar = re.sub(r'^type strain:\s*', '', cultivar, flags=re.IGNORECASE)
        else:
          break
      readFile.close()
      # submitters repeat the organism in the strain or isolate, sometimes an
      # older name of the same genus: Psychrobacter aquimaris SW-210,
      # Vogesella sp., Cupriavidus taiwanensis STM 6160, Nitrospira sp. E2OT
      # the whole sciName first, it can carry pv./subsp./serovar:
      #   Xanthomonas arboricola pv. juglandis CPBF 1494
      if len(sciName.strip()):
        pat = r'^' + re.escape(sciName.strip()) + r'(\s+|$)'
        cultivar = re.sub(pat, '', cultivar)
        isolate = re.sub(pat, '', isolate)
      words = [w for w in sciName.split() if w != "Candidatus"]
      if len(words):
        pat = r'^(Candidatus\s+)?' + re.escape(words[0]) + r'\s+(sp\.?|[a-z][\w-]*)(\s+|$)'
        cultivar = re.sub(pat, '', cultivar)
        isolate = re.sub(pat, '', isolate)
      cultivar = re.sub(r'^strain\s+', '', cultivar, flags=re.IGNORECASE)
      isolate = re.sub(r'^strain\s+', '', isolate, flags=re.IGNORECASE)
      if len(isolate) and len(cultivar):
        extraStrings = f"{cultivar} {isolate}{asmType} {yearDate}"
      elif len(isolate):
        extraStrings = f"{isolate}{asmType} {yearDate}"
      elif len(cultivar):
        extraStrings = f"{cultivar}{asmType} {yearDate}"
      if len(extraStrings) < 1:
        pat = re.compile(r'^ +')
        extraStrings = pat.sub('', f"{asmType} {yearDate}")
      
      return asmType, sciName, orgName, yearDate, isolate, cultivar, extraStrings
    except FileNotFoundError:
      print(f"Error: File '{asmRpt}' not found.", file=sys.stderr)
      sys.exit(1)

### drop stray quote marks or parens left behind by the name trimming,
### only when they are unbalanced, e.g.: serovar strain 'Patoc
### a ' that opens a word without a closing partner is a stray quote,
### leave alone apostrophes such as: brewer's yeast, witches' butter
def dropUnbalanced(name):
    openQuotes = len(re.findall(r"(?<!\S)'", name))
    closeQuotes = len(re.findall(r"'(?!\S)", name))
    if openQuotes > closeQuotes:
      name = re.sub(r"(?<!\S)'", '', name)
    if name.count('(') != name.count(')'):
      name = re.sub(r'[()]', '', name)
    return re.sub(r'\s+', ' ', name).strip()

### inFh is a file handle to a list of assembly identifiers,
###  might be a multi column file, ignore anything after the first column
def processList(inFh):
    ncbiSrc = "/hive/data/outside/ncbi/genomes"
    results = []
    for asmId in inFh:
      asmId = asmId.rstrip()	# eliminate new line at end of string
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
      # dmCerClav1.hap1.1 -> hap1.1, ddVioUlig1.alt.1 -> alt.1, GDT2T_hap1 -> hap1
      # leading space to separate it from isolate/cultivar in extraStrings
      m = re.search(r'[._]((?:hap|alt)\d*(?:\.\d+)?)$', asmName)
      if m:
        hapX = " " + m.group(1)
        asmNameAbbrev = asmName[:m.start()]
      elif re.search(r'alternate_haplotype|primary_haplotype', asmNameAbbrev):
        hapX = asmNameAbbrev
        ab = asmNameAbbrev.replace('.hap1','')
        asmNameAbbrev = ab.replace('.hap2','')
        ab = asmNameAbbrev.replace('_alternate_haplotype','')
        asmNameAbbrev = ab.replace('_primary_haplotype','')
        hx = asmNameAbbrev + '.'
        ab = hapX.replace(hx,'')
        hapX = ab
      asmType, sciName, orgName, yearDate, isolate, cultivar, extraStrings = extractNames(asmRpt, hapX)
      outName = dropUnbalanced(orgName)
      if len(extraStrings):
        # Preserve parentheses for viral strain nomenclature, only remove brackets and other chars
        pat = re.compile(r'[\[\]*]')
        extraStrings = pat.sub('', extraStrings)
        pat = re.compile(r'\?')
        extraStrings = pat.sub(' ', extraStrings)
        # the asmName is noise in an isolate (dmCerClav1 hap1.1), but when
        # it is the strain itself (strain=Foc013) it is the identifier, keep it
        if asmNameAbbrev != cultivar.strip():
          extraStrings = re.sub(r'(?<!\S)' + re.escape(asmNameAbbrev) + ' ', '', extraStrings)
        extraStrings = re.sub(r'\s+', ' ', extraStrings)
        # drop the cultivar and isolate from orgName only as whole phrases,
        # word by word would turn: brown hare + breed=Cape hare -> brown
        # = separates strain synonyms, a space lets the strain match as a
        # whole phrase: Rhizophagus irregularis DAOM 181602=DAOM 197198
        orgName = re.sub(r'\s+', ' ', orgName.replace('=', ' '))
        for phrase in (cultivar, isolate):
          # orgName has had these characters removed, match that form too,
          # the = also in the phrase: DSM 2637 = ATCC 25755 = JCM 11008
          for qMark in ('', ' '):
            p = re.sub(r'[()\[\]*]', '', phrase).replace('?', qMark).replace('=', ' ')
            p = re.sub(r'\s+', ' ', p).strip()
            if len(p):
              orgName = re.sub(r'(?<!\S)' + re.escape(p) + r'(?!\S)', '', orgName)
        orgName = re.sub(r'=|\s+$|^\s+', '', orgName)
        orgName = re.sub(r'\s+', ' ', orgName).strip()
        orgName = dropUnbalanced(orgName)
        if len(orgName):
          outName = f"{orgName} ({extraStrings})"
        else:
          outName = extraStrings

      results.append([asmId, outName])

#      print(f"asmType: '{asmType}', {sciName}, {orgName}, {yearDate}, {isolate}, {cultivar} {extraStrings}")
    return results

### read the asmId and name columns of an existing orderList to check new
### names against, skip the asmIds being named in this run
def readOrderList(orderList, skipIds):
    taken = {}
    try:
      with open(orderList, 'r', encoding='utf-8', errors='replace') as fh:
        for line in fh:
          f = line.rstrip('\n').split('\t')
          if len(f) < 2 or f[0].startswith('#') or f[0] in skipIds:
            continue
          taken.setdefault(f[1].strip(), []).append(f[0])
    except FileNotFoundError:
      print(f"Error: File '{orderList}' not found.", file=sys.stderr)
      sys.exit(1)
    return taken

### add a tag inside the parens, first: (v2 2026) or last: (2021 refseq)
def addTag(name, tag, atEnd=False):
    if name.endswith(')') and '(' in name:
      if atEnd:
        return f"{name[:-1]} {tag})"
      # the ( that matches the final ), they can nest: (p50T (= Dazao) 2008)
      depth = 0
      for i in range(len(name) - 1, -1, -1):
        if name[i] == ')':
          depth += 1
        elif name[i] == '(':
          depth -= 1
          if depth == 0:
            break
      return f"{name[:i+1]}{tag} {name[i+1:]}"
    return f"{name} ({tag})"

### the asmName part of an asmId when it says something about the assembly,
### empty for NCBI default names (ASM1411746v1), bare versions (v2.0) and
### accession look-alikes (GCA_900093555), the hap suffix is dropped since
### the name already has it: idAnoCruz5.1_alternate_haplotype -> idAnoCruz5.1
def asmNameTag(asmId):
    p = asmId.split('_', 2)
    if len(p) < 3:
      return ""
    asmName = re.sub(r'_(alternate|primary)_haplotype$|[._](hap|alt)\d*(\.\d+)?$', '', p[2])
    if re.fullmatch(r'ASM\d+v\d+|[vV]?\d+(\.\d+)*|GC[AF]_\d+(\.\d+)?', asmName):
      return ""
    return asmName

### names that collide, within this list or with the orderList, when they
### are all the same accession number: the version (v2) first when the
### versions differ, genbank/refseq last when both GCA and GCF are there,
### otherwise the asmName when it is informative and unique in the group
### (IHEM_4915, BMD700), else the accession itself
def makeUnique(results, taken):
    groups = {}
    for r in results:
      groups.setdefault(r[1], []).append(r)
    for name, group in groups.items():
      allIds = [r[0] for r in group] + taken.get(name, [])
      if len(allIds) < 2:
        continue
      gcxs = set(asmId.split('_')[0] for asmId in allIds)
      accs = [asmId.split('_')[1] for asmId in allIds]
      sameNumber = len(set(a.split('.')[0] for a in accs)) == 1
      useVersion = len(set(accs)) > 1
      useSource = len(gcxs) > 1
      asmTags = Counter(asmNameTag(asmId) for asmId in allIds)
      for r in group:
        p = r[0].split('_')
        if not sameNumber:
          tag = asmNameTag(r[0])
          if len(tag) and asmTags[tag] == 1:
            r[1] = addTag(name, tag)
          else:
            r[1] = addTag(name, f"{p[0]}_{p[1]}")
          continue
        newName = name
        if useVersion:
          newName = addTag(newName, f"v{p[1].split('.')[-1]}")
        if useSource:
          newName = addTag(newName, "refseq" if p[0] == "GCF" else "genbank", atEnd=True)
        r[1] = newName
    counts = Counter(r[1] for r in results)
    for r in results:
      if counts[r[1]] > 1 or r[1] in taken:
        print(f"warning: duplicate name: {r[0]}\t{r[1]}", file=sys.stderr)

def main():
    site.ENABLE_USER_SITE = False
    if len(sys.argv) not in (2, 3):
#        print(f"sys.path: {sys.path}")
        print("Usage: ./commonNames.py <filename|stdin> [existing.orderList.tsv]")
        print("e.g.: ./commonNames.py some.asmId.list plants.orderList.tsv")
        print("    where some.asmId.list is a simple list of NCBI assembly ids")
        print("    will look up the common names for each ID from the assembly_report files")
        print("    optional orderList: also make names unique against the names there")
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

    results = processList(fileIn)
    taken = {}
    if len(sys.argv) == 3:
      taken = readOrderList(sys.argv[2], {r[0] for r in results})
    makeUnique(results, taken)
    for asmId, name in results:
      print(f"{asmId}\t{name}")

if __name__ == "__main__":
    main()
