package edu.ucsc.genome.qa.monitor;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.sql.*;

import java.text.*;
import java.util.Iterator;
import java.util.Properties;
import java.util.Calendar;

/** 
 *  Connect to where mod_log_sql is dumping Apache access logs.
 *  Generate hourly report for previous day.
 */

public class ApacheReport {

    static String getMonth(int month) {
      if (month == 0) return "Jan";
      if (month == 1) return "Feb";
      if (month == 2) return "Mar";
      if (month == 3) return "Apr";
      if (month == 4) return "May";
      if (month == 5) return "Jun";
      if (month == 6) return "Jul";
      if (month == 7) return "Aug";
      if (month == 8) return "Sep";
      if (month == 9) return "Oct";
      if (month == 10) return "Nov";
      return "Dec";
    }

   /** 
    *  Open file for writing HTML output
    *
    *  @param path   The path; becomes part of file name
    *  @param year   The year; becomes part of file name
    *  @param month  The month; becomes part of file name
    *  @param day    The day; becomes part of file name
    *  @throws       Exception
    *  @return       The PrintWriter object
    */
    static PrintWriter setOutputFile(String outpath, int year, int month, int day) throws Exception { 

      String filename = outpath + year + getMonth(month) + day + ".html";
      System.out.println("\nWriting daily report to \n" + filename + "\n");
      String url = "/usr/local/apache/htdocs/";
      if (outpath.startsWith(url)) {
        String urlpath = outpath.replaceFirst(url, "http://hgwdev.cse.ucsc.edu/");
        System.out.println("\nTry the URL directly: \n" + 
                           urlpath + year + getMonth(month) + day + ".html");
      }
      FileWriter fout = new FileWriter(filename);
      PrintWriter pw = new PrintWriter(fout);
      return(pw);
    }

   /** 
    *  Prints header for HTML output file
    *
    *  @param pw     A PrintWriter object
    *  @param year   The year; becomes part of file name
    *  @param month  The month; becomes part of file name
    *  @param day    The day; becomes part of file name
    */
    static void printHeader(PrintWriter pw, int year, int month, int day) {
      pw.print("<HTML>\n");
      pw.print("<HEAD>\n");
      pw.print("<TITLE>Daily Apache Report</TITLE>\n");
      pw.print("</HEAD>\n");
      pw.print("<BODY>\n");
      pw.print("<H2>Daily Apache Report " + getMonth(month) + " " + 
               day + ", " + year + "</H2>\n");
      pw.print("<TABLE BORDER CELLSPACING=0 CELLPADDING=5>\n");
      pw.print("<TR>\n");
      pw.print("<TH>Hour</TH>");
      pw.print("<TH>Accesses</TH>");
      pw.print("<TH>Error 500</TH>");
      pw.print("<TH>Percent</TH>");
      pw.print("</TR>\n");
    }

   /** 
    *  Prints footer for HTML output file
    *
    *  @param pw     A PrintWriter object
    */
    static void printFooter(PrintWriter pw) {
      pw.print("</TABLE>\n");
      pw.print("</BODY>\n");
      pw.print("</HTML>\n");
    }

   /** 
    *  Creates a Calendar object with YEAR/MONTH/DAY set 24 hours in the past
    *
    *  @return  a Calendar object with YEAR/MONTH/DAY set 24 hours in the past
    */
    static Calendar getCalYesterday() {

      // first figure out what day it is

      Calendar calNow = Calendar.getInstance();
      int dayNow   = calNow.get(Calendar.DATE);
      int monthNow = calNow.get(Calendar.MONTH);
      int yearNow  = calNow.get(Calendar.YEAR);

      // next figure out what time it is (milliseconds since Jan. 1, 1970)
      java.util.Date dateNow = calNow.getTime();
      long millisecondsNow = dateNow.getTime();

      // subtract 24 hours
      long millisecondsInDay = 1000 * 60 * 60 * 24;
      long millisecondsYesterday = millisecondsNow - millisecondsInDay;

      // construct Calendar
      java.util.Date dateYesterday = new Date(millisecondsYesterday);
      // System.out.println("millisecNow       = " + millisecondsNow);
      // System.out.println("millisecYesterday = " + millisecondsYesterday);
      // System.out.println("dateYesterday: " + dateYesterday.toString() + "\n");
      Calendar calYesterday = Calendar.getInstance();
      calYesterday.setTime(dateYesterday);

      return(calYesterday);

    }


 /** 
  *  Writes a usage message to standard out.
  *
  */
  static void usage() {
    System.out.println(
      "\nApacheReport - read logs from mod_log_sql\n" +
      "Generate reports.\n" +
      
      "usage:\n" +
      "   java ApacheReport propertiesFile [-path=pathname] [-mode=verbose]\n" +
      "where properties files may contain sourceMachine, sourceDB, sourceTable, " +
      "targetMachine, errorCodes, minutes.\n" +
      "and where path defaults to /usr/local/apache/htdocs/qa/test-results/apache " +
      "\n(use \"local\" for local directory).\n" +
      "\n   java ApacheReport default\n" +
      "This will use the default properties.\n"
      );
      System.exit(-1);
  }

  public static void main(String[] args) {
    String mode  = "";
    String outpath = "/usr/local/apache/htdocs/qa/test-results/apache/";
    boolean debug = false;

    /* Process command-line properties, and load them into LogTarget object. */
    if (args.length < 1 || args.length > 3)
      usage();
    if (args.length == 2 || args.length == 3) {
      // set second argument
      String defineArg = "-path=";
      if (args[1].startsWith(defineArg)) {
        outpath = args[1].replaceFirst(defineArg, "");
        if (outpath.equals("local")) {
          outpath = "";
        } else {
          // allow trailing "/" or not in input
          outpath = outpath + "/";
          outpath = outpath.replaceAll("//", "/");
        }
      }
      defineArg = "-mode=";
      if (args[1].startsWith(defineArg)) {
        mode = args[1].replaceFirst(defineArg, "");
      }
    }
    if (args.length == 3) {
      // set third argument
      String defineArg = "-path=";
      if (args[2].startsWith(defineArg)) {
        outpath = args[2].replaceFirst(defineArg, "");
        if (outpath.equals("local")) {
          outpath = "";
        } else {
          // allow trailing "/" or not in input
          outpath = outpath + "/";
          outpath = outpath.replaceAll("//", "/");
        }
      }
      defineArg = "-mode=";
      if (args[2].startsWith(defineArg)) {
        mode = args[2].replaceFirst(defineArg, "");
      }
    }

    LogTarget target = new LogTarget(args[0]);

    // construct Calendar
    Calendar calYesterday = getCalYesterday();

    int dayYesterday = calYesterday.get(Calendar.DATE);
    int monthYesterday = calYesterday.get(Calendar.MONTH);
    int yearYesterday  = calYesterday.get(Calendar.YEAR);
    if (debug == true || mode.equals("verbose")) {
      System.out.print("\nGenerating report for year = " + yearYesterday + 
                     " month = " + monthYesterday);
      System.out.println(" day = " + dayYesterday);
    }


    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
    
    // set DB connection 
    HGDBInfo dbinfo; 
    try {
      dbinfo = new HGDBInfo(target.sourceMachine, target.sourceDB);
    } catch (Exception e) {
      System.out.println(e.toString());
      return;
    }
    if (!dbinfo.validate()) return;
    String myurl = QADBLibrary.jdbcURL(dbinfo);

    try {
      Connection conn = DriverManager.getConnection(myurl);
      Statement stmt = conn.createStatement();

      // all machines?
      boolean allMachines = false;
      String machine = target.targetMachine;
      if (machine.equals("all")) allMachines = true;

      // configure output file
      PrintWriter pw = setOutputFile(outpath, yearYesterday, monthYesterday, dayYesterday);

      printHeader(pw, yearYesterday, monthYesterday, dayYesterday);
      // try to trap some output.
      // pw.close();  that worked to trap header.

      // set percent output to two decimals
      DecimalFormat df = new DecimalFormat("#,##0.00");
  
      // iterate through 24 hours
      int secondsInHour = 60 * 60;
      int totalAccess = 0;
      int totalError = 0;
      for (int reportHour = 0; reportHour <= 23; reportHour++) {
        pw.print("<TR>\n");
        pw.print("<TD>" + reportHour + "</TD>\n");
        calYesterday.set(yearYesterday, monthYesterday, dayYesterday, reportHour, 0, 0);
        java.util.Date reportDate = calYesterday.getTime();
        long millisecondsReport = calYesterday.getTimeInMillis();
        long secondsReportStart = millisecondsReport / 1000;
        long secondsReportEnd = secondsReportStart + secondsInHour;
        if (mode.equals("verbose")) {
          System.out.println("reportHour = " + reportHour);
        }
        if (debug == true) {
          System.out.println("reportHour = " + reportHour);
          System.out.println("millisecReport = " + millisecondsReport);
          System.out.println("secReportStart = " + secondsReportStart);
          System.out.println("secReportEnd   = " + secondsReportEnd  );
        }

        // check for any rows within this hour
        String nullquery = "select count(*) as cnt from ";
        nullquery = nullquery + target.sourceTable + " ";
        nullquery = nullquery + "where time_stamp >= " + secondsReportStart + " ";
	nullquery = nullquery + "and time_stamp < " + secondsReportEnd;
        if (!allMachines) {
          nullquery = nullquery + " and machine_id = " + target.targetMachine;
        }
        ResultSet nullRS = stmt.executeQuery(nullquery);
        nullRS.next();
        int nullcnt = nullRS.getInt("cnt");
        
        if (debug == true || mode.equals("verbose")) {
          System.out.println(nullquery);
          System.out.println("Count of rows with any status code = " + nullcnt);
        }
        pw.print("<TD>" + nullcnt + "</TD>\n");
	totalAccess = totalAccess + nullcnt;

        // check for matching rows
        String testquery = "select count(*) as cnt from ";
        testquery = testquery + target.sourceTable + " ";
        testquery = testquery + "where status = " + target.errorCode + " ";
        testquery = testquery + "and time_stamp >= " + secondsReportStart + " ";
        testquery = testquery + "and time_stamp < " + secondsReportEnd;

        if (!allMachines) {
          testquery = testquery + " and machine_id = " + target.targetMachine;
        }

        ResultSet testRS = stmt.executeQuery(testquery);
        testRS.next();
        int cnt = testRS.getInt("cnt");
        if (debug == true || mode.equals("verbose")) {
          System.out.println(testquery);
          System.out.println("Count of matching rows = " + cnt + "\n");
        }
        pw.print("<TD>" + cnt + "</TD>\n");
	totalError = totalError + cnt;

        // need to convert numerator and denominator to float first (yes)?
	float percent;
        if (nullcnt == 0) {
          percent = 0;
        } else {
          percent = (float) cnt / (float) nullcnt;
        }
        String printPercent = df.format(percent);
        pw.print("<TD>" + printPercent + "</TD>\n");
        pw.print("</TR>\n");
      }
      // print totals
      pw.print("<TR>\n");
      pw.print("<TD>Totals</TD>\n");
      pw.print("<TD>" + totalAccess + "</TD>\n");
      pw.print("<TD>" + totalError + "</TD>\n");
      float totalPercent = totalError / totalAccess;
      String printTotPercent = df.format(totalPercent);
      pw.print("<TD>" + printTotPercent + "</TD>\n");
      pw.print("</TR>\n");
      printFooter(pw);
      pw.close();
      stmt.close();
      conn.close();

    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

  }
}
