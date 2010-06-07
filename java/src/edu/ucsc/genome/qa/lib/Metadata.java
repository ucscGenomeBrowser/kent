package edu.ucsc.genome.qa.lib;
import java.util.ArrayList;

/** 
 * 
 *  utilities for accessing metadata
 */
public class Metadata {

 /** 
  *  Get assemblies that use hgNear.
  * 
  *  @return   ArrayList of strings of assembly names
  */
  public static ArrayList getHGNearAssemblies(HGDBInfo dbinfo) {
    ArrayList assemblies = new ArrayList();
    assemblies = 
      QADBLibrary.getColumnMatchingCondition(dbinfo, "dbDb", "name",
                                             "hgNearOk = 1");
    return assemblies;
  }
}
