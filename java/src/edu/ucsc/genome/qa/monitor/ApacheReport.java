package edu.ucsc.genome.qa.monitor;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.sql.*;

import java.text.*;
import java.util.LinkedList;
import java.util.ListIterator;
import java.util.Iterator;
import java.util.Properties;
import java.util.Calendar;

/**
 *  Connect to where mod_log_sql is dumping Apache access logs.
 *  Generate hourly report for previous day.
 */

public class ApacheReport {

    static String fileMonth;
    static DecimalFormat df = new DecimalFormat("#,##0.00");

   /**
    *  Get the proper digit for the 0-based month variable
    *
    *  @param month     The month; becomes part of file name.
    *
    *  @return          The two-character version of the month.
    */
    static String getMonth(int month) {
      if (month == 0) return "01";
      if (month == 1) return "02";
      if (month == 2) return "03";
      if (month == 3) return "04";
      if (month == 4) return "05";
      if (month == 5) return "06";
      if (month == 6) return "07";
      if (month == 7) return "08";
      if (month == 8) return "09";
      if (month == 9) return "10";
      if (month == 10) return "11";
      return "12";
    }

   /**
    *  Get the month name in text for the 0-based month variable
    *
    *  @param month     The month; becomes part of file name.
    *
    *  @return          The text 3-char name of the month.
    */
    static String getMonthText(int month) {
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
    *  Open daily file for writing HTML output
    *
    *  @param outpath   The path; becomes part of file name.
    *  @param year      The year; becomes part of file name.
    *  @param month     The month; becomes part of file name.
    *  @param day       The day; becomes part of file name.
    *
    *  @throws          Exception.
    *  @return          The PrintWriter object for daily file report.
    */
    static PrintWriter setDailyFile(String outpath, int year, int month, int day) throws Exception {
      DecimalFormat df2 = new DecimalFormat("#,#00");
      String filename = outpath + year + "-" + getMonth(month) + "-"
                        + df2.format(day) + ".html";
      System.out.println("\nWriting daily report to \n" + filename + "\n");
      String url = "/usr/local/apache/htdocs/";
      if (outpath.startsWith(url)) {
        String urlpath = outpath.replaceFirst(url, "http://hgwdev.soe.ucsc.edu/");
        System.out.println("Try the URL directly: \n" +
                           urlpath + year + "-" + getMonth(month) + "-"
                           + df2.format(day) + ".html");
      }
      FileWriter fout = new FileWriter(filename);
      PrintWriter pw = new PrintWriter(fout);
      return(pw);
    }

   /**
    *  Name monthly file for yesterday for writing HTML output
    *  and print the name of the file to standard out.
    *  Create new file for next month if it is a new month.
    *
    *  @param outpath   The path; becomes part of file name.
    *  @param year      The year; becomes part of file name.
    *  @param month     The month; becomes part of file name.
    *
    *  @throws          Exception.
    *  @return          The name for monthly report file for yesterday.
    */
    static String setMonthlyFile(String outpath, int year, int month)
          throws Exception {

      // for testing -- set the month to last month

      String yestMonth = getMonth(month);

      // make name of file for yesterday's data
      fileMonth = outpath + year + "-" + getMonth(month) + ".html";
      Calendar calToday = Calendar.getInstance();
      int nowMonth = calToday.get(Calendar.MONTH);
      int nowYear = calToday.get(Calendar.YEAR);
      String thisMonth = getMonth(nowMonth);

      if (!thisMonth.equals(yestMonth)) {
        System.out.println("\n========================================\n");
        System.out.println("\n" + getMonthText(month)
                   + " monthly report available at: \n" + fileMonth + "\n");
        String url = "/usr/local/apache/htdocs/";
        if (outpath.startsWith(url)) {
          String urlpath = outpath.replaceFirst(url, "http://hgwdev.soe.ucsc.edu/");
          System.out.println("Try the URL directly: \n" +
                             urlpath + year + "-" + getMonth(month) + ".html");
        }
        // start new month file when month changes

        String thisMonthFile = outpath + nowYear + "-" + thisMonth + ".html";

        FileWriter fwNextMonth = new FileWriter(thisMonthFile);
        PrintWriter pwNextMonth = new PrintWriter(fwNextMonth);
        printHeader(pwNextMonth, nowYear, nowMonth, 0, "month");
        printFooter(pwNextMonth);
        pwNextMonth.close();
        // System.exit(-1);

      }
      return(fileMonth);
    }

   /**
    *  Prints header for HTML output file
    *
    *  @param pw        A PrintWriter object.
    *  @param year      The year; becomes part of file name.
    *  @param month     The month; becomes part of file name.
    *  @param day       The day; becomes part of file name.
    *  @param fileFlag  Type of file -- daily or monthly.
    */
    static void printHeader(PrintWriter pw, int year, int month, int day, String fileFlag) {
      pw.print("<HTML>\n");
      pw.print("<HEAD>\n");
      if (fileFlag.equals("daily")) {
        pw.print("<TITLE>Daily Apache Report -- " + getMonthText(month) + " " +
           day + ", " + year + " </TITLE>\n");
      } else {
        pw.print("<TITLE>Daily Apache Report -- " + getMonthText(month) + ", "
           + year + " </TITLE>\n");
      }
      pw.print("</HEAD>\n");
      pw.print("<BODY>\n");
      if (fileFlag.equals("daily")) {
        pw.print("<H2>Daily Apache Report -- " + getMonthText(month) + " " +
                  day + ", " + year + "</H2>\n");
      } else {
        pw.print("<H2>Monthly Apache Report -- " + getMonthText(month) + ", " +
               year + "</H2>\n");
      }
      pw.print("<TABLE BORDER CELLSPACING=0 CELLPADDING=5>\n");
      pw.print("<TR>\n");
      if (fileFlag.equals("daily")) {
        pw.print("<TH>Hour</TH>");
      } else {
        pw.print("<TH>Day</TH>");
      }
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
      pw.print("</TABLE>");
      pw.print("</BODY>");
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
      "where properties files may contain sourceMachine, sourceDB, \nsourceTable, " +
      "targetMachine, errorCodes, minutes.\n" +
      "and where path defaults to /usr/local/apache/htdocs/qa/test-results/apache " +
      "\n(use \"local\" for local directory).\n" +
      "\n   java ApacheReport default\n" +
      "This will use the default properties.\n"
      );
      System.exit(-1);
  }

/**
  *  Writes Daily and Monthly usage reports of error 500s on the RoundRobin.
  *
  */
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
      PrintWriter pw = setDailyFile(outpath, yearYesterday, monthYesterday, dayYesterday);
      printHeader(pw, yearYesterday, monthYesterday, dayYesterday, "daily");

      // try to trap some output.
      // pw.close();  // that worked to trap header.

      // set percent output to two decimals

      // iterate through 24 hours
      int secondsInHour = 60 * 60;
      int totalAccess = 0;
      int totalError = 0;
      for (int reportHour = 0; reportHour <= 23; reportHour++) {
      // for (int reportHour = 0; reportHour <= 1; reportHour++) {
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

        // need to convert numerator and denominator to float first?  (yes)
	float percent;
        if (nullcnt == 0) {
          percent = 0;
        } else {
          percent = (float) cnt / (float) nullcnt * 100;
        }
        String printPercent = df.format(percent);
        pw.print("<TD>" + printPercent + "</TD>\n");
        pw.print("</TR>\n");
      }
      float totalPercent = (float) totalError / (float) totalAccess * 100;
      // print totals
      pw.print("<TR>\n");
      pw.print("<TD>Totals</TD>\n");
      pw.print("<TD>" + totalAccess + "</TD>\n");
      pw.print("<TD>" + totalError + "</TD>\n");

      String printTotPercent = df.format(totalPercent);
      pw.print("<TD>" + printTotPercent + "</TD>\n");
      // System.out.println("totalAccess = " + totalAccess);
      // System.out.println("totalError  = " + totalError );
      // System.out.println("origPercent = " + totalPercent);
      // System.out.println("totalPercent= " + printTotPercent);
      pw.print("</TR>\n");
      printFooter(pw);
      pw.close();
      stmt.close();
      conn.close();

      // run monthly report
      fileMonth = setMonthlyFile(outpath, yearYesterday, monthYesterday);
      printMonthly(dayYesterday, totalAccess, totalError, printTotPercent);
    }
    catch (Exception e) {
      System.err.println(e.getMessage());
    }
  }

 /**
  *  Read monthly file and write HTML output
  *
  *  @param dayYesterday     Yesterday's date.
  *  @param totalAccess      Number of system accesses yesterday.
  *  @param totalError       Number of errors yesterday.
  *  @param printTotPercent  Formatted total percent.
  */
  private static void printMonthly (int dayYesterday, int totalAccess,
     int totalError, String printTotPercent)
     throws Exception {

    File f = new File(fileMonth);
    FileReader  frMonth = new FileReader(f);
    BufferedReader  brMonth = new BufferedReader(frMonth);

    // get existing file and load into linklist,
    // filtering out the closing HTML tags.

    LinkedList ll = new LinkedList();
    String checkEnd = "</TABLE></BODY></HTML>";

    String nextLine = brMonth.readLine();
    while (nextLine.indexOf(checkEnd) < 0) {
      ll.addLast(nextLine);
      nextLine = brMonth.readLine();
    }

    brMonth.close();


    // write new data on next line
    String row = tableRow(dayYesterday, totalAccess, totalError, printTotPercent);
    ll.addLast(row);
    FileWriter fwMonth  = new FileWriter(fileMonth);
    PrintWriter pwMonth = new PrintWriter(fwMonth);

    // print LinkedList and tally totals for access and errors
    ListIterator iter = ll.listIterator(0);
    String outLine = "";
    int accTot = 0;
    int errTot = 0;
    float pctTot = 0;
    while (iter.hasNext()) {
      // keep existing line to compare
      String prevLine = outLine;
      outLine = iter.next().toString() + "\n";
      // don't reprint line if run twice in same day or if is Total line
      if ( !(outLine.equals(prevLine) || outLine.matches("Total"))) {
        pwMonth.print(outLine);
        int [] tots = parseRow(outLine);
        accTot = accTot + tots[0];
        errTot = errTot + tots[1];
        // System.out.println("accTot  = " + accTot);
        // System.out.println("errTot  = " + errTot);
      }
      if (accTot == 0) {
        pctTot = 0;
      } else {
        pctTot = (float) errTot / (float) accTot * 100;
      }
    }
    // print Total row.
    String printPct = df.format(pctTot);
    // System.out.println("pctTot   = " + pctTot);
    // System.out.println("printPct = " + printPct);
    String totLine = tableRow(0, accTot, errTot, printPct); 
    totLine = totLine.replaceFirst("<TD>0", "<TD>Totals");
    pwMonth.print(totLine);

    // print footer back.
    printFooter(pwMonth);
    pwMonth.close();
  }

 /**
  *  Parse out values from access row.
  *
  *  @param outLine          A row from Monthly Report table.
  *
  *  @return                 Access and error values from row.
  */
  private static int[] parseRow(String outLine) {
    int begTag = outLine.indexOf("</TD>");
    String [] tdFrag = null;
    tdFrag = outLine.split("<TD>");
    int acc = 0;
    int err = 0;
    // System.out.println("outLine = \n" + outLine);

    if ( begTag == -1 ) {
      // not a data row.
    } else {
      String [] accGrab = null;
      accGrab = tdFrag[2].split("</TD>");
      String access = accGrab[0];
      String [] errGrab = null;
      errGrab = tdFrag[3].split("</TD>");
      String errors = errGrab[0];
      acc = Integer.parseInt(access);
      err = Integer.parseInt(errors);
    }
    int [] outval = new int[2];
    outval[0] = acc;
    outval[1] = err;
    return(outval);
  }

 /**
  *  Writes HTML row for four-column table.
  *
  *  @param index            First column (day or hour).
  *  @param access           Number of system accesses yesterday.
  *  @param error            Number of errors yesterday.
  *  @param totPercent       Formatted total percent.
  *
  *  @return                 HTML row for a table.
  */
  private static String tableRow(int index, int access,
    int error, String totPercent) {
    String newDataLine;
    newDataLine = "<TR><TD>" + index + "</TD> " +
                      "<TD>" + access + "</TD> " +
                      "<TD>" + error + "</TD> " +
                      "<TD>" + totPercent + "</TD> </TR>";
    return(newDataLine);
  }
}
