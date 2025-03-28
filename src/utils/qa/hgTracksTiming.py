# Meant as a substitute for hgTracksRandom
# Queries a list of GB servers (crontab set to every 15m) and documents their load time
# as well as their status code, if they took too long to load, or if hgTracks display did not
# fully load. Alerts with a printed error when a negative condition is encountered.
# Each run it reads the list of all observed times and regenerates a table and graphs displaying
# the change of server load times over time. Once per month it reports as a reminder to check for abnormalities
# If running on a new user, you will need to copy the index.html page from qateam and run the function here once: makeSymLinks(user,save_dir)

import requests, subprocess, time, datetime, getpass, os, urllib3, matplotlib
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import matplotlib.dates as mdates
import matplotlib
from collections import defaultdict
from collections import deque

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

def makeSymLinks(user,save_dir):
    if user == 'qateam':
        bash("ln -sf "+save_dir+"*.html /usr/local/apache/htdocs-genecats/qa/test-results/hgTracksTiming/")
        bash("ln -sf "+save_dir+"*.png /usr/local/apache/htdocs-genecats/qa/test-results/hgTracksTiming/")
    else:
        bash("ln -sf "+save_dir+"*.html /cluster/home/"+user+"/public_html/cronResults/hgTracksTiming/") 
        bash("ln -sf "+save_dir+"*.png /cluster/home/"+user+"/public_html/cronResults/hgTracksTiming/")  

def getLastLinesAndMakeList(file_path, num_lines=20):
    with open(file_path, "r") as file:
        if num_lines == 'All':
            all_lines = file.readlines()  # Read all lines in the file
        else:
            last_lines = deque(file, maxlen=num_lines)  # Read only the last 'num_lines' lines
    
    dates = []
    times = []

    if num_lines == 20:
        for line in last_lines:
            # Split the line and extract the date and time
            parts = line.rstrip().split("\t")
            dates.append(parts[0])  # First part is the date
            times.append(float(parts[1].split("s")[0]))  # Second part is the time, removing the 's'

    elif num_lines == 80:
        timeToWrite = []
        for i, line in enumerate(last_lines):
            parts = line.rstrip().split("\t")
            time = float(parts[1].split("s")[0])
        # Apply logic to every 4th line (0, 4, 8, ...)
            if i % 4 == 0:
                timeToWrite.append(time)
                averageTime = round(sum(timeToWrite)/len(timeToWrite),1)
                dates.append(parts[0])  # First part is the date
                times.append(averageTime)
                timeToWrite = []
            else:
                timeToWrite.append(time)

    elif num_lines == 'All': #Calculate average times of all lines / 20 
        total_lines = len(all_lines)

        # Determine 20 evenly spaced line indices
        indices = [int(i * total_lines / 20) for i in range(20)]
        timeToWrite = []
        
        for i, line in enumerate(all_lines):
            parts = line.rstrip().split("\t")
            time = float(parts[1].split("s")[0])
            
            if i in indices:
                timeToWrite.append(time)
                averageTime = round(sum(timeToWrite)/len(timeToWrite),1)
                dates.append(parts[0])  # First part is the date
                times.append(averageTime)
                timeToWrite = []
            else:
                timeToWrite.append(time)
                
    return(dates,times)

def generateGraphs(user,save_dir,filePath,server):
    #Create the 3 time scale graphs for each server
    
    reportsToGenerate = ['Last 5h','Last 20h','AllTime/20']
    n=0
    
    for report in reportsToGenerate:
        n+=1
        # x axis values
        if report == "Last 5h":
            dates,times = getLastLinesAndMakeList(filePath, num_lines=20)
        elif report == "Last 20h":
            dates,times = getLastLinesAndMakeList(filePath, num_lines=80)
        elif report == "AllTime/20":
            dates,times = getLastLinesAndMakeList(filePath, num_lines='All')
        
        x_dates = dates
        y = times

        # plotting the points 
        plt.plot(x_dates, y, marker='o')

        # Rotate date labels for better readability
        plt.gcf().autofmt_xdate()

        # naming the x axis
        plt.xlabel('Date/time')
        # naming the y axis
        plt.ylabel("Load time in s")
        plt.xticks(x_dates)
        plt.title(report + " - " + server) # giving a title to my graph

        # Ensure the figure is fully rendered before saving
        plt.gcf().canvas.draw()  # Force rendering of the canvas

        # Save the plot to a file
        plt.savefig(save_dir + "/" + server + "." + str(n) + ".png", bbox_inches='tight')

        # Clear the current plot to avoid overlaps with the next plot
        plt.clf()

def create_save_dir(user):
    save_dir = f"/hive/users/{user}/hgTracksTiming/"
    os.makedirs(save_dir, exist_ok=True)  # Creates the directory if it doesn't exist
    return save_dir

def createTableOfTimeChanges(filePath,save_dir,server,n,totalN):
    tableFilePath = save_dir + "timeChangesTable.html"
    monthly_data = defaultdict(list)

    # Parse the input file
    with open(filePath, "r") as file:
        for line in file:
            parts = line.rstrip().split("\t")
            date_str = parts[0]  # First part is the date
            time = float(parts[1].split("s")[0])  # Second part is the time, removing the 's'

            # Extract year and month
            date_obj = datetime.datetime.strptime(date_str, "%Y-%m-%d-%H:%M")
            year_month = (date_obj.year, date_obj.month)

            # Store time value for the year-month combination
            monthly_data[year_month].append(time)

    # Calculate averages
    averages = {ym: sum(times) / len(times) for ym, times in monthly_data.items()}

    # Generate HTML
    if server =="hgwdev":
        writeMode = "w"
    else:
        writeMode = "a"
    with open(tableFilePath, writeMode) as output_file:
        if server =="hgwdev":
            output_file.write("<div>\n<table>\n<tr>\n<td valign='top'>\n")

        elif n%2!=0:
            output_file.write("</td>\n</tr>\n</table>\n<td valign='top'>\n")
        else:
            output_file.write("<table>\n<tr>\n<td valign='top'>\n")
            
        output_file.write("<h3>"+server+"</h3>\n<table border=\"1\">\n")
        
        # Create table header (Months)
        months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
        output_file.write("<tr><th>Year</th>" + "".join(f"<th>{month}</th>" for month in months) + "</tr>\n")

        # Populate table rows with only available months
        years = sorted(set(year for year, _ in averages.keys()))
        for year in years:
            output_file.write(f"<tr><td>{year}</td>")
            prev_value = None
            for month in range(1, 13):
                value = averages.get((year, month), "-")
                if isinstance(value, float):
                    value = f"{value:.3f}"  # Format to 3 decimal places
                output_file.write(f"<td>{value}</td>")
            output_file.write("</tr>\n")

            # Calculate and display % change row
            output_file.write(f"<tr><td>Change</td>")
            for month in range(1, 13):
                current_value = averages.get((year, month))
                if prev_value is not None and current_value is not None:
                    percent_change = ((current_value - prev_value) / prev_value) * 100
                    color = "red" if percent_change > 0 else "green"
                    output_file.write(f"<td style='color:{color}'> {percent_change:.2f}% </td>")
                else:
                    output_file.write("<td>-</td>")
                prev_value = current_value
            output_file.write("</tr>\n")

        output_file.write("</table>\n")
        if n==totalN:
            output_file.write("</div>\n")

def checkFileExistsForMonthlyReport(save_dir,user):
    month = datetime.datetime.today().strftime("%m")
    fileToCheckAndReport = save_dir + "monthFile" + month
    if not os.path.isfile(fileToCheckAndReport):
        # The month check file does not exist, report the monthly output
        # Delete all files matching "monthFile*" in the current directory
        for filename in os.listdir(save_dir):
            if filename.startswith("monthFile") and os.path.isfile(filename):
                os.remove(filename)

        # Create a new blank file with the specified path
        with open(fileToCheckAndReport, 'w') as new_file:
            pass  # Creates an empty file

        print("Monthly reminder to check the hgTracksTiming information for any abnormalities:\n")
        if user == 'qateam':
            print("https://genecats.gi.ucsc.edu/qa/test-results/hgTracksTiming/")
        else:
            print("https://hgwdev.gi.ucsc.edu/~"+user+"/cronResults/hgTracksTiming/")   

def queryServersAndReport(server,url,filePath,today,n,user):
    start_time = time.time()
    response = requests.get(url, verify=False)  # Disable SSL verification
    end_time = time.time()
    load_time = end_time - start_time
    page_content = response.text  # Get the page content

    # Check if the expected string is in the response
    if "END hgTracks" in page_content:
        if load_time < 15:
            problem = False
            status = "SUCCESS"
        else:
            problem = True
            status = "FAIL - hgTracks page loaded, but load time over 15s"
    else:
        problem = True
        status = "FAIL - Got status 200 return, but missing the 'END hgTracks' page string of a successful load"

    if problem == True:
        print("Potential problem with Genome Browser server.")
        print(f"URL: {url} | Status: {response.status_code} | Load Time: {load_time:.3f}s | Check: {status}")
        print("\nSee the latest timing numbers:")
        if user == 'qateam':
            print("https://genecats.gi.ucsc.edu/qa/test-results/hgTracksTiming/")
        else:
            print("https://hgwdev.gi.ucsc.edu/~"+user+"/cronResults/hgTracksTiming/")
    
    with open(filePath, "a") as file:
        file.write(f"{today}\t{load_time:.3f}s\t{response.status_code}\n")

def main():
    #Don't try to display the plot, this is for jupyter
    matplotlib.use('Agg')
    # Suppress SSL warnings - was due to an asia issue
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
    
    user = getpass.getuser()
    save_dir = create_save_dir(user)
    today = datetime.datetime.today().strftime("%Y-%m-%d-%H:%M")
        
    # Dic with all the URLs to test. To temporarily pause testing of URLs for maintenance, expected outage, etc.
    # Remove it from this dictionary
    urls = {
        "hgwdev": "https://hgwdev.gi.ucsc.edu/cgi-bin/hgTracks?hgt.trackImgOnly=1&hgt.reset=1",
        "hgwbeta": "https://hgwbeta.soe.ucsc.edu/cgi-bin/hgTracks?hgt.trackImgOnly=1&hgt.reset=1",
        "hgw1": "https://hgw1.soe.ucsc.edu/cgi-bin/hgTracks?hgt.trackImgOnly=1&hgt.reset=1",
        "hgw2": "https://hgw2.soe.ucsc.edu/cgi-bin/hgTracks?hgt.trackImgOnly=1&hgt.reset=1",
        "euro": "https://genome-euro.ucsc.edu/cgi-bin/hgTracks?hgt.trackImgOnly=1&hgt.reset=1",
        "asia": "https://genome-asia.ucsc.edu/cgi-bin/hgTracks?hgt.trackImgOnly=1&hgt.reset=1"
    }

    n=0
    for server, url in urls.items():
        n+=1
        filePath = save_dir + server + ".txt"
        queryServersAndReport(server,url,filePath,today,n,user)
        createTableOfTimeChanges(filePath,save_dir,server,n,len(urls))
        generateGraphs(user,save_dir,filePath,server)
    
    checkFileExistsForMonthlyReport(save_dir,user)
    
main()
