package edu.ucsc.genome.qa.monitor;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.sql.*;

import java.util.Iterator;
import java.util.Properties;

/** 
 *  Connect to where mod_log_sql is dumping Apache access logs.
 *  Check for new rows within the last number of minutes.
 */

public class ApacheMonitor {

  static void usage() {
    System.out.println(
      "\nApacheMonitor - read logs from mod_log_sql\n" +
      "Check for new rows within the last number of minutes.\n" +
      
      "usage:\n" +
      "   java ApacheMonitor propertiesFile [verbose]\n" +
      "where properties files may contain sourceMachine, sourceDB, sourceTable, " +
      "targetMachine, errorCodes, minutes.\n" +
      "   java ApacheMonitor default\n" +
      "This will use the default properties\n" +
      "Verbose will report even if no errors found\n"
      );
    System.exit(-1);
  }

  public static void main(String[] args) {

    boolean debug = false;
    int debugTime = 1087840000;  // works 06-21-04 11:30
    String mode  = "";

    /* Process command line properties, and load them into machine and table. */
    if (args.length < 1 || args.length > 2)
        usage();
    if (args.length == 2) {
        mode = args[1];
    }
    LogTarget target = new LogTarget(args[0]);

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

      // get mysql time, set time delta
      String timequery = "SELECT unix_timestamp(now()) AS secondsNow";
      ResultSet timeRS = stmt.executeQuery(timequery); 
      timeRS.next(); 
      int secondsNow = timeRS.getInt("secondsNow");
      int secondsDelta = target.minutes * 60;
      int timeDelta = secondsNow - secondsDelta;

      if (debug == true) {
        System.out.println("got past timestamp query");
      }
      // check for any rows within time delta
      String nullquery = "SELECT COUNT(*) AS cnt FROM ";
      nullquery = nullquery + target.sourceTable + " ";
      nullquery = nullquery + "WHERE time_stamp > " + timeDelta;
      if (!allMachines) {
        nullquery = nullquery + " AND machine_id = " + target.targetMachine;
      }

      if (debug == true) {
        System.out.println(nullquery);
        System.out.println("setting new nullquery");
        nullquery = "SELECT COUNT(*) AS cnt FROM " + target.sourceTable +
                    " WHERE time_stamp > " + debugTime;
        System.out.println(nullquery);
      }
      ResultSet nullRS = stmt.executeQuery(nullquery);
      if (debug == true) {
        System.out.println("created nullquery ResultSet object");
      }
      nullRS.next();
      int nullcnt = nullRS.getInt("cnt");

      if (debug == true) {
        System.out.println("got past first COUNT query");
      }
      // check for matching rows
      String testquery = "SELECT COUNT(*) AS cnt FROM ";
      testquery = testquery + target.sourceTable + " ";
      testquery = testquery + "WHERE status = " + target.errorCode + " ";
      testquery = testquery + "AND time_stamp > " + timeDelta;

      if (debug == true) {
        System.out.println("got past second COUNT query");
        System.out.println("setting testquery to debugTime");
        testquery = "SELECT COUNT(*) AS cnt FROM " + target.sourceTable +
                    "WHERE status = 500 AND time_stamp > " + debugTime;
      }
      if (!allMachines) {
        testquery = testquery + " and machine_id = " + target.targetMachine;
      }

      ResultSet testRS = stmt.executeQuery(testquery);
      testRS.next();
      int cnt = testRS.getInt("cnt");

      // set to print only if errors detected or if verbose mode
      if (cnt != 0 || mode.equals("verbose")) {
        System.out.println(nullquery);
        System.out.println("Count of rows with any status code = " + nullcnt);
        System.out.println(testquery);
        System.out.println("Count of matching rows = " + cnt);
      }


      // if nothing found, we're done
      if (cnt == 0) System.exit(0);

      // get all matching rows
      String listquery = "SELECT machine_id, referer, remote_host, " +
        "request_uri, time_stamp FROM ";
      listquery = listquery + target.sourceTable + " ";
      listquery = listquery + "WHERE status = " + target.errorCode + " ";
      listquery = listquery + "AND time_stamp > " + timeDelta;
      if (!allMachines) {
        listquery = listquery + " AND machine_id = " + target.targetMachine;
      }
      if (debug == true) {
        listquery = "SELECT machine_id, referer, remote_host," +
          " request_uri, time_stamp FROM " + target.sourceTable + 
          " WHERE status = " + target.errorCode +
          " AND time_stamp > " + debugTime;
      }

      // set variables for formatting output into columns
      int i = 0;
      int refererSize = 0;
      int remHostSize = 0;

      // get results of query
      ResultSet listRS = stmt.executeQuery(listquery);
      listRS.last();
      int arraySize = listRS.getRow();
      listRS.beforeFirst();
      String remHost[] = new String[arraySize];
      String refUser[] = new String[arraySize];
      while (listRS.next()) {
        String request_uri = listRS.getString("request_uri");
        String referer     = listRS.getString("referer");
        String remote_host = listRS.getString("remote_host");
        String machine_id  = listRS.getString("machine_id");
	int time_stamp = listRS.getInt("time_stamp");
	int deltaSeconds = secondsNow - time_stamp;
	int deltaMinutes = deltaSeconds / 60;
	System.out.print("Status " + target.errorCode + " from " + 
                          request_uri + " on ");
	System.out.println(machine_id + "; " + deltaMinutes + " minutes ago");
        // store details and set size variables to longest string each time
        remHost[i] = remote_host;
        refUser[i] = referer;
        if (remote_host.length() > remHostSize) {
          remHostSize = remote_host.length();
        }
        if (referer.length() > refererSize) {
          refererSize = referer.length();
        }
        i++;
      }

      // compute sizes and print header for details output 
      String rem       = "remote_host";
      String separator = "-----------";
      int remHostDiff = remHostSize - rem.length(); 
      for (int j = 0; j < remHostDiff; j++) {
        rem       = rem + " ";
        separator = separator+ "-";
      }
      separator = separator + "-|-";
      for (int j = 0; j < refererSize; j++) {
        separator = separator + "-";
      }
      separator = separator + "-|";
      System.out.println(" \n" + rem + " | referer");
      System.out.println(separator);

      // print details of error
      for (int j = 0; j < remHost.length; j++) {
        int remPrintSpaces = remHostSize - remHost[j].length();
        for (int k = 0; k < remPrintSpaces; k++) {
          remHost[j] = remHost[j] + " ";
        }
        System.out.println(remHost[j] + " | " + refUser[j] + " |");
      }
      System.out.println(separator);

      stmt.close();
      conn.close();

    } catch (Exception e) {
      // markd's suggestion to change this:
      // System.err.println(e.getMessage());
      throw new Error(e);
    }
  }
}
