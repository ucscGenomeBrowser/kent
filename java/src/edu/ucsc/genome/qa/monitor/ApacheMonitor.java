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
      "ApacheMonitor - read logs from mod_log_sql\n" +
      "Check for new rows within the last number of minutes.\n" +
      
      "usage:\n" +
      "   java ApacheMonitor propertiesFile\n" +
      "where properties files may contain sourceMachine, sourceDB, sourceTable, " +
      "targetMachine, errorCodes, minutes.\n" +
      "   java ApacheMonitor default\n" +
      "This will use the default properties\n"
      );
    System.exit(-1);
  }

  public static void main(String[] args) {

    boolean debug = false;

    /* Process command line properties, and load them into machine and table. */
    if (args.length != 1)
        usage();
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
      String timequery = "select unix_timestamp(now()) as secondsNow";
      ResultSet timeRS = stmt.executeQuery(timequery); 
      timeRS.next(); 
      int secondsNow = timeRS.getInt("secondsNow");
      int secondsDelta = target.minutes * 60;
      int timeDelta = secondsNow - secondsDelta;

      // check for matching rows
      String testquery = "select count(*) as cnt from ";
      testquery = testquery + target.sourceTable + " ";
      testquery = testquery + "where status = " + target.errorCode + " ";
      testquery = testquery + "and time_stamp > " + timeDelta;

      if (!allMachines) {
        testquery = testquery + " and machine_id = " + target.targetMachine;
      }

      System.out.println(testquery);
      ResultSet testRS = stmt.executeQuery(testquery);
      testRS.next();
      int cnt = testRS.getInt("cnt");
      System.out.println("Count of matching rows = " + cnt);

      // if nothing found, we're done
      if (cnt == 0) System.exit(0);

      // get all matching rows
      String listquery = "select request_uri, machine_id, time_stamp from ";
      listquery = listquery + target.sourceTable + " ";
      listquery = listquery + "where status = " + target.errorCode + " ";
      listquery = listquery + "and time_stamp > " + timeDelta;
      if (!allMachines) {
        listquery = listquery + " and machine_id = " + target.targetMachine;
      }

      System.out.println(listquery);

      ResultSet listRS = stmt.executeQuery(listquery);
      while (listRS.next()) {
        String request_uri = listRS.getString("request_uri");
        String machine_id = listRS.getString("machine_id");
	int time_stamp = listRS.getInt("time_stamp");
	int deltaSeconds = secondsNow - time_stamp;
	int deltaMinutes = deltaSeconds / 60;
	System.out.print("Status " + target.errorCode + " from " + request_uri + " on ");
	System.out.println(machine_id + "; " + deltaMinutes + " minutes ago");
      }

    } catch (Exception e) {
      System.err.println(e.getMessage());
    }

  }
}
