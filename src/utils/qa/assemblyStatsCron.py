#07/20/19
#This was adapted from a jupyter notebook - hence lots of weird bash calls

import datetime
from collections import OrderedDict
import getpass
import subprocess
import os
import urllib.parse

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True,
                                       encoding="utf-8", errors="replace", stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

def bashNoErrorCatch(cmd):
    """Run the cmd in bash subprocess, don't catch error since grep returns exit code 1 when no match is found"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True,
                                       encoding="utf-8", errors="replace", stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout.rstrip().split("\n")
    except:
        bashStdoutt = []
    return(bashStdoutt)

user = getpass.getuser()
outputDir = "/hive/users/"+user+"/ErrorLogsOutput"
logDir = "/hive/users/"+user+"/ErrorLogs"

# genome-asia scps its hubStatus dump into the output dir on the 28th of each month (qateam
# cannot ssh to asia, so the delivery has to go the other way). Wiping it here is what silently
# dropped every asia-only hub from this report between Feb 2025 and Aug 2026.
asiaHubStatusFile = outputDir+"/genomeAsiaHubStatus.txt"

#Clean out any previous unfinished run, keeping anything delivered here by another machine
bash("mkdir -p "+logDir+" "+outputDir)
bash("rm -f "+logDir+"/*")
bash("find "+outputDir+" -maxdepth 1 -type f ! -name genomeAsiaHubStatus.txt -delete")

# Get the year to query proper wwwstats directory
today = datetime.datetime.today()
year = str(today).split('-')[0]

# Hubs are only trusted from a mirror that has refreshed them recently. Kept as today-30d.
lastMonth = today - datetime.timedelta(days=30)
# The month this report is *about*, used only for naming the archive and published files.
# "today - 30 days" cannot be used for the label: run on March 1 it lands on January 30 and
# the March run then overwrites January's published file, which is why no February report
# has ever existed. Backing up from the 1st of this month is right for every calendar date.
labelMonth = (today.replace(day=1) - datetime.timedelta(days=1)).strftime('%Y-%m')

# Get the last 5 error logs from the RR
latestLogs = bash('ls /hive/users/chmalee/logs/trimmedLogs/result/hgw1').rstrip().split("\n")

######################## TESTING MODE - ONLY PROCESS MINIMAL AMOUNT OF LOGS ########################
testMode = False #Set true for testing mode
#################################################################################################

if testMode: #Default is one RR log and one asia log
    latestLogs = latestLogs[len(latestLogs)-1:]
    nodes = ['RR', 'asiaNode'] #Add nodes with error logs, nodes can be added or removed
    machines = ['hgw1'] #Add hgw machines to check
else:
    latestLogs = latestLogs[len(latestLogs)-5:]
    nodes = ['RR', 'asiaNode', 'euroNode'] #Add nodes with error logs, nodes can be added or removed
    machines = ['hgw1','hgw2'] #Add hgw machines to check

for node in nodes:
    if node == 'RR':
        for machine in machines:
            for log in latestLogs: #Copy the 5 latest error logs for each of the rr machines
                bash("cp /hive/users/chmalee/logs/trimmedLogs/result/"+machine+'/'+log+' '+logDir+'/'+node+machine+log)

    else:
        latestLogs = bash("ls /hive/users/chmalee/logs/trimmedLogs/result/"+node).rstrip().split("\n")
        if testMode:
            latestLogs = latestLogs[len(latestLogs)-1:]
        else:
            latestLogs = latestLogs[len(latestLogs)-5:]

        for log in latestLogs: #Copy the 5 latest error logs for each of the other nodes
            bash('cp /hive/users/chmalee/logs/trimmedLogs/result/'+node+'/'+log+' '+logDir+'/'+node+log)

# Run generateUsageStats.py with -d (directory), -t (default track stats), -o (output)
bash("/cluster/home/"+user+'/kent/src/hg/logCrawl/dbTrackAndSearchUsage/generateUsageStats.py -d '+logDir+' --allOutput -t -o '+outputDir+' > /dev/null')

#####################################################################################
##### Resolve every hub id seen in the logs against the mirror hubStatus tables #####
#####################################################################################

# Everything downstream - which assemblies are GenArk, which hubs are hubSpace, which are
# public - is decided by the hub's URL, so the resolution has to happen before any section
# is written.

def hubIdFromName(name):
    """Pull the hub id out of a hub_<id>_<rest> track or database name, or return None"""
    if not name.startswith("hub_"):
        return None
    splitName = name.split("_")
    if len(splitName) > 2 and splitName[1].isdigit():
        return splitName[1]
    return None

def stripHubPrefix(name):
    """hub_164399_GCA_004023905.1 -> GCA_004023905.1, leaving non-hub names alone"""
    if hubIdFromName(name) is None:
        return name
    return "_".join(name.split("_")[2:])

def normalizeHubUrl(hubUrl):
    """Same hub, different registration: http vs https, a ?genome= suffix, a trailing slash.
       Compare on host and path alone or a public hub reappears in the non-public list."""
    normalized = hubUrl.split("?")[0].rstrip("/").lower()
    for scheme in ("https://", "http://", "ftp://"):
        if normalized.startswith(scheme):
            normalized = normalized[len(scheme):]
            break
    return normalized

# Sorted descending by use, so the first line seen for a hub id is its most used track
bash('sort '+outputDir+'/trackCounts.tsv -rnk3 > '+outputDir+'/trackCounts.tsv.sorted')
bash('grep "hub_" '+outputDir+'/trackCounts.tsv | sort -rnk3 > '+outputDir+'/allTracksOrderedUsage.txt')

bestTrackForHub = OrderedDict() #hubId -> [useCount, db, track]
with open(outputDir+'/allTracksOrderedUsage.txt') as allHubTracks:
    for line in allHubTracks:
        splitLine = line.rstrip("\n").split("\t")
        if len(splitLine) < 3:
            continue
        hubId = hubIdFromName(splitLine[1])
        if hubId is None:
            # The id is only on the database (an assembly hub whose track is a custom track or
            # oligoMatch). Those are not hub tracks and have no cross-mirror count to report.
            continue
        if hubId not in bestTrackForHub:
            bestTrackForHub[hubId] = [int(splitLine[2]), splitLine[0], splitLine[1]]

# Assembly hubs can also appear only as a database, with no hub_ track of their own
hubIdsSeen = set(bestTrackForHub.keys())
with open(outputDir+'/dbCounts.tsv') as dbCountsForHubs:
    for line in dbCountsForHubs:
        hubId = hubIdFromName(line.split("\t")[0])
        if hubId is not None:
            hubIdsSeen.add(hubId)

#Query hubPublic and hubStatus in order to filter out public hubs then sort out the IDs
bash('/cluster/bin/x86_64/hgsql -h genome-centdb -e "select hubUrl from hubPublic" hgcentral > '+outputDir+'/hubPublicHubUrl.txt')
bash('/cluster/bin/x86_64/hgsql -h genome-centdb -e "select hubUrl,id from hubStatus" hgcentral> '+outputDir+'/hubStatusHubUrl.txt')
bash('grep -f '+outputDir+'/hubPublicHubUrl.txt '+outputDir+'/hubStatusHubUrl.txt | cut -f2 > '+outputDir+'/publicIDs.txt')

#Add hub_ID format to match stats program output, then grep out the public hubs from the track list
bash('cat '+outputDir+'/publicIDs.txt | sed s/^/hub_/g > '+outputDir+'/hubPublicIDs.txt')

#Pull out whole fields from euro and RR hubStatus in order to collect the info for matching IDs.
#A mirror being unreachable should cost us that mirror's hubs, not the whole report.
mirrorWarnings = []
def dumpHubStatus(description, cmd):
    try:
        bash(cmd)
    except RuntimeError as e:
        mirrorWarnings.append("WARNING: could not read the "+description+" hubStatus table, its "
                              "hubs are missing from the counts below. "+str(e).split("\n")[0])

dumpHubStatus('genome-euro', 'ssh qateam@genome-euro "hgsql -e \'select id,hubUrl,shortLabel,lastOkTime from hubStatus\' hgcentral" > '+outputDir+'/genomeEuroHubStatus.txt')
dumpHubStatus('RR', '/cluster/bin/x86_64/hgsql -h genome-centdb -e "select id,hubUrl,shortLabel,lastOkTime from hubStatus where lastOkTime !=\'\'" hgcentral > '+outputDir+'/RRHubStatus.txt')
#The genome-asia hubStatus is copied here monthly by a qateam cron on asia (sendHubStatusToDev.sh).
#Named users cannot read qateam's key there, so they make their own copy instead.
if user != 'qateam':
    dumpHubStatus('genome-asia', "ssh "+user+"@genome-asia \"hgsql -e 'select id,hubUrl,shortLabel,lastOkTime from hubStatus' hgcentral\" > "+asiaHubStatusFile)

# Warnings that belong in the report itself. The cron redirects stderr to /dev/null and bash()
# swallows it besides, so anything written there would be as invisible as the bug it reports.
reportWarnings = list(mirrorWarnings)

if not os.path.exists(asiaHubStatusFile):
    reportWarnings.append("WARNING: no genome-asia hubStatus file. Asia-only hubs are missing "
                          "from the hub counts below. Expected at "+asiaHubStatusFile)
else:
    asiaAgeDays = (today - datetime.datetime.fromtimestamp(os.path.getmtime(asiaHubStatusFile))).days
    if asiaAgeDays > 40:
        reportWarnings.append("WARNING: the genome-asia hubStatus file is "+str(asiaAgeDays)+
                              " days old. Asia hub counts below may be stale.")

def loadHubStatus(statusFile, machine, hubStatusByMirror):
    """Keep the rows for hub ids we actually saw. These files hold millions of rows, so they
       are streamed once rather than grepped per hub."""
    if not os.path.exists(statusFile):
        return
    with open(statusFile, encoding="utf-8", errors="replace") as statusHandle:
        for line in statusHandle:
            splitLine = line.rstrip("\n").split("\t")
            if len(splitLine) != 4 or not splitLine[0].isdigit():
                continue #skips the mysql header row
            if splitLine[0] in hubIdsSeen:
                hubStatusByMirror.setdefault(splitLine[0], OrderedDict())[machine] = splitLine[1:]

hubStatusByMirror = {} #hubId -> {machine: [hubUrl, shortLabel, lastOkTime]}
loadHubStatus(outputDir+'/RRHubStatus.txt', 'RR', hubStatusByMirror)
loadHubStatus(outputDir+'/genomeEuroHubStatus.txt', 'Euro', hubStatusByMirror)
loadHubStatus(asiaHubStatusFile, 'Asia', hubStatusByMirror)

def hubIsCurrent(lastOkTime):
    """A mirror only speaks for a hub id if it refreshed the hub recently. Hub ids are handed
       out per hgcentral, so the same number means different hubs on different mirrors."""
    if lastOkTime in ('', 'NULL'):
        return False
    try:
        return datetime.datetime.strptime(lastOkTime.split(" ")[0], '%Y-%m-%d') > lastMonth
    except ValueError:
        return False

ambiguousHubIds = []

def resolveHub(hubId):
    """Return [hubUrl, shortLabel, machine] for a hub id, or None if no mirror can vouch for it"""
    candidates = [(machine, fields) for machine, fields in hubStatusByMirror.get(hubId, {}).items()
                  if hubIsCurrent(fields[2])]
    if not candidates:
        return None
    if len(candidates) > 1:
        # Same id, different hubs. The track name we saw in the logs usually matches the right
        # hub's shortLabel, so trust that before falling back to whoever refreshed most recently.
        observedTrack = stripHubPrefix(bestTrackForHub[hubId][2]).lower() if hubId in bestTrackForHub else ""
        #Needs enough label to be evidence - a two letter shortLabel matches almost anything
        matching = [c for c in candidates if len(c[1][1]) >= 8 and
                    (c[1][1].lower() in observedTrack or observedTrack.startswith(c[1][1].lower()[:20]))]
        if len(matching) == 1:
            candidates = matching
        #Otherwise fall through on mirror order, RR first, which is how this has always
        #resolved and matches a report written from the RR's point of view
        if len({normalizeHubUrl(c[1][0]) for c in candidates}) > 1:
            ambiguousHubIds.append(hubId)
    machine, fields = candidates[0]
    return [fields[0], fields[1], machine]

resolvedHubs = {} #hubId -> [hubUrl, shortLabel, machine]
for hubId in hubIdsSeen:
    resolved = resolveHub(hubId)
    if resolved is not None:
        resolvedHubs[hubId] = resolved

#Public hubs are recognised two ways. By hub id, which is how generateUsageStats.py decides
#what goes in the public hub section, so the two sections cannot disagree. And by normalized
#URL, which catches the same hub registered a second time under a slightly different address.
publicHubIds = set(bashNoErrorCatch('cat '+outputDir+'/publicIDs.txt'))
pubHubUrls = set(normalizeHubUrl(url) for url
                 in bash('cat '+outputDir+'/hubPublicHubUrl.txt').rstrip().split("\n") if url)

def hubIsPublic(hubId, hubUrl, machine):
    #publicIDs.txt is built from the RR hgcentral, so the id only means something for a hub
    #resolved from the RR. The same integer is a different hub on euro or asia.
    if machine == 'RR' and hubId in publicHubIds:
        return True
    return normalizeHubUrl(hubUrl) in pubHubUrls

def hubCategory(hubUrl):
    """Sort a hub into the family that decides how it gets reported"""
    hubUrl = hubUrl.lower()
    if '/gbdb/genark/' in hubUrl:
        return 'genark'
    elif '/hubspace/' in hubUrl:
        return 'hubspace'
    elif hubUrl.startswith('../trash/hgcomposite/'):
        return 'trackCollection' #made by the browser's own track collection tool, not a real hub
    elif 'encodeproject.org/batch_hub/' in hubUrl:
        return 'encodeSearch' #one throwaway hub per "Visualize" click on encodeproject.org
    elif hubUrl.startswith('/gbdb/'):
        return 'curated'
    return 'other'

def hubSpaceUser(hubUrl):
    """https://genome.ucsc.edu/hubspace/ac/Carolina+Alberca/MM_TB12949_SNP/hub.txt -> Carolina Alberca"""
    splitUrl = hubUrl.split('/hubspace/')[1].split('/')
    if len(splitUrl) < 2:
        return None
    return urllib.parse.unquote_plus(splitUrl[1])

# An assembly is GenArk if any hub id serving it resolves to a /gbdb/genark/ URL. Requiring
# every id to resolve would drop assemblies merely for having one id we cannot look up, and
# most of those are asia ids.
genArkAccessions = set()
genArkLabels = {}
for hubId in sorted(resolvedHubs):
    resolved = resolvedHubs[hubId]
    if hubCategory(resolved[0]) != 'genark':
        continue
    #/gbdb/genark/GCF/036/323/735/GCF_036323735.1/hub.txt -> GCF_036323735.1
    accession = resolved[0].split('/')[-2]
    genArkAccessions.add(accession)
    genArkLabels.setdefault(accession, resolved[1])
    if hubId in bestTrackForHub and hubIdFromName(bestTrackForHub[hubId][1]) is not None:
        genArkAccessions.add(stripHubPrefix(bestTrackForHub[hubId][1]))
# An assembly hub's accession also shows up as a database name of its own
with open(outputDir+'/dbCounts.tsv') as dbCountsForGenArk:
    for line in dbCountsForGenArk:
        dbName = line.split("\t")[0]
        hubId = hubIdFromName(dbName)
        if hubId is not None and hubId in resolvedHubs and hubCategory(resolvedHubs[hubId][0]) == 'genark':
            genArkAccessions.add(stripHubPrefix(dbName))

#The following section pulls out a list of default track for the top X assemblies for filtering
defaultsFile = open(outputDir+"/defaults.txt", "w")

#The head command can be expanded to be more inclusive if additional assembly defaults are finding their way onto the list
bash("sort "+outputDir+'/dbCounts.tsv -rnk2 > '+outputDir+'/dbCountsTopSorted.tsv')
topDbs = bash('head -n 10 '+outputDir+'/dbCountsTopSorted.tsv | cut -f1 -d "\t"').rstrip().split("\n")
#Hub backed databases are skipped - hub ids differ between hgcentrals, so the track names
#hgTracks hands back here would never match the ones seen in the logs.
topDbs = [db for db in topDbs if not db.startswith("hub_")][0:4]

defaultTracks = set()
for db in topDbs:
    #The following part queries hgTracks for each of the assemblies and extracts the list of defaults
    bash('echo '+db+' > '+outputDir+'/temp.txt')
    #Must run from the cgi-bin directory or hgTracks cannot find ../htdocs/urw-fonts and dies
    #before it ever logs the track list. The space before db= matters just as much - without it
    #cgiSpoof swallows the whole string and every assembly silently returns hg38's defaults.
    defaults = bashNoErrorCatch('cd /usr/local/apache/cgi-bin && HGDB_CONF=$HOME/.hg.conf.beta '
                                './hgTracks hgt.trackImgOnly=1 db='+db+' > /dev/null')

    #hgTracks writes "trackLog N db hgsid track:vis,track:vis" to stderr, split into ~800 byte
    #blocks so Apache does not chop the lines. Take every numbered block; the trailing
    #"trackLog position" line and the CGI_TIME/RESOURCE lines are not track lists.
    tracksForDb = []
    for defaultsLine in defaults:
        splitLine = defaultsLine.split(" ")
        if len(splitLine) > 4 and splitLine[0] == "trackLog" and splitLine[1].isdigit():
            tracksForDb.extend(splitLine[4].split(","))

    if not tracksForDb:
        reportWarnings.append("WARNING: hgTracks returned no default track list for "+db+
                              ", its tracks are not filtered out of the non-default list below.")
        continue

    for track in tracksForDb:
        track = track.rsplit(":", 1)[0] #drop the trailing visibility
        if track:
            defaultsFile.write(db+"\t"+track+"\n")
            defaultTracks.add((db, track))
    defaultsFile.write(db+"\t"+"cytoBand"+"\n")
    defaultTracks.add((db, "cytoBand"))
defaultsFile.close()

#############################################
##### Build the report, section by section ##
#############################################

def formatTable(header, rows):
    """Line the columns up, header included, and size the rule to the table. tabFmt only ever
       saw the data rows, so the headers never lined up with the columns underneath them."""
    allRows = [[str(cell) for cell in row] for row in [header] + list(rows)]
    for row in allRows:
        while len(row) < len(header):
            row.append("")
        del row[len(header):]
    widths = [max(len(row[col]) for row in allRows) for col in range(len(header))]
    tableWidth = sum(widths) + 2 * (len(widths) - 1)
    out = []
    for rowNumber, row in enumerate(allRows):
        out.append("  ".join(cell.ljust(widths[col]) for col, cell in enumerate(row)).rstrip())
        if rowNumber == 0:
            out.append("-" * tableWidth)
    return "\n".join(out)

resultsLines = []
resultsLines.append("This cronjob pulls out GB stats over the last month, across all RR machines "
                    "and Asia/Euro mirrors using the generateUsageStats.py script. It only counts "
                    "each hgsid occurrence once, filtering out any hgsid that only showed up one time.")

##### Report the database usage, aggregating curated hubs and GenArk ######

dbCountsRaw = open(outputDir+'/dbCounts.tsv','r')
dbCountsCombined = open(outputDir+'/dbCountsCombinedWithCuratedHubs.tsv','w')

dbsCounts = {}
totalCount = 0
for line in dbCountsRaw:
    line = line.rstrip().split("\t")
    if line[0].startswith("hub"):
        db = "_".join(line[0].split("_")[2:])
    else:
        db = line[0]
    count = int(line[1])
    totalCount = totalCount + count
    if db not in dbsCounts:
        dbsCounts[db] = count
    else:
        dbsCounts[db] = dbsCounts[db] + count

if totalCount == 0:
    raise RuntimeError("dbCounts.tsv held no usable counts - generateUsageStats.py produced nothing")

for key in dbsCounts:
    dbCountsCombined.write(key+"\t"+str(dbsCounts[key])+"\t"+str(round(dbsCounts[key]/totalCount*100,2))+"\n")

dbCountsCombined.close()
dbCountsRaw.close()

bash('sort '+outputDir+'/dbCountsCombinedWithCuratedHubs.tsv -rnk2 > '+outputDir+'/dbCountsCombinedWithCuratedHubs.tsv.sorted')

genArkTotal = sum(count for db, count in dbsCounts.items() if db in genArkAccessions)

# Every row below is a real database and they add up to 100%. The GenArk line is a roll-up of
# the GenArk rows already in that list, so it is marked rather than counted twice.
dbRows = sorted(dbsCounts.items(), key=lambda item: (-item[1], item[0]))
dbRows = [[db, count, "{:.2f}".format(count/totalCount*100)] for db, count in dbRows]
if genArkTotal:
    dbRows.append(["GenArk", genArkTotal, "{:.2f} (All summed)".format(genArkTotal/totalCount*100)])
    dbRows.sort(key=lambda row: -row[1])

resultsLines.append("")
resultsLines.append("List of db usage, hubs are aggregated across mirrors to a single count:")
resultsLines.append(formatTable(["db", "dbUse", "percentUse"], dbRows[0:10]))

##### Report GenArk assembly usage, ranked ######

genArkRows = sorted(((count, db) for db, count in dbsCounts.items() if db in genArkAccessions),
                    key=lambda item: (-item[0], item[1]))
genArkTable = []
with open(outputDir+'/genArkUsage.tsv','w') as genArkFile:
    for count, db in genArkRows:
        label = genArkLabels.get(db, "")
        genArkFile.write(db+"\t"+str(count)+"\t"+"{:.2f}".format(count/genArkTotal*100 if genArkTotal else 0)+"\t"+label+"\n")
        if len(genArkTable) < 10:
            genArkTable.append([(db+"  "+label).strip(), count, "{:.2f}".format(count/genArkTotal*100 if genArkTotal else 0)])

resultsLines.append("")
resultsLines.append("List of GenArk assembly usage. percentUse is of the GenArk total:")
if genArkTable:
    resultsLines.append(formatTable(["db", "dbUse", "percentUse"], genArkTable))
else:
    resultsLines.append("No GenArk assembly usage found.")

##### Report default track usage for hg38 and hg19 ######

#generateUsageStats.py cannot warn through stderr - this script merges and discards it, and
#the cron discards its own - so it leaves warnings in defaultCounts.tsv for us to surface
with open(outputDir+'/defaultCounts.tsv', encoding="utf-8", errors="replace") as defaultCounts:
    for line in defaultCounts:
        if line.startswith("#WARNING\t"):
            reportWarnings.append("WARNING: "+line.rstrip("\n").split("\t", 1)[1])

def defaultTrackRows(db):
    """defaultCounts.tsv is db, track, use, % using, % turning off - sorted by most turned off"""
    rows = []
    with open(outputDir+'/defaultCounts.tsv', encoding="utf-8", errors="replace") as defaultCounts:
        for line in defaultCounts:
            if line.startswith("#"):
                continue
            splitLine = line.rstrip("\n").split("\t")
            if len(splitLine) < 5 or splitLine[0] != db:
                continue
            if "MarkH3k27ac" in splitLine[1]: #dozens of near identical subtracks, they swamp the list
                continue
            rows.append([splitLine[0], splitLine[2], splitLine[3], splitLine[4], splitLine[1]])
    #Sorted by % turning off, then track name so that ties never reorder between runs
    rows.sort(key=lambda row: (-float(row[3]), row[4]))
    return rows[0:15]

for db in ["hg38", "hg19"]:
    resultsLines.append("")
    resultsLines.append("List of default track usage for "+db+", sorted by how many users are turning off the track:")
    resultsLines.append(formatTable(["db", "trackUse", "% using", "% turning off", "trackName"],
                                    defaultTrackRows(db)))

##### Report non-default track usage ######

nonDefaultRows = []
with open(outputDir+'/trackCounts.tsv.sorted', encoding="utf-8", errors="replace") as trackCountsSorted:
    with open(outputDir+'/trackCounts.tsv.sorted.noDefaults','w', encoding="utf-8") as noDefaults:
        for line in trackCountsSorted:
            splitLine = line.rstrip("\n").split("\t")
            if len(splitLine) < 3 or (splitLine[0], splitLine[1]) in defaultTracks:
                continue
            noDefaults.write(line)
            if len(nonDefaultRows) < 15:
                nonDefaultRows.append([splitLine[0], splitLine[2], splitLine[1]])

resultsLines.append("")
resultsLines.append("List of non-default track usage:")
resultsLines.append(formatTable(["db", "trackUse", "trackName"], nonDefaultRows))

##### Report public hub usage and non-public hub usage ######

bash("sort "+outputDir+"/trackCountsHubs.tsv -rnk4 -t $\'\\t\' > "+outputDir+"/trackCountsHubs.tsv.sorted")
allPubHubs = bashNoErrorCatch('cat '+outputDir+'/trackCountsHubs.tsv.sorted')

#This section pulls out only a single occurence of each public hub, picking the first track (most uses) to represent it
pubHubFile = open(outputDir+"/pubHubs.txt", "w")
results = OrderedDict()
for each in allPubHubs:
    each = each.split('\t')
    if len(each) > 3 and each[0] not in results.keys():
        results[each[0]] = each[0:]

for key, value in results.items():
    pubHubFile.write(value[0]+"\t"+value[1]+"\t"+value[2]+"\t"+value[3]+"\n")
pubHubFile.close()

#Add up each hub's track counts across every mirror it was used on. Two dictionaries are needed:
#trackCountsHubs.tsv carries no hub id, so the public hub section can only be keyed on track
#and database, while the non-public section keys on the resolved hub URL. Keying only on
#track+db lumps unrelated hubs together whenever they share a common track name - it is why
#every one of the 2800 track collection hubs used to report the same number.
allHubCountsByTrack = {}
allHubCountsByUrl = {}
trackCounts = open(outputDir+"/trackCounts.tsv.sorted","r", encoding="utf-8", errors="replace")
for line in trackCounts:
    line = line.rstrip().split("\t")
    if len(line) < 3 or not line[1].startswith("hub_"):
        continue
    database = stripHubPrefix(line[0])
    trackName = stripHubPrefix(line[1])
    nameToMatch = trackName + database
    allHubCountsByTrack[nameToMatch] = allHubCountsByTrack.get(nameToMatch, 0) + int(line[2])

    hubId = hubIdFromName(line[1])
    #A hub id no mirror could vouch for still has to keep its counts, or the totals quietly
    #shrink by whatever the missing mirror was carrying
    hubKeyPart = resolvedHubs[hubId][0] if hubId in resolvedHubs else "UNRESOLVED:"+str(hubId)
    urlKey = (hubKeyPart, trackName, database)
    allHubCountsByUrl[urlKey] = allHubCountsByUrl.get(urlKey, 0) + int(line[2])
trackCounts.close()

#Now use the new list to report the most popular public hub numbers across all mirrors
pubHubList = open(outputDir+"/pubHubs.txt", "r", encoding="utf-8", errors="replace")
pubHubDic = {}
for pubHub in pubHubList:
    pubHub = pubHub.rstrip().split("\t")
    database = stripHubPrefix(pubHub[1])
    name = pubHub[2] + database
    #Make name a combination of track name + assembly
    pubHubDic[name] = {}
    pubHubDic[name]['trackName'] = pubHub[2]
    pubHubDic[name]['dbs'] = database
    pubHubDic[name]['hubName'] = pubHub[0]
    pubHubDic[name]['count'] = allHubCountsByTrack.get(name, int(pubHub[3]))
pubHubList.close()

pubHubFile = open(outputDir+"/allPubHubsCombinedWithMirrorsCounts.txt", "w", encoding="utf-8")
for key in pubHubDic:
    pubHubFile.write(pubHubDic[key]['dbs']+"\t"+str(pubHubDic[key]['count'])+"\t"+pubHubDic[key]['trackName']+"\t"+pubHubDic[key]['hubName']+"\n")
pubHubFile.close()

bash('sort -rnk2 '+outputDir+'/allPubHubsCombinedWithMirrorsCounts.txt > '+outputDir+'/allPubHubsCombinedWithMirrorsCounts.sorted.txt')

resultsLines.append("")
resultsLines.append("List of public hub usage (only most used track represented). Counts added across all mirrors:")
pubHubRows = []
with open(outputDir+'/allPubHubsCombinedWithMirrorsCounts.sorted.txt', encoding="utf-8", errors="replace") as sortedPubHubs:
    for line in sortedPubHubs:
        splitLine = line.rstrip("\n").split("\t")
        if len(splitLine) == 4 and len(pubHubRows) < 15:
            pubHubRows.append(splitLine)
resultsLines.append(formatTable(["db", "trackUse", "track", "pubHub"], pubHubRows))

##### Report hubs that are not public hubs ######

# One entry per hub, carrying its cross-mirror count
hubsByUrl = OrderedDict()
for hubId, best in bestTrackForHub.items():
    if hubId not in resolvedHubs:
        continue
    hubUrl, shortLabel, machine = resolvedHubs[hubId]
    if hubIsPublic(hubId, hubUrl, machine):
        continue
    database = stripHubPrefix(best[1])
    trackName = stripHubPrefix(best[2])
    useCount = allHubCountsByUrl.get((hubUrl, trackName, database), best[0])
    category = hubCategory(hubUrl)

    #hubSpace hubs collapse to the busiest one per user, otherwise a single person uploading a
    #couple of hundred hubs takes over the whole list
    if category == 'hubspace':
        groupKey = "hubspaceUser:" + str(hubSpaceUser(hubUrl))
    else:
        groupKey = normalizeHubUrl(hubUrl)
    existing = hubsByUrl.get(groupKey)
    #Ties are broken on the label so that two runs never disagree about which hub survives
    if existing is None or useCount > existing[1] or (useCount == existing[1] and shortLabel < existing[2]):
        hubsByUrl[groupKey] = [database, useCount, shortLabel, hubUrl, machine, category]

#Summed family rows use the same cross-mirror count as the ranked rows, taken after the
#collapse so that two registrations of one hub are not counted twice
familyTotals = {}
for entry in hubsByUrl.values():
    familyTotals[entry[5]] = familyTotals.get(entry[5], 0) + entry[1]

nonPublicRows = []
for entry in sorted(hubsByUrl.values(), key=lambda row: (-row[1], row[2])):
    #GenArk has its own section, curated hubs are covered by the native database checks, and
    #the two machine generated families are reported as a single summed line each
    if entry[5] in ('genark', 'curated', 'trackCollection', 'encodeSearch'):
        continue
    nonPublicRows.append(entry[0:5])

nonPubHubsFile = open(outputDir+"/allRegularHubsCombinedWithMirrorsCounts.txt", "w", encoding="utf-8")
for row in nonPublicRows:
    nonPubHubsFile.write("\t".join(str(cell) for cell in row)+"\n")
nonPubHubsFile.close()
bash('sort -rnk2 '+outputDir+'/allRegularHubsCombinedWithMirrorsCounts.txt > '+outputDir+'/allRegularHubsCombinedWithMirrorsCounts.sorted.txt')

#Same hubs, the column order this file has always used: db, mirror, count, label, url
hubsNotPublic = open(outputDir+"/hubsNotPublic.txt", "w", encoding="utf-8")
for row in nonPublicRows:
    hubsNotPublic.write(row[0]+"\t"+row[4]+"\t"+str(row[1])+"\t"+row[2]+"\t"+row[3]+"\n")
hubsNotPublic.close()

summedRows = []
if familyTotals.get('trackCollection'):
    summedRows.append(["-", familyTotals['trackCollection'], "Track collections (all summed)",
                       "browser track collection tool", "-"])
if familyTotals.get('encodeSearch'):
    summedRows.append(["-", familyTotals['encodeSearch'], "ENCODE search hubs (all summed)",
                       "encodeproject.org/batch_hub", "-"])

resultsLines.append("")
resultsLines.append("List of hub usage that are not public hubs. Counts are added across all mirrors/machines:")
resultsLines.append(formatTable(["db", "useCount", "shortLabel", "hubUrl", "mirror"],
                                nonPublicRows[0:10] + summedRows))

##### Report hubSpace usage, one row per user ######

hubSpaceRows = []
with open(outputDir+'/hubSpaceUsage.tsv','w', encoding="utf-8") as hubSpaceFile:
    for entry in sorted(hubsByUrl.values(), key=lambda row: (-row[1], row[2])):
        if entry[5] != 'hubspace':
            continue
        userName = hubSpaceUser(entry[3])
        hubSpaceFile.write(str(userName)+"\t"+str(entry[1])+"\t"+entry[0]+"\t"+entry[2]+"\n")
        if len(hubSpaceRows) < 10:
            hubSpaceRows.append([userName, entry[1], entry[0], entry[2]])

resultsLines.append("")
resultsLines.append("List of hubSpace hub usage, one row per user (their most used hub):")
if hubSpaceRows:
    resultsLines.append(formatTable(["user", "useCount", "db", "shortLabel"], hubSpaceRows))
else:
    resultsLines.append("No hubSpace hub usage found.")

##### Footer #####

unresolvedHubIds = [hubId for hubId in hubIdsSeen if hubId not in resolvedHubs]
if unresolvedHubIds:
    reportWarnings.append("Note: "+str(len(unresolvedHubIds))+" of "+str(len(hubIdsSeen))+
                          " hub ids could not be matched to a current hub on any mirror. Their "
                          "usage is counted in the database totals above, but they cannot be "
                          "named, so they do not appear in the hub lists.")

if ambiguousHubIds:
    reportWarnings.append("Note: "+str(len(ambiguousHubIds))+" hub id(s) were registered to "
                          "different hubs on different mirrors and could not be told apart: "+
                          ", ".join(sorted(ambiguousHubIds)[0:10]))

resultsLines.append("")
if reportWarnings:
    resultsLines.extend(reportWarnings)
    resultsLines.append("")
resultsLines.append("Previous outputs of this cron can be found here: https://genecats.gi.ucsc.edu/qa/test-results/usageStats/")
resultsLines.append("Monthly usage counts of all public hubs can be found here: https://genecats.gi.ucsc.edu/qa/test-results/usageStats/publicHubUsageCounts/")
resultsLines.append("Archive of monthly raw data can be found here: /hive/users/qateam/assemblyStatsCronArchive/")

with open(outputDir+'/results.txt','w', encoding="utf-8") as resultsFile:
    resultsFile.write("\n".join(resultsLines)+"\n")

bash("mkdir -p /hive/users/"+user+"/assemblyStatsCronArchive/"+labelMonth)
bash("cp -f "+outputDir+"/* /hive/users/"+user+"/assemblyStatsCronArchive/"+labelMonth+" 2>/dev/null || true")

if user == 'qateam':
    bash("cat "+outputDir+"/results.txt > /usr/local/apache/htdocs-genecats/qa/test-results/usageStats/"+labelMonth)
    publicHubPageHeader = """This page contains the usage count of UCSC Genome Browser public hubs. The numbers represent
individual browsing sessions across all UCSC mirrors for the month of """+labelMonth+""".

assembly\tusageCount\tmostPopularTrack\thubName
"""
    with open("/usr/local/apache/htdocs-genecats/qa/test-results/usageStats/publicHubUsageCounts/pubHubUsageCounts."+labelMonth+".txt",'w') as hubsUsageFile:
        hubsUsageFile.write(publicHubPageHeader)
    bash('cat '+outputDir+'/allPubHubsCombinedWithMirrorsCounts.sorted.txt >> /usr/local/apache/htdocs-genecats/qa/test-results/usageStats/publicHubUsageCounts/pubHubUsageCounts.'+labelMonth+'.txt')

with open(outputDir+'/results.txt', encoding="utf-8") as resultsFile:
    for line in resultsFile:
        print(line.rstrip())

bash("rm -f "+logDir+"/*")
#Leave the asia delivery in place. Removing it here would mean a re-run before the 28th has no
#asia data at all, and would make the staleness check above unreachable.
bash("find "+outputDir+" -maxdepth 1 -type f ! -name genomeAsiaHubStatus.txt -delete")
