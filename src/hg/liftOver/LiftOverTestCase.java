import java.io.*;
import java.util.*;

/* This container will hold the rows from the liftOverTestCase table */

class LiftOverTestCase {

  // data
  String id;
  String comment;
  String origAssembly;
  String newAssembly;
  String origChrom;
  int origStart;
  int origEnd;
  String status;
  String message;
  String newChrom;
  int newStart;
  int newEnd;

  // constructors
  LiftOverTestCase(String idVar, String commentVar, 
            String origAssemblyVar, String newAssemblyVar,
            String origChromVar, int origStartVar, int origEndVar,
	    String statusVar, String messageVar, String newChromVar,
	    int newStartVar, int newEndVar) {

    id = idVar;
    comment = commentVar;
    origAssembly = origAssemblyVar;
    newAssembly = newAssemblyVar;
    origChrom = origChromVar;
    origStart = origStartVar;
    origEnd = origEndVar;
    status = statusVar;
    message = messageVar;
    newChrom = newChromVar;
    newStart = newStartVar;
    newEnd = newEndVar;
  }

}
