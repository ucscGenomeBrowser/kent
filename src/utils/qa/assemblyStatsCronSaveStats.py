
# coding: utf-8

# In[2]:


#Made by Lou - 07/20/19

import datetime
from collections import OrderedDict 
import getpass

user = getpass.getuser()

# Get the year to query proper wwwstats directory
today = datetime.datetime.today()
year = str(today).split('-')[0]

# Get the last 5 error logs from the RR
latestLogs = get_ipython().getoutput(u'ls /hive/users/chmalee/logs/trimmedLogs/result/hgw1')
######################## FOR TESTING JUST ONE LOG
#latestLogs = latestLogs[len(latestLogs)-1:]
#########################
latestLogs = latestLogs[len(latestLogs)-5:]

nodes = ['RR', 'asiaNode', 'euroNode'] #Add nodes with error logs, nodes can be added or removed

machines = ['hgw1','hgw2'] #Add hgw machines to check
for node in nodes:
    if node == 'RR':
        for machine in machines:
            for log in latestLogs: #Copy the 5 latest error logs for each of the rr machines
                get_ipython().system(u" cp /hive/users/chmalee/logs/trimmedLogs/result/'$machine'/'$log' /hive/users/'$user'/ErrorLogs/'$node''$machine''$log'")
    else:
        latestLogs = get_ipython().getoutput(u"ls /hive/users/chmalee/logs/trimmedLogs/result/'$node'")
        latestLogs = latestLogs[len(latestLogs)-5:]
        for log in latestLogs: #Copy the 5 latest error logs for each of the other nodes
            get_ipython().system(u" cp /hive/users/chmalee/logs/trimmedLogs/result/'$node'/'$log' /hive/users/'$user'/ErrorLogs/'$node''$log'")
    
# Run generateUsageStats.py with -d (directory), -t (default track stats), -o (output) 
get_ipython().system(u" /cluster/home/'$user'/kent/src/hg/logCrawl/dbTrackAndSearchUsage/generateUsageStats.py -d /hive/users/'$user'/ErrorLogs --allOutput -t -o /hive/users/'$user'/ErrorLogsOutput > /dev/null")

#The following section pulls out a list of default track for the top X assemblies for filtering
file = open("/hive/users/"+user+"/ErrorLogsOutput/defaults.txt", "a")

#The head command can be expanded to be more inclusive if additional assembly defaults are finding their way onto the list
get_ipython().system(u" sort /hive/users/'$user'/ErrorLogsOutput/dbCounts.tsv -rnk2 > /hive/users/'$user'/ErrorLogsOutput/dbCountsTopSorted.tsv")
dbs = get_ipython().getoutput(u'head -n 4 /hive/users/\'$user\'/ErrorLogsOutput/dbCountsTopSorted.tsv | cut -f1 -d "\t"')

for db in dbs[0:4]:
    #The following part queries hgTracks for each of the assemblies and extracts the list of defaults
    get_ipython().system(u" echo '$db' > /hive/users/'$user'/ErrorLogsOutput/temp.txt")
    defaults = get_ipython().getoutput(u'HGDB_CONF=$HOME/.hg.conf.beta /usr/local/apache/cgi-bin/hgTracks db=$(cat /hive/users/"$USER"/ErrorLogsOutput/temp.txt) > /dev/null')
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

get_ipython().system(u' echo This cronjob pulls out GB stats over the last month, across all RR machines and Asia/Euro mirrors using the generateUsageStats.py script. It only counts each hgsid occurrence once, filtering our any hgsid that only showed up one time. > /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' echo >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u' echo List of db usage: >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u" echo db$'\\t'dbUse >> /hive/users/qateam/ErrorLogsOutput/results.txt")
get_ipython().system(u' echo -------------------------------------------------------------------------------------- >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' sort /hive/users/"$USER"/ErrorLogsOutput/dbCounts.tsv -rnk2 > /hive/users/"$USER"/ErrorLogsOutput/dbCounts.tsv.sorted')
get_ipython().system(u' head -n 15 /hive/users/"$USER"/ErrorLogsOutput/dbCounts.tsv.sorted | ~markd/bin/tabFmt >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' echo >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u' echo "List of default track usage for hg38, sorted by how many users are turning off the track:" >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u" echo db$'\\t'trackUse$'\\t'% using$'\\t'% turning off$'\\t'trackName >> /hive/users/qateam/ErrorLogsOutput/results.txt")
get_ipython().system(u' echo -------------------------------------------------------------------------------------- >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' grep ^hg38 /hive/users/"$USER"/ErrorLogsOutput/defaultCounts.tsv | grep -v "MarkH3k27ac" | sort -nrk 5 | awk -v OFS="\\t" \'{ print $1,$3,$4,$5,$2 }\' | head -n 15 | ~markd/bin/tabFmt >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' echo >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u' echo "List of default track usage for hg19, sorted by how many users are turning off the track:" >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u" echo db$'\\t'trackUse$'\\t'% using$'\\t'% turning off$'\\t'trackName >> /hive/users/qateam/ErrorLogsOutput/results.txt")
get_ipython().system(u' echo -------------------------------------------------------------------------------------- >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' grep ^hg19 /hive/users/"$USER"/ErrorLogsOutput/defaultCounts.tsv | grep -v "MarkH3k27ac" | sort -nrk 5 | awk -v OFS="\\t" \'{ print $1,$3,$4,$5,$2 }\' | head -n 15 | ~markd/bin/tabFmt >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' echo >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u' echo List of non-default track usage: >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u" echo db$'\\t'trackUse$'\\t'trackName >> /hive/users/qateam/ErrorLogsOutput/results.txt")
get_ipython().system(u' echo -------------------------------------------------------------------------------------- >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' sort /hive/users/"$USER"/ErrorLogsOutput/trackCounts.tsv -rnk3 > /hive/users/"$USER"/ErrorLogsOutput/trackCounts.tsv.sorted')
get_ipython().system(u' cat /hive/users/"$USER"/ErrorLogsOutput/trackCounts.tsv.sorted | grep -v -f /hive/users/"$USER"/ErrorLogsOutput/defaults.txt > /hive/users/"$USER"/ErrorLogsOutput/trackCounts.tsv.sorted.noDefaults')
get_ipython().system(u' head -n 15 /hive/users/"$USER"/ErrorLogsOutput/trackCounts.tsv.sorted.noDefaults | awk -v OFS="\\t" \'{ print $1,$3,$2 }\' | ~markd/bin/tabFmt >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' echo >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u' echo "List of public hub usage (only most used track represented):" >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u" echo db$'\\t'trackUse$'\\t'track$'\\t'pubHub >> /hive/users/qateam/ErrorLogsOutput/results.txt")
get_ipython().system(u' echo -------------------------------------------------------------------------------------- >> /hive/users/qateam/ErrorLogsOutput/results.txt')

file2 = open("/hive/users/"+user+"/ErrorLogsOutput/filteredHubs.txt", "a") #Using a file to be able to use the same ~markd/bin/tabFmt format
get_ipython().system(u' sort /hive/users/"$USER"/ErrorLogsOutput/trackCountsHubs.tsv -rnk4 -t $\'\\t\' > /hive/users/"$USER"/ErrorLogsOutput/trackCountsHubs.tsv.sorted')
topHubs = get_ipython().getoutput(u'head -n 5000 /hive/users/"$USER"/ErrorLogsOutput/trackCountsHubs.tsv.sorted ')
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

get_ipython().system(u' cat /hive/users/"$USER"/ErrorLogsOutput/filteredHubs.txt | awk -F "\\t" -v OFS="\\t" \'{ print $2,$4,$3,$1 }\' | ~markd/bin/tabFmt >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' echo >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u' echo List of public hub track usage overall: >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u" echo db$'\\t'trackUse$'\\t'track$'\\t'pubHub >> /hive/users/qateam/ErrorLogsOutput/results.txt")
get_ipython().system(u' echo -------------------------------------------------------------------------------------- >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' head /hive/users/"$USER"/ErrorLogsOutput/trackCountsHubs.tsv.sorted | awk -F "\\t" -v OFS="\\t" \'{ print $2,$4,$3,$1 }\' | ~markd/bin/tabFmt >> /hive/users/qateam/ErrorLogsOutput/results.txt')

#Query hubPublic and hubStats in order to filter out public hubs then sort out the IDs
get_ipython().system(u' /cluster/bin/x86_64/hgsql -h genome-centdb -e "select hubUrl from hubPublic" hgcentral > /hive/users/"$USER"/ErrorLogsOutput/hubPublicHubUrl.txt')
get_ipython().system(u' /cluster/bin/x86_64/hgsql -h genome-centdb -e "select hubUrl,id from hubStatus" hgcentral> /hive/users/"$USER"/ErrorLogsOutput/hubStatusHubUrl.txt')
get_ipython().system(u' grep -f /hive/users/"$USER"/ErrorLogsOutput/hubPublicHubUrl.txt /hive/users/"$USER"/ErrorLogsOutput/hubStatusHubUrl.txt | cut -f2 > /hive/users/"$USER"/ErrorLogsOutput/publicIDs.txt')

#Add hub_ID format to match stats program output, then grep out the public hubs from the track list
get_ipython().system(u' cat /hive/users/"$USER"/ErrorLogsOutput/publicIDs.txt | sed s/^/hub_/g > /hive/users/"$USER"/ErrorLogsOutput/hubPublicIDs.txt')
get_ipython().system(u' grep "hub_" /hive/users/"$USER"/ErrorLogsOutput/trackCounts.tsv | grep -vf /hive/users/"$USER"/ErrorLogsOutput/hubPublicIDs.txt | sort -rnk3 > /hive/users/"$USER"/ErrorLogsOutput/nonPublicHubTracksPopularity.txt')

#Pull out top 100 hubs track (some are repeated) in order to ultimately filter out just 5
get_ipython().system(u' head -n 100 /hive/users/"$USER"/ErrorLogsOutput/nonPublicHubTracksPopularity.txt > /hive/users/"$USER"/ErrorLogsOutput/nonPublicHubTracksPopularityTop1000.txt')

#Pull out whole fields from euro and RR hubStatus in order to collect the info for matching IDs
get_ipython().system(u' ssh qateam@genome-euro "hgsql -e \'select id,hubUrl,shortLabel,lastOkTime from hubStatus\' hgcentral" > /hive/users/"$USER"/ErrorLogsOutput/genomeEuroHubStatus.txt')
get_ipython().system(u' /cluster/bin/x86_64/hgsql -h genome-centdb -e "select id,hubUrl,shortLabel,lastOkTime from hubStatus where lastOkTime !=\'\'" hgcentral > /hive/users/"$USER"/ErrorLogsOutput/RRHubStatus.txt')

hubs = get_ipython().getoutput(u'cat /hive/users/"$USER"/ErrorLogsOutput/nonPublicHubTracksPopularityTop1000.txt')
lastMonth = today - datetime.timedelta(days=30)
lastMonthFormat = lastMonth.strftime('%Y-%m')
hubsDic = OrderedDict()

for hub in hubs: #Start iterating through top 100 hub track lines
    #Pull out the use number, associated db, and hubkey
    hub = hub.split("\t")
    hubKey = hub[1].split("_")[1]
    hubCount = hub[2]
    hubDb = hub[0]
    if hubKey not in hubsDic.keys(): #Check that this key has not yet been counted
        entryMade = False #Reset entryMade variable, made to check RR and then Euro if RR has no match
        # Grep out the proper hubstatus line, returns an empty list if no matches
        rrHub = get_ipython().getoutput(u"grep ^'$hubKey'$'\\t' /hive/users/'$user'/ErrorLogsOutput/RRHubStatus.txt")
        if rrHub != []:
            #Pull out matching hubURL, ShortLabel, lastOKtime (used to see compare against)
            #recurring hubIDs between RR and euro
            rrHub = rrHub[0].split("\t")
            rrHubURL = rrHub[1]
            rrHubShortLabel = rrHub[2]
            rrHubLastOk = rrHub[3]
            rrHubLastOk = datetime.datetime.strptime(rrHubLastOk.split(" ")[0], '%Y-%m-%d')
            if rrHubLastOk > lastMonth: #If  hub has been OK in last month, assume it's correct
                #Pull out all relevant info and save to dictionary
                hubsDic[hubKey] = {}
                hubsDic[hubKey]['shortLabel'] = rrHubShortLabel
                hubsDic[hubKey]['hubURL'] = rrHubURL
                hubsDic[hubKey]['hubCount'] = hubCount
                hubsDic[hubKey]['hubDb'] = hubDb
                hubsDic[hubKey]['machine'] = "RR" 
                entryMade = True #Set true so that the hubID isn't searched for in Euro
        if entryMade is False: #This assumes that the hubID was either not present in the RR hubStatus, or it pointed to a hub not used in last month
            euroHub = get_ipython().getoutput(u"grep ^'$hubKey'$'\\t' /hive/users/'$user'/ErrorLogsOutput/genomeEuroHubStatus.txt")
            if euroHub != []:
                euroHub = euroHub[0].split("\t")
                euroHubURL = euroHub[1]
                euroHubShortLabel = euroHub[2]
                euroHubLastOk = euroHub[3]
                euroHubLastOk = datetime.datetime.strptime(euroHubLastOk.split(" ")[0], '%Y-%m-%d')
                if euroHubLastOk > lastMonth:
                    hubsDic[hubKey] = {}
                    hubsDic[hubKey]['shortLabel'] = euroHubShortLabel
                    hubsDic[hubKey]['hubURL'] = euroHubURL
                    hubsDic[hubKey]['hubCount'] = hubCount
                    hubsDic[hubKey]['hubDb'] = hubDb
                    hubsDic[hubKey]['machine'] = "Euro"
                    entryMade = True

#Create new file to write out the top 10 most used hubs
hubsNotPublic = open("/hive/users/"+user+"/ErrorLogsOutput/hubsNotPublic.txt", "a")
n=-1
for key in hubsDic.keys():
    n+=1
    if n < 15:
        hubsNotPublic.write(hubsDic[key]['hubDb']+"\t"+hubsDic[key]['machine']+"\t"+str(hubsDic[key]['hubCount'])+"\t"+hubsDic[key]['shortLabel']+"\t"+hubsDic[key]['hubURL']+"\n")
hubsNotPublic.close()

get_ipython().system(u' echo >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u' echo "List of hub uses that are Not public hubs. Some public hubs sneak in due to alternative mirror or hubUrl:" >> /hive/users/qateam/ErrorLogsOutput/results.txt')
get_ipython().system(u" echo db$'\\t'machine$'\\t'trackUse$'\\t'shortLabel$'\\t'hubUrl >> /hive/users/qateam/ErrorLogsOutput/results.txt")
get_ipython().system(u' echo -------------------------------------------------------------------------------------- >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u' cat /hive/users/"$USER"/ErrorLogsOutput/hubsNotPublic.txt | ~markd/bin/tabFmt >> /hive/users/qateam/ErrorLogsOutput/results.txt')

get_ipython().system(u" cat /hive/users/qateam/ErrorLogsOutput/results.txt > /usr/local/apache/htdocs-genecats/qa/test-results/usageStats/'$lastMonthFormat'")

get_ipython().system(u" rm /hive/users/'$user'/ErrorLogs/*")
get_ipython().system(u" rm /hive/users/'$user'/ErrorLogsOutput/*")

