#Checks that the Asia, Euro and hgw1/2 logs in the trimmed logs directory
#are no more than a month old. This ensures our crons from primarily Asia/Euro
#are still running

import datetime, subprocess

def bash(cmd):
    """Input bash cmd and return stdout"""
    rawOutput = subprocess.run(cmd,check=True, shell=True, stdout=subprocess.PIPE, universal_newlines=True)
    return(rawOutput.stdout.split('\n')[0:-1])

def checkDateTimeOnFile(machine,lastMonth):
    latestLog = bash("ls /hive/users/chmalee/logs/trimmedLogs/result/"+machine+" | tail -1")
    date = latestLog[0].split(".")[0]
    datetime_object = datetime.datetime.strptime(date, '%Y%m%d')
    if datetime_object < lastMonth:
        print("The following machine logs are more than 45 days old. Latest log: "+machine+": "+str(date))

def checkDateTimeOnAllMachines():
    machinesToCheck = ['hgw1','hgw2','asiaNode','euroNode']
    today = datetime.datetime.today()
    lastMonth = today - datetime.timedelta(days=45)
    for machine in machinesToCheck:
        checkDateTimeOnFile(machine,lastMonth)
        
checkDateTimeOnAllMachines()
