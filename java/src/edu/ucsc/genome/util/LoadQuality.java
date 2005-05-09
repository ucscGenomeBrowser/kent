package edu.ucsc.genome.util;
import edu.ucsc.genome.qa.lib.*;

import java.io.*;
import java.lang.Runtime;
import java.sql.*;


class LoadQuality {
  public static void main(String args[]) {
    FileReader fin;
    BufferedReader br;
    String thisline;
    String scores = "";
    String contig = "";
    boolean firsttime = true;
    String tablename = "contigQuality";
    String insert;
    String s1, s2;

    if (args.length != 2) {
      System.out.println("Please give input file name and database name");
      System.exit(1);
    }

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return; 

    // get access to database 
    HGDBInfo dbinfo; 
    try {
      dbinfo = new HGDBInfo("localhost", args[1]); 
    } catch (Exception e) { 
      System.out.println(e.toString()); 
      System.out.println("cannot connect to " + args[1]);
      return; 
    } 
    
    if (!dbinfo.validate()) return;
    try {
      String dbURL = QADBLibrary.jdbcURL(dbinfo);
      Connection con = DriverManager.getConnection(dbURL);
      Statement stmt = con.createStatement();
      fin = new FileReader(args[0]);
      br = new BufferedReader(fin);
      while ((thisline = br.readLine()) != null) {
        // System.out.println(thisline);
	int len = thisline.length();
        s2 = thisline.substring(1);
	if (len > 7) {
          s1 = thisline.substring(1,7);
	} else {
	  s1 = "not contig";
	}
	if (s1.equals("Contig")) {
	  if (firsttime) {
	    firsttime = false;
	  } else {
            System.out.println(contig);
	    insert = "insert into " + tablename; 
	    insert = insert + " (name, scores) values (\"" + contig + "\",\"\")"; 
	    // System.out.println(insert);
	    stmt.execute(insert);
	    QADBLibrary.blobInsert(con, scores, "scores", tablename, "name", contig);
            // System.out.println(scores);
	    scores = "";
	  }
	  contig = s2;
        } else {
	  scores = scores + thisline;
	}
      }
      fin.close();
      System.out.println(contig);
      insert = "insert into " + tablename; 
      insert = insert + " (name, scores) values (\"" + contig + "\",\"\")"; 
      // System.out.println(insert);
      stmt.execute(insert);
      QADBLibrary.blobInsert(con, scores, "scores", tablename, "name", contig);
      // System.out.println(scores);

      stmt.close();
      con.close();
    } catch (Exception e) {
      System.out.println("exception encountered");
      System.out.println(e.getMessage());
      System.exit(2);
    }

  }
}

