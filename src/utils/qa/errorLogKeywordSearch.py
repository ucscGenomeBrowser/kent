#Looks through the error logs and graphs out the occurence of various keywords

import matplotlib
#Don't try to display the plot
matplotlib.use('Agg')

import datetime
from collections import OrderedDict
import getpass
import subprocess
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import matplotlib.dates as mdates

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

def copyLatestLogs():
    user = getpass.getuser()

    # Get the year to query proper wwwstats directory
    today = datetime.datetime.today()
    year = str(today).split('-')[0]

    # Get latest error logs from the RR
    latestLogs = bash('ls /hive/data/inside/wwwstats/RR/2024/hgw1').rstrip().split("\n")
    latestLogs = latestLogs[len(latestLogs)-11:len(latestLogs)-1]

    nodes = ['RR', 'asiaNode', 'euroNode'] #Add nodes with error logs, nodes can be added or removed
    machines = ['hgw1','hgw2'] #Add hgw machines to check

    for node in nodes:
        if node == 'RR':
            for machine in machines:
                for log in latestLogs: 
                    bash("ln -sf /hive/data/inside/wwwstats/RR/"+year+"/"+machine+"/"+log+' /hive/users/'+user+'/ErrorLogs/'+node+machine+log)
        else:
            for log in latestLogs:
                bash("ln -sf /hive/data/inside/wwwstats/"+node+"/"+year+"/"+log+' /hive/users/'+user+'/ErrorLogs/'+node+log)
    return(user,latestLogs)

def createDicOfSearchTerms():
    totalLinesInLog = dict(label='Total lines in logs', description='Total number of lines seen in the logs', value=[], searchKeyWord="wc -l")
    totalUniqueIPs = dict(label='Total unique IPs', description='Total number of unique IPs without port number, e.g. N.N.N and not N.N.N:NNN', value=[], searchKeyWord='grep "\[client" | cut -f4 -d "]" | cut -f3 -d " " | cut -f1 -d ":" | sort | uniq | wc -l')
    totalUniqueIPsSubnets = dict(label='Total unique IP subnets', description='Total number of unique IPs with only partial subnet, e.g. NNN.NNN and not NNN.NNN.N.NN', value=[], searchKeyWord='grep "\[client" | cut -f4 -d "]" | cut -f3 -d " " | cut -f1 -d ":" | cut -f1-2 -d "." | sort | uniq | wc -l')
    totalUniqueHgsids = dict(label='Total unique hgsIDs', description='Total number of unique hgsIDs', value=[], searchKeyWord=r"grep 'hgsid' | sed -n 's/.*[?&]hgsid=\([0-9A-Za-z_]*\).*/\1/p' | sort | uniq | wc -l")
    totalLoadedSessions = dict(label='Total loaded sessions', description='Total number of loaded sessions', value=[], searchKeyWord=' grep "CGI_TIME: hgTracks" | grep "/cgi-bin/hgSession?" | wc -l')
    totalSavedCTs = dict(label='Total saved CTs', description='Total number of saved custom tracks', value=[], searchKeyWord='grep "customTrack: saved" | wc -l')
    totalCTerrors = dict(label='Total CT errors', description='Total number of custom track load errors', value=[], searchKeyWord='grep "hgCustom load error" | wc -l')
    totalStackDumps = dict(label='Total stack dumps', description='Total number of stack dumps', value=[], searchKeyWord='grep "Stack dump" | wc -l')
    totalTryingToAllocate = dict(label='Total 500Mb allocate memory', description="Happens if code tries to allocate a chunk bigger than hard-wired limit of 500m. Could indicate naughty CGI", value=[], searchKeyWord='grep "needMem: trying to allocate" | wc -l')
    totalOutOfMemory = dict(label='Total out of memory', description='Happens if malloc() fails because the OS native limits (or hg.conf maxMem limits)', value=[], searchKeyWord='grep "needMem: Out of memory" | wc -l')
    totalHogExits = dict(label='hogExit', description='hogExit: Total number of people that hit the bottleneck', value=[], searchKeyWord='grep "hogExit" | wc -l')
    totalHgCollectionsExpire = dict(label='hgCollections', description='Total number of expired hgCollections', value=[], searchKeyWord='grep "Track Collections expire 48" | wc -l')
    totalWarnTimings = dict(label='warnTiming', description='warnTiming: Number of people that hit the warnSeconds hg.conf var. Warns them about image taking too long to load', value=[], searchKeyWord='grep "warnTiming" | wc -l')

    itemsToFind = [totalLinesInLog,totalUniqueIPs,totalUniqueIPsSubnets,totalUniqueHgsids,totalLoadedSessions,totalSavedCTs,totalCTerrors,totalHgCollectionsExpire,totalHogExits,totalStackDumps,totalTryingToAllocate,totalOutOfMemory,totalWarnTimings]
    return(itemsToFind)

def searchForTermsInLogs():
    user,latestLogs = copyLatestLogs()
    itemsToFind = createDicOfSearchTerms()

    # n=0 Uncomment these lines to see progress
    for log in latestLogs:
#         n+=1
        logPath = "zcat /hive/users/"+user+"/ErrorLogs/*"+log+" | "
        for searchTerm in itemsToFind:
            searchTerm['value'].append(int(bash(logPath+searchTerm['searchKeyWord'])))
#         print("Current progress:", n/len(latestLogs))

    bash("rm /hive/users/"+user+"/ErrorLogs/*")
    return(user,latestLogs,itemsToFind)

def generateGraphs(user,latestLogs,itemsToFind):
    logDates = [log.split(".")[1] for log in latestLogs]
    dateRange = str(logDates[0])+"-"+str(logDates[len(logDates)-1])
    saveDir = "/hive/users/"+user+"/errorLogSearchCronResults/"+dateRange
    bash("mkdir -p "+saveDir)
    htmlPageOutput = open(saveDir+"/index.html",'w')

    n=0
    for report in itemsToFind:
        n+=1
        # x axis values
        x_dates = [datetime.datetime.strptime(date, "%Y%m%d") for date in logDates]
        # corresponding y axis values
        y = report['value']

        # plotting the points 
        plt.plot(x_dates, y, marker='o')

        # Format the x-axis to show dates, with one point per week
        plt.gca().xaxis.set_major_locator(mdates.WeekdayLocator())  # Major ticks: weekly
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%Y%m%d'))  # Format as "YYYYMMDD"

        # Rotate date labels for better readability
        plt.gcf().autofmt_xdate()

        # naming the x axis
        plt.xlabel('Error log week yearMonthDay')
        # naming the y axis
        plt.ylabel(report['label'])
        plt.xticks(x_dates)
        # giving a title to my graph
        plt.title(report['label'])

        # Add a caption
        plt.text(0.5, -0.35, report['description']+" \nsearch term: "+report['searchKeyWord'], ha='center', va='center', fontsize=10, transform=plt.gca().transAxes)

        # Ensure the figure is fully rendered before saving
        plt.gcf().canvas.draw()  # Force rendering of the canvas

        # Save the plot to a file
        plt.savefig(saveDir + "/" + str(n) + ".png", bbox_inches='tight')
        htmlPageOutput.write('<img src="'+str(n) + '.png">')

        # Clear the current plot to avoid overlaps with the next plot
        plt.clf()

    htmlPageOutput.close()

    if user == 'qateam':
        bash("mkdir -p /usr/local/apache/htdocs-genecats/qa/test-results/errorLogSearchResults/"+dateRange)
        bash("ln -sf "+saveDir+"/* /usr/local/apache/htdocs-genecats/qa/test-results/errorLogSearchResults/"+dateRange+"/")
        print("See the latest error log search results over the last 10 weeks:\n")
        print("https://genecats.gi.ucsc.edu/qa/test-results/errorLogSearchResults/")
    else:
        bash("mkdir -p /cluster/home/"+user+"/public_html/cronResults/errorLogSearchResults/"+dateRange)
        bash("ln -sf "+saveDir+"/* /cluster/home/"+user+"/public_html/cronResults/errorLogSearchResults/"+dateRange+"/")
        print("See the latest error log search results over the last 10 weeks:\n")
        print("https://hgwdev.gi.ucsc.edu/~"+user+"/cronResults/errorLogSearchResults/"+dateRange+"/")    
    
def main():
    user,latestLogs,itemsToFind = searchForTermsInLogs()
    generateGraphs(user,latestLogs,itemsToFind)

main()
