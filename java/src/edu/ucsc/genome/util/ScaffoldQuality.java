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
	int expectedPos = 0;
	System.out.println("fixedStep chrom=" + scaffold.name + " start=1");
	contigs = scaffold.contigs;
	// handle starting gaps
	if (scaffold.startPos > expectedPos) {
	  for (int i = 0; i < expectedPos; i++) 
	    System.out.println("0");
        }
	expectedPos = scaffold.startPos;

	// iterate through contigs
	// store scores in ArrayList
	Iterator contigIter = contigs.iterator();
	ArrayList scoreArray = new ArrayList();
	while (contigIter.hasNext()) {
	  contig = (Contig) contigIter.next();
	  // handle gaps
	  if (contig.chromStart > expectedPos) {
	    for (int j = expectedPos; j <= contig.chromStart; j++)
	      System.out.println("0");
	  }

	  expectedPos = contig.chromEnd + 1;
          Statement stmt2 = con.createStatement();
          scoreArray = QADBLibrary.getScores(contig, "contigQuality", stmt2);

	  // print
	  for (int k = contig.fragStart; k < contig.fragEnd; k++)
	    System.out.println(scoreArray.get(k));
	  expectedPos = contig.chromEnd + 1;
	}

	// this is where I would check for ending gaps?
	// to do that, would need to read gap table
	// if (expectedPos < scaffold.endPos)
	  // for (int k = expectedPos; k < scaffold.endPos; k++)
	    // System.out.println("0");
      }
    } catch (Exception e) { 
      System.out.println(e.toString()); 
      System.out.println("cannot connect to " + args[1]);
      return; 
    } 
  }
}

