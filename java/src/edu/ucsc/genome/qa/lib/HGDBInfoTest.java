package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.net.*;
import java.sql.*;
import java.util.Properties;

/**
 *  Checks for validity of database connection and metadatabase info.
 */
public class HGDBInfoTest {

 /**
  *  Runs program to check for database validity.
  */
  public static void main(String[] args) {

    // make sure CLASSPATH has been set for JDBC driver
    if (!QADBLibrary.checkDriver()) return;
    
    // get read access to database
    HgConf hgConf;
    HGDBInfo dbinfo; 
    String db = "pyrAer1";
    try {
      dbinfo = new HGDBInfo("hgwbeta", db);
    } catch (Exception e) {
      System.out.println(e.toString());
      return;
    }
    if (dbinfo.validate()) 
        System.out.println(db + " is valid");
     else
        System.out.println(db + " is not valid");
  }
}
