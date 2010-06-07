package edu.ucsc.genome.qa.cgiCheck;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.net.*;
import java.sql.*;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.Properties;

/** 
 *  2 parts:
 *  1) Holding sort option constant (Name Similarity), iterate through columns
 *  2) Holding column constant (Name), iterate through sort options
 *  Loops over all assemblies that have hgNearOk.
 *  For all pages viewed, check for non-200 return code.
 *  Also checks for HGERROR.  
 *  Also compares to expected results.  
 */
public class HGNearCheck {

 /**
  * Print usage info and exit
  */
 static void usage() {
    System.out.println(
      "HGNearCheck - automatic tests on hgNear cgi\n" +
      "usage:\n" +
      "   java HGNearCheck propertiesFile\n" +
      "       where properties files may contain database, table.\n" +
      "   java HGNearCheck default\n" +
      "      This will use the default properties\n"
      );
    System.exit(1);
 }

  public static void main(String[] args) {

    boolean debug = false;

    /* Process command line properties, and load them
     * into machine and table. */
    if (args.length != 1)
        usage();
    TestTarget target = new TestTarget(args[0]);

    // todo: determine gene names to test
    // test value for now
    String geneName = "HOXA9";

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
    
    // get read access to database
    HGDBInfo metadbinfo; 
    try {
      metadbinfo = new HGDBInfo("hgwbeta", "hgcentraltest");
    } catch (Exception e) {
      System.out.println(e.toString());
      return;
    }
    if (!metadbinfo.validate()) return;

    ArrayList assemblyList = Metadata.getHGNearAssemblies(metadbinfo);

    // iterate over assembly list
    // System.out.println("iterate over assembly list");
    Iterator assemblyIter = assemblyList.iterator();
    while (assemblyIter.hasNext()) {
      String assembly = (String) assemblyIter.next();
      if(!assembly.equals("hg16")) continue;

      try {
	// create HGDBInfo for this assembly
	// will use this for getting new gene names to search for
	HGDBInfo dbinfo = new HGDBInfo("localhost", assembly);
	if (!dbinfo.validate()) {
	  System.out.println("Cannot connect to database for " + assembly);
	  continue;
	}
        // initialize robot
        String hgNearURL = "http://" + target.machine + "/cgi-bin/hgNear?db=";
        hgNearURL = hgNearURL + assembly;
        Robot myRobot = new Robot(hgNearURL);
        // PART 1
        // set sort mode
        HgNear.setSort(myRobot.currentPage, "abc");
        // get column list for this assembly
        String hgNearConfig = hgNearURL + "&near.do.configure=configure";
        Robot configRobot = new Robot(hgNearConfig);
        // TO DO: hide all
        ArrayList cols = HgNear.getColumns(configRobot);
        // iterate through columns
        Iterator colIter = col.iterator();
        while (colIter.hasNext()) {
          String column = (String) colIter.next();
          System.out.println(column);
          HgNear.setColumn(configRobot);
          // iterate over gene searches
        }
        // PART 2
        // set column
        // iterate through sort modes
        // iterate over gene searches
      } catch (Exception e) {
        System.out.println(e.getMessage());
      }
      System.out.println();
      System.out.println();
    }
  }
}
