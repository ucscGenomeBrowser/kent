package edu.ucsc.genome.util;
import edu.ucsc.genome.qa.lib.*;
import java.sql.*;
import java.util.ArrayList;
import java.util.Iterator;


class ScaffoldQuality {
  public static void main(String args[]) {

    ArrayList contigs;
    Contig contig;
    Scaffold scaffold;

    if (args.length != 2) {
      System.out.println("Please give database name and assembly table name");
      System.exit(1);
    }

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return; 

    // get access to database 
    HGDBInfo dbinfo; 
    try {
      dbinfo = new HGDBInfo("localhost", args[0]); 
    } catch (Exception e) { 
      System.out.println(e.toString()); 
      System.out.println("cannot connect to " + args[0]);
      return; 
    } 
    
    if (!dbinfo.validate()) return;
    try {
      String dbURL = QADBLibrary.jdbcURL(dbinfo);
      Connection con = DriverManager.getConnection(dbURL);

      ArrayList scaffolds = QADBLibrary.readGold(dbinfo, args[1]);

      // iterate through scaffolds
      Iterator scaffoldIter = scaffolds.iterator();
      while (scaffoldIter.hasNext()) {
        scaffold = (Scaffold) scaffoldIter.next();
	// scaffold.print();
	contigs = scaffold.contigs;

	// iterate through contigs
	Iterator contigIter = contigs.iterator();
	ArrayList scoreArray = new ArrayList();
	while (contigIter.hasNext()) {
	  contig = (Contig) contigIter.next();
          Statement stmt2 = con.createStatement();
          scoreArray = QADBLibrary.getScores(contig, "contigQuality", stmt2);
	  int pos = contig.chromStart + 1;
	  System.out.println("fixedStep chrom=" + scaffold.name + " start=" + pos);
	  // could check here if extra scores getting throw away
	  // LoadQuality padded with extra zeroes if necessary
	  for (int k = contig.fragStart; k < contig.fragEnd; k++)
	    System.out.println(scoreArray.get(k));
	}

      }
    } catch (Exception e) { 
      System.out.println(e.toString()); 
      System.out.println("cannot connect to " + args[1]);
      return; 
    } 
  }
}

