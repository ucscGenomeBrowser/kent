package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.net.*;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.regex.*;

import com.meterware.httpunit.*;

import org.xml.sax.*;

/**
 * Utilities for checking HgNear CGI using httpunit 
 *
 */
public class HgNear {

 /**
  * Set sort mode
  *
  * @param page     The active page
  * @param mode     The mode of sorting
  */
  public static void setSort(WebResponse page, String mode) throws Exception {

    System.out.println("HgNear: Setting sort order to " + mode);
    WebForm form = page.getFormWithName("mainForm");
    form.setParameter("near.order", mode);
    // WORKING HERE: click Go button
    // String paramNames[] = form.getParameterNames();

    // for (int i=0; i < paramNames.length; i++) {
      // System.out.println(paramNames[i]);
    // }
  }

 /**
  * Get column data
  *
  * @param robot   The current robot 
  */
  public static ArrayList getColumns(Robot robot) throws Exception {

    ArrayList columns = new ArrayList();
    // only one unnamed form on this page
    WebResponse page = robot.currentPage;
    WebForm form = page.getForms()[0];
    String paramNames[] = form.getParameterNames();

    for (int i=0; i < paramNames.length; i++) {
      String thisParam = paramNames[i];
      String prefix = thisParam.substring(0,5);
      if (prefix.equals("near.")) {
        thisParam = thisParam.substring(5);
        prefix = thisParam.substring(0,4);
        if (prefix.equals("col.")) {
          thisParam = thisParam.substring(4);
          int separatorIndex = thisParam.indexOf('.');
          String suffix = thisParam.substring(separatorIndex);
          if (suffix.equals(".vis")) {
            thisParam = thisParam.substring(0,separatorIndex);
            columns.add(thisParam);
          }
        }
      }
    }

    return columns;
  }

 /** 
  * Set column
  *
  * @param  The current robot
  */

  public static void setColumn(Robot robot, String column) throws Exception {
    // only one unnamed form on this page
    WebResponse page = robot.currentPage;
    WebForm form = page.getForms()[0];
    String fullParam = "near.col." + column + ".vis";
    form.setParameter(fullParam, "CHECKED");
    // click submit
    // update robot

  }

}
