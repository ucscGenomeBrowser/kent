#07/20/19
#This was adapted from a jupyter notebook - hence lots of weird bash calls

import datetime
from collections import OrderedDict
import getpass
import subprocess

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

def bashNoErrorCatch(cmd):
    """Run the cmd in bash subprocess, don't catch error since grep returns exit code 1 when no match is found"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout.rstrip().split("\n")
    except:
        bashStdoutt = []
    return(bashStdoutt)

user = getpass.getuser()

# Get the year to query proper wwwstats directory
today = datetime.datetime.today()
year = str(today).split('-')[0]

# Get the last 5 error logs from the RR
latestLogs = bash('ls /hive/users/chmalee/logs/trimmedLogs/result/hgw1').rstrip().split("\n")
######################## FOR TESTING JUST ONE LOG
# latestLogs = latestLogs[len(latestLogs)-1:]
#########################
latestLogs = latestLogs[len(latestLogs)-5:]

nodes = ['RR', 'asiaNode', 'euroNode'] #Add nodes with error logs, nodes can be added or removed

machines = ['hgw1','hgw2'] #Add hgw machines to check
for node in nodes:
    if node == 'RR':
        for machine in machines:
            for log in latestLogs: #Copy the 5 latest error logs for each of the rr machines
                bash("cp /hive/users/chmalee/logs/trimmedLogs/result/"+machine+'/'+log+' /hive/users/'+user+'/ErrorLogs/'+node+machine+log)

    else:
        latestLogs = bash("ls /hive/users/chmalee/logs/trimmedLogs/result/"+node).rstrip().split("\n")
        latestLogs = latestLogs[len(latestLogs)-5:]
        for log in latestLogs: #Copy the 5 latest error logs for each of the other nodes
            bash('cp /hive/users/chmalee/logs/trimmedLogs/result/'+node+'/'+log+' /hive/users/'+user+'/ErrorLogs/'+node+log)

# Run generateUsageStats.py with -d (directory), -t (default track stats), -o (output)
bash("/cluster/home/"+user+'/kent/src/hg/logCrawl/dbTrackAndSearchUsage/generateUsageStats.py -d /hive/users/'+user+'/ErrorLogs --allOutput -t -o /hive/users/'+user+'/ErrorLogsOutput > /dev/null')

#The following section pulls out a list of default track for the top X assemblies for filtering
file = open("/hive/users/"+user+"/ErrorLogsOutput/defaults.txt", "a")

#The head command can be expanded to be more inclusive if additional assembly defaults are finding their way onto the list
bash("sort /hive/users/"+user+'/ErrorLogsOutput/dbCounts.tsv -rnk2 > /hive/users/'+user+'/ErrorLogsOutput/dbCountsTopSorted.tsv')
dbs = bash('head -n 4 /hive/users/'+user+'/ErrorLogsOutput/dbCountsTopSorted.tsv | cut -f1 -d "\t"').rstrip().split("\n")


for db in dbs[0:4]:
    #The following part queries hgTracks for each of the assemblies and extracts the list of defaults
    bash('echo '+db+' > /hive/users/'+user+'/ErrorLogsOutput/temp.txt')
    defaults = bash('HGDB_CONF=$HOME/.hg.conf.beta /usr/local/apache/cgi-bin/hgTracks db='+db+' hgt.trackImgOnly=1 > /dev/null').rstrip().split("\n")

    for trackLogs in range(len(defaults)):
        if trackLogs >= (len(defaults)-1):
            pass
        else:
            defaultsLine = defaults[trackLogs].split(" ")[4]
            defaultsLine = defaultsLine.split(',')
            for track in range(len(defaultsLine)):
                defaultsLine[track] = defaultsLine[track][0:(len(defaultsLine[track])-2)]
                file.write(db+"\t"+defaultsLine[track]+"\n")
    file.write(db+"\t"+"cytoBand"+"\n")
file.close()

bash('echo This cronjob pulls out GB stats over the last month, across all RR machines and Asia/Euro mirrors using the generateUsageStats.py script. It only counts each hgsid occurrence once, filtering our any hgsid that only showed up one time. > /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('echo >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash('echo List of db usage: >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash("echo db$'\\t'dbUse >> /hive/users/"+user+"/ErrorLogsOutput/results.txt")
bash('echo -------------------------------------------------------------------------------------- >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('sort /hive/users/'+user+'/ErrorLogsOutput/dbCounts.tsv -rnk2 > /hive/users/'+user+'/ErrorLogsOutput/dbCounts.tsv.sorted')
bash('head -n 15 /hive/users/'+user+'/ErrorLogsOutput/dbCounts.tsv.sorted | ~markd/bin/tabFmt >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('echo >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash('echo "List of default track usage for hg38, sorted by how many users are turning off the track:" >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash("echo db$'\\t'trackUse$'\\t'% using$'\\t'% turning off$'\\t'trackName >> /hive/users/"+user+"/ErrorLogsOutput/results.txt")
bash('echo -------------------------------------------------------------------------------------- >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('grep ^hg38 /hive/users/'+user+'/ErrorLogsOutput/defaultCounts.tsv | grep -v "MarkH3k27ac" | sort -nrk 5 | awk -v OFS="\\t" \'{ print $1,$3,$4,$5,$2 }\' | head -n 15 | ~markd/bin/tabFmt >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('echo >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash('echo "List of default track usage for hg19, sorted by how many users are turning off the track:" >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash("echo db$'\\t'trackUse$'\\t'% using$'\\t'% turning off$'\\t'trackName >> /hive/users/"+user+"/ErrorLogsOutput/results.txt")
bash('echo -------------------------------------------------------------------------------------- >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('grep ^hg19 /hive/users/'+user+'/ErrorLogsOutput/defaultCounts.tsv | grep -v "MarkH3k27ac" | sort -nrk 5 | awk -v OFS="\\t" \'{ print $1,$3,$4,$5,$2 }\' | head -n 15| ~markd/bin/tabFmt >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('echo >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash('echo List of non-default track usage: >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash("echo db$'\\t'trackUse$'\\t'trackName >> /hive/users/"+user+"/ErrorLogsOutput/results.txt")
bash('echo -------------------------------------------------------------------------------------- >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('sort /hive/users/'+user+'/ErrorLogsOutput/trackCounts.tsv -rnk3 > /hive/users/'+user+'/ErrorLogsOutput/trackCounts.tsv.sorted')
bash('cat /hive/users/'+user+'/ErrorLogsOutput/trackCounts.tsv.sorted | grep -v -f /hive/users/'+user+'/ErrorLogsOutput/defaults.txt > /hive/users/'+user+'/ErrorLogsOutput/trackCounts.tsv.sorted.noDefaults')
bash('head -n 15 /hive/users/'+user+'/ErrorLogsOutput/trackCounts.tsv.sorted.noDefaults | awk -v OFS="\\t" \'{ print $1,$3,$2 }\' | ~markd/bin/tabFmt >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('echo >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash('echo "List of public hub usage (only most used track represented):" >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash("echo db$'\\t'trackUse$'\\t'track$'\\t'pubHub >> /hive/users/"+user+"/ErrorLogsOutput/results.txt")
bash('echo -------------------------------------------------------------------------------------- >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

file2 = open("/hive/users/"+user+"/ErrorLogsOutput/filteredHubs.txt", "a") #Using a file to be able to use the same ~markd/bin/tabFmt format
bash("sort /hive/users/"+user+"/ErrorLogsOutput/trackCountsHubs.tsv -rnk4 -t $\'\\t\' > /hive/users/"+user+"/ErrorLogsOutput/trackCountsHubs.tsv.sorted")
topHubs = bash('head -n 15000 /hive/users/'+user+'/ErrorLogsOutput/trackCountsHubs.tsv.sorted').rstrip().split("\n")
#This section pulls out only a single occurence of each public hub, picking the first track (most uses) to represent it
results = OrderedDict()
for each in topHubs:
    if len(results.keys()) < 15:
        each = each.split('\t')
        if each[0] not in results.keys():
            results[each[0]] = each[0:]
    else:
        pass
for key, value in results.items():
    file2.write(value[0]+"\t"+value[1]+"\t"+value[2]+"\t"+value[3]+"\n")
file2.close()

bash('cat /hive/users/'+user+'/ErrorLogsOutput/filteredHubs.txt | awk -F "\\t" -v OFS="\\t" \'{ print $2,$4,$3,$1 }\' | ~markd/bin/tabFmt >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('echo >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash('echo List of public hub track usage overall: >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash("echo db$'\\t'trackUse$'\\t'track$'\\t'pubHub >> /hive/users/"+user+"/ErrorLogsOutput/results.txt")
bash('echo -------------------------------------------------------------------------------------- >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('head -n 15 /hive/users/'+user+'/ErrorLogsOutput/trackCountsHubs.tsv.sorted | awk -F "\\t" -v OFS="\\t" \'{ print $2,$4,$3,$1 }\' | ~markd/bin/tabFmt >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

#Query hubPublic and hubStats in order to filter out public hubs then sort out the IDs
bash('/cluster/bin/x86_64/hgsql -h genome-centdb -e "select hubUrl from hubPublic" hgcentral > /hive/users/'+user+'/ErrorLogsOutput/hubPublicHubUrl.txt')
bash('/cluster/bin/x86_64/hgsql -h genome-centdb -e "select hubUrl,id from hubStatus" hgcentral> /hive/users/'+user+'/ErrorLogsOutput/hubStatusHubUrl.txt')
bash('grep -f /hive/users/'+user+'/ErrorLogsOutput/hubPublicHubUrl.txt /hive/users/'+user+'/ErrorLogsOutput/hubStatusHubUrl.txt | cut -f2 > /hive/users/'+user+'/ErrorLogsOutput/publicIDs.txt')

#Add hub_ID format to match stats program output, then grep out the public hubs from the track list
bash('cat /hive/users/'+user+'/ErrorLogsOutput/publicIDs.txt | sed s/^/hub_/g > /hive/users/'+user+'/ErrorLogsOutput/hubPublicIDs.txt')
bash('grep "hub_" /hive/users/'+user+'/ErrorLogsOutput/trackCounts.tsv | sort -rnk3 > /hive/users/'+user+'/ErrorLogsOutput/allTracksOrderedUsage.txt')

#Pull out whole fields from euro and RR hubStatus in order to collect the info for matching IDs
bash('ssh qateam@genome-euro "hgsql -e \'select id,hubUrl,shortLabel,lastOkTime from hubStatus\' hgcentral" > /hive/users/'+user+'/ErrorLogsOutput/genomeEuroHubStatus.txt')
bash('/cluster/bin/x86_64/hgsql -h genome-centdb -e "select id,hubUrl,shortLabel,lastOkTime from hubStatus where lastOkTime !=\'\'" hgcentral > /hive/users/'+user+'/ErrorLogsOutput/RRHubStatus.txt')
#The genome-asia hubStatus is automatically generated via cron by qateam on asia and copied to dev. Use line below to create a new file from personal user
if user != 'qateam':
    bash("ssh "+user+"@genome-asia \"hgsql -e 'select id,hubUrl,shortLabel,lastOkTime from hubStatus' hgcentral\" > /hive/users/"+user+"/ErrorLogsOutput/genomeAsiaHubStatus.txt")

hubs = bash('cat /hive/users/'+user+'/ErrorLogsOutput/allTracksOrderedUsage.txt').rstrip().split("\n")
pubHubUrls = bash('cat /hive/users/'+user+'/ErrorLogsOutput/hubPublicHubUrl.txt').rstrip().split("\n")
lastMonth = today - datetime.timedelta(days=30)
lastMonthFormat = lastMonth.strftime('%Y-%m')
hubsDic = OrderedDict()

for hub in hubs: #Start iterating through track lines, stop pulling items at 20
    if len(hubsDic.keys()) < 20:
        #Pull out the use number, associated db, and hubkey
        hub = hub.split("\t")
        if 'hub' in hub[1]:
            hubKey = hub[1].split("_")[1]
        elif 'hub' in hub[0]:
            hubKey = hub[0].split("_")[1]
        else:
            print(hub)
        
        hubCount = hub[2]
        hubDb = hub[0]
        if hubKey not in hubsDic.keys(): #Check that this key has not yet been counted
            entryMade = False #Reset entryMade variable, made to check RR and then Euro if RR has no match
            # Grep out the proper hubstatus line, returns an empty list if no matches
            #        ^1017$'\t'
            rrHub = bashNoErrorCatch("grep ^"+hubKey+"$'\\t' /hive/users/"+user+"/ErrorLogsOutput/RRHubStatus.txt")

            if rrHub != []:
                #Pull out matching hubURL, ShortLabel, lastOKtime (used to see compare against)
                #recurring hubIDs between RR and euro
                rrHub = rrHub[0].split("\t")
                if len(rrHub) > 2:
                    rrHubURL = rrHub[1]
                    rrHubShortLabel = rrHub[2]
                    rrHubLastOk = rrHub[3]
                    if rrHubLastOk != '':
                        rrHubLastOk = datetime.datetime.strptime(rrHubLastOk.split(" ")[0], '%Y-%m-%d')
                        if rrHubLastOk > lastMonth: #If  hub has been OK in last month, assume it's correct
                            if rrHubURL in pubHubUrls:
                                entryMade = True
                            else: #Pull out all relevant info and save to dictionary
                                hubsDic[hubKey] = {}
                                hubsDic[hubKey]['shortLabel'] = rrHubShortLabel
                                hubsDic[hubKey]['hubURL'] = rrHubURL
                                hubsDic[hubKey]['hubCount'] = hubCount
                                hubsDic[hubKey]['hubDb'] = hubDb
                                hubsDic[hubKey]['machine'] = "RR"
                                entryMade = True #Set true so that the hubID isn't searched for in Euro
                        
            if entryMade is False: #This assumes that the hubID was either not present in the RR hubStatus, or it pointed to a hub not used in last month
                euroHub = bashNoErrorCatch("grep ^"+hubKey+"$'\\t' /hive/users/"+user+"/ErrorLogsOutput/genomeEuroHubStatus.txt")
                if euroHub != []:
                    euroHub = euroHub[0].split("\t")
                    if len(euroHub) > 2:
                        euroHubURL = euroHub[1]
                        euroHubShortLabel = euroHub[2]
                        euroHubLastOk = euroHub[3]
                        if euroHubLastOk != '':
                            euroHubLastOk = datetime.datetime.strptime(euroHubLastOk.split(" ")[0], '%Y-%m-%d')
                            if euroHubLastOk > lastMonth:
                                if euroHub[1] in pubHubUrls:
                                    entryMade = True
                                else:
                                    hubsDic[hubKey] = {}
                                    hubsDic[hubKey]['shortLabel'] = euroHubShortLabel
                                    hubsDic[hubKey]['hubURL'] = euroHubURL
                                    hubsDic[hubKey]['hubCount'] = hubCount
                                    hubsDic[hubKey]['hubDb'] = hubDb
                                    hubsDic[hubKey]['machine'] = "Euro"
                                    entryMade = True
                            
            if entryMade is False: #This assumes that the hubID was either not present in the RR hubStatus, or it pointed to a hub not used in last month
                asiaHub = bashNoErrorCatch("grep ^"+hubKey+"$'\\t' /hive/users/"+user+"/ErrorLogsOutput/genomeAsiaHubStatus.txt")
                if asiaHub != []:
                    asiaHub = asiaHub[0].split("\t")
                    if len(asiaHub) > 2:
                        asiaHubURL = asiaHub[1]
                        asiaHubShortLabel = asiaHub[2]
                        asiaHubLastOk = asiaHub[3]
                        if asiaHubLastOk != '':
                            asiaHubLastOk = datetime.datetime.strptime(asiaHubLastOk.split(" ")[0], '%Y-%m-%d')
                            if asiaHubLastOk > lastMonth:
                                if asiaHub[1] in pubHubUrls:
                                    pass
                                else:
                                    hubsDic[hubKey] = {}
                                    hubsDic[hubKey]['shortLabel'] = asiaHubShortLabel
                                    hubsDic[hubKey]['hubURL'] = asiaHubURL
                                    hubsDic[hubKey]['hubCount'] = hubCount
                                    hubsDic[hubKey]['hubDb'] = hubDb
                                    hubsDic[hubKey]['machine'] = "Asia"
                                    entryMade = True
                    else:
                        bash("echo The following hub: "+hub+" was not found in the RR/euro/asia hubStatus with a lastOkTime within the last month. This likely means an error has occurred. >> /hive/users/"+user+"/ErrorLogsOutput/results.txt")

#Create new file to write out the top 20 most used hubs
hubsNotPublic = open("/hive/users/"+user+"/ErrorLogsOutput/hubsNotPublic.txt", "a")
for key in hubsDic.keys():
    hubsNotPublic.write(hubsDic[key]['hubDb']+"\t"+hubsDic[key]['machine']+"\t"+str(hubsDic[key]['hubCount'])+"\t"+hubsDic[key]['shortLabel']+"\t"+hubsDic[key]['hubURL']+"\n")
hubsNotPublic.close()

bash('echo >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash('echo "List of hub uses that are not public hubs. Some public hubs sneak in due to alternative hubUrl. This includes curated hubs:" >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash("echo db$'\\t'machine$'\\t'trackUse$'\\t'shortLabel$'\\t'hubUrl >> /hive/users/"+user+"/ErrorLogsOutput/results.txt")
bash('echo -------------------------------------------------------------------------------------- >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('cat /hive/users/'+user+'/ErrorLogsOutput/hubsNotPublic.txt | ~markd/bin/tabFmt >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash('echo >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash('echo >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash('echo Previous outputs of this cron can be found here: https://genecats.gi.ucsc.edu/qa/test-results/usageStats/ >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')
bash('echo Archive of monthly raw data can be found here: /hive/users/qateam/assemblyStatsCronArchive/ >> /hive/users/'+user+'/ErrorLogsOutput/results.txt')

bash("mkdir -p /hive/users/"+user+"/assemblyStatsCronArchive/"+lastMonthFormat)
bash("cp /hive/users/"+user+"/ErrorLogsOutput/* /hive/users/"+user+"/assemblyStatsCronArchive/"+lastMonthFormat)

if user == 'qateam':
    bash("cat /hive/users/"+user+"/ErrorLogsOutput/results.txt > /usr/local/apache/htdocs-genecats/qa/test-results/usageStats/"+lastMonthFormat)

resultsFile = open('/hive/users/'+user+'/ErrorLogsOutput/results.txt','r')
for line in resultsFile:
    print(line.rstrip())
resultsFile.close()

bash("rm /hive/users/"+user+"/ErrorLogs/*")
bash("rm /hive/users/"+user+"/ErrorLogsOutput/*")
