package edu.ucsc.genome.qa.cgiCheck;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.net.*;
import java.sql.*;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.Properties;


 // Heather Trumbower & Robert Kuhn
 // Modified from SGDGeneCheck.java
 // 01-09-04

/** 
 *  Works out Proteome Browser.
 *  For all rows in knownGene, view details page.
 *  For all pages viewed, check for non-200 return code.
 *  Doesn't click into any links.
 *  Doesn't check for HGERROR.  
 *  Prints to .ok, .error files via HgTracks.pbgene.
 */
public class PBGeneCheck {

 /**
  * Print usage info and exit
  */
 static void usage() {
    System.out.println(
      "PBGeneCheck - do some basic automatic tests on pbTracks cgi\n" +
      "usage:\n" +
      "   java HGGeneCheck propertiesFile\n" +
      "where properties files may contain machine, database, quick." +
      "   java HGGeneCheck default\n" +
      "This will use the default properties\n"
      );
    System.exit(-1);
 }

 /** 
  * Checks all Known Genes for successful links in Proteome Browser.
  *
  */
  public static void main(String[] args) {

    boolean debug = false;
    if (args.length != 1)
        usage();
    TestTarget target = new TestTarget(args[0]);

    String table    = "knownGene";

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
    
    // get read access to database
    HGDBInfo metadbinfo, dbinfo; 
    try {
      metadbinfo = new HGDBInfo("hgwbeta", "hgcentraltest");
      dbinfo = new HGDBInfo(target.machine, target.dbSpec);
    } catch (Exception e) {
      System.out.println(e.toString());
      return;
    }
    if (!metadbinfo.validate()) return;

    if (!dbinfo.validate()) {
      System.out.println("Cannot connect to database for " + target.dbSpec);
      return;
    }

    HgTracks.pbgene(dbinfo, target.machine, target.dbSpec, table, target.quickOn);
  }
}
