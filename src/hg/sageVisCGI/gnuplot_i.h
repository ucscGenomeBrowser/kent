
/*----------------------------------------------------------------------------
 
   File name    :   gnuplot_i.h
   Author       :   N. Devillard
   Created on   :   Fri Sept 26 1997
   Description  :   C interface to gnuplot
  
   gnuplot is a freely available, command-driven graphical display tool for
   Unix. It compiles and works quite well on a number of Unix flavours as
   well as other operating systems. The following module enables sending
   display requests to gnuplot through simple C calls.

 ---------------------------------------------------------------------------*/

/*
	$Id: gnuplot_i.h,v 1.1 2005/10/20 23:52:24 hiram Exp $
	$Author: hiram $
	$Date: 2005/10/20 23:52:24 $
	$Revision: 1.1 $
 */

#ifndef _GNUPLOT_PIPES_H_
#define _GNUPLOT_PIPES_H_

/*---------------------------------------------------------------------------
                                Includes
 ---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>


/* Maximal number of simultaneous temporary files */
#define GP_MAX_TMP_FILES    64
/* Maximal size of a temporary file name */
#define GP_TMP_NAME_SIZE    512


/*---------------------------------------------------------------------------
                                New Types
 ---------------------------------------------------------------------------*/

/*
 * This structure holds all necessary information to talk to a gnuplot
 * session.
 */

typedef struct _GNUPLOT_CTRL_ {
    /* command file handling */
    FILE    * gnucmd ;
    
    /* Plotting options */
    int       nplots ;      /* Number of active plots at the moment */
    char      pstyle[32] ;  /* Current plotting style */

    /* temporary files opened */
    char      to_delete[GP_MAX_TMP_FILES][GP_TMP_NAME_SIZE] ;
    int       ntmp ;


} gnuplot_ctrl ;

/*---------------------------------------------------------------------------
                        Function ANSI C prototypes
 ---------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/**
  @name     check_X_display
  @memo     Checks out if the DISPLAY environment variable is set.
  @param    activate int flag
  @return   int 1 if the variable is set, 0 otherwise.
  @doc
 
  This function checks out the DISPLAY environment variable to see if
  it exists. It does not check if the display is actually correctly
  configured. If you do not want to activate this check (e.g. on
  systems that do not support this kind of display mechanism), pass a
  0 integer as the activate flag. Any other value will activate it.
 */
/*--------------------------------------------------------------------------*/
 
int check_X_display(int activate);


/*-------------------------------------------------------------------------*/
/**
  @name     gnuplot_get_program_path
  @memo     Find out where a command lives in your PATH.
  @param    pname Name of the program to look for.
  @return   pointer to statically allocated character string.
  @doc
 
  This is the C equivalent to the 'which' command in Unix. It parses
  out your PATH environment variable to find out where a command
  lives. The returned character string is statically allocated within
  this function, i.e. there is no need to free it. Beware that the
  contents of this string will change from one call to the next,
  though (as all static variables in a function).
 
  The input character string must be the name of a command without
  prefixing path of any kind, i.e. only the command name. The returned
  string is the path in which a command matching the same name was
  found.
 
  Examples (assuming there is a prog named 'hello' in the cwd):
 
  \begin{itemize}
  \item gnuplot_get_program_path("hello") returns "."
  \item gnuplot_get_program_path("ls") returns "/bin"
  \item gnuplot_get_program_path("csh") returns "/usr/bin"
  \item gnuplot_get_program_path("/bin/ls") returns NULL
  \end{itemize}
 
 */
/*-------------------------------------------------------------------------*/

char * gnuplot_get_program_path(char * pname);

/*-------------------------------------------------------------------------*/
/**
  @name     gnuplot_init
  @memo     Opens up a gnuplot session, ready to receive commands.
  @return   Newly allocated gnuplot control structure.
  @doc
 
  This opens up a new gnuplot session, ready for input. The struct
  controlling a gnuplot session should remain opaque and only be
  accessed through the provided functions.
 */
/*--------------------------------------------------------------------------*/
 
gnuplot_ctrl * gnuplot_init(void);


/*-------------------------------------------------------------------------*/
/**
  @name     gnuplot_close
  @memo     Closes a gnuplot session previously opened by gnuplot_init()
  @param    handle Gnuplot session control handle.
  @return   void
  @doc
 
  Kills the child PID and deletes all opened temporary files.
  It is mandatory to call this function to close the handle, otherwise
  temporary files are not cleaned and child process might survive.
 
 */
/*--------------------------------------------------------------------------*/
 
void gnuplot_close(gnuplot_ctrl * handle);


/*-------------------------------------------------------------------------*/
/**
  @name     gnuplot_cmd
  @memo     Sends a command to an active gnuplot session.
  @param    handle Gnuplot session control handle
  @param    cmd    Command to send, same as a printf statement.
  @return   void
  @doc
 
  This sends a string to an active gnuplot session, to be executed.
  There is strictly no way to know if the command has been
  successfully executed or not.
  The command syntax is the same as printf.
 
  Examples:
 
  \begin{itemize}
  \item gnuplot_cmd(g, "plot %d*x", 23.0);
  \item gnuplot_cmd(g, "plot %g * cos(%g * x)", 32.0, -3.0);
  \end{itemize}
 
 */
/*--------------------------------------------------------------------------*/
 
void gnuplot_cmd(gnuplot_ctrl *  handle, char *  cmd, ...);


/*-------------------------------------------------------------------------*/
/**
  @name     gnuplot_setstyle
  @memo     Change the plotting style of a gnuplot session.
  @param    h Gnuplot session control handle
  @param    plot_style Plotting-style to use (character string)
  @return   void
  @doc
 
  The provided plotting style is a character string. It must be one of
  the following:
 
  \begin{itemize}
  \item {\it lines}
  \item {\it points}
  \item {\it linespoints}
  \item {\it impulses}
  \item {\it dots}
  \item {\it steps}
  \item {\it errorbars}
  \item {\it boxes}
  \item {\it boxeserrorbars}
  \end{itemize}
 */
/*--------------------------------------------------------------------------*/
 
void gnuplot_setstyle(gnuplot_ctrl * h, char * plot_style);


/*-------------------------------------------------------------------------*/
/**
  @name     gnuplot_resetplot
  @memo     Resets a gnuplot session (next plot will erase previous ones).
  @param    h Gnuplot session control handle.
  @return   void
  @doc
 
  Resets a gnuplot session, i.e. the next plot will erase all previous
  ones.
 */
/*--------------------------------------------------------------------------*/
 
void gnuplot_resetplot(gnuplot_ctrl * h);


/*-------------------------------------------------------------------------*/
/**
  @name     gnuplot_plot_x
  @memo     Plots a 2d graph from a list of doubles.
  @param    handle  Gnuplot session control handle.
  @param    x       Array of doubles.
  @param    n       Number of values in the passed array.
  @param    title   Title of the plot.
  @return   void
  @doc
 
  Plots out a 2d graph from a list of doubles. The x-coordinate is the
  index of the double in the list, the y coordinate is the double in
  the list.
 
  Example:
 
  \begin{verbatim}
    gnuplot_ctrl    *h ;
    double          d[50] ;
    int             i ;
 
    h = gnuplot_init() ;
    for (i=0 ; i<50 ; i++) {
        d[i] = (double)(i*i) ;
    }
    gnuplot_plot_x(h, d, 50, "parabola") ;
    sleep(2) ;
    gnuplot_close(h) ;
  \end{verbatim}
 
 
 */
/*--------------------------------------------------------------------------*/
 
void gnuplot_plot_x(
    gnuplot_ctrl    *   handle,
    double          *   d,
    int                 n,
    char            *   title
);


/*-------------------------------------------------------------------------*/
/**
  @name     gnuplot_plot_xy
  @memo     Plot a 2d graph from a list of points.
  @param    handle      Gnuplot session control handle.
  @param    x           Pointer to a list of x coordinates.
  @param    y           Pointer to a list of y coordinates.
  @param    n           Number of doubles in x (assumed the same as in y).
  @param    title       Title of the plot.
  @return   void
  @doc
 
  Plots out a 2d graph from a list of points. Provide points through a list
  of x and a list of y coordinates. Both provided arrays are assumed to
  contain the same number of values.
 
  \begin{verbatim}
    gnuplot_ctrl    *h ;
    double          x[50] ;
    double          y[50] ;
    int             i ;
 
    h = gnuplot_init() ;
    for (i=0 ; i<50 ; i++) {
        x[i] = (double)(i)/10.0 ;
        y[i] = x[i] * x[i] ;
    }
    gnuplot_plot_xy(h, x, y, 50, "parabola") ;
    sleep(2) ;
    gnuplot_close(h) ;
  \end{verbatim}
 */
/*--------------------------------------------------------------------------*/
 
void gnuplot_plot_xy(
    gnuplot_ctrl    *   handle,
    double          *   x,
    double          *   y,
    int                 n,
    char            *   title
) ;


/*-------------------------------------------------------------------------*/
/**
  @name     gnuplot_plot_once
  @memo     Open a new session, plot a signal, close the session.
  @param    title   Plot title
  @param    style   Plot style
  @param    label_x Label for X
  @param    label_y Label for Y
  @param    x       Array of X coordinates
  @param    y       Array of Y coordinates (can be NULL)
  @param    n       Number of values in x and y.
  @return
  @doc

  This function opens a new gnuplot session, plots the provided
  signal as an X or XY signal depending on a provided y, waits for
  a carriage return on stdin and closes the session.

  It is Ok to provide an empty title, empty style, or empty labels for
  X and Y. Defaults are provided in this case.
 */
/*--------------------------------------------------------------------------*/

void gnuplot_plot_once(
    char    *   title,
    char    *   style,
    char    *   label_x,
    char    *   label_y,
    double  *   x,
    double  *   y,
    int         n
);


/*-------------------------------------------------------------------------*/
/**
  @name     gnuplot_plot_slope
  @memo     Plot a slope on a gnuplot session.
  @param    handle      Gnuplot session control handle.
  @param    a           Slope.
  @param    b           Intercept.
  @param    title       Title of the plot.
  @return   void
  @doc
 
  Plot a slope on a gnuplot session. The provided slope has an
  equation of the form:
 
  \begin{verbatim}
  y = ax+b
  \end{verbatim}
 
  Example:
 
  \begin{verbatim}
    gnuplot_ctrl    *   h ;
    double              a, b ;
 
    h = gnuplot_init() ;
    gnuplot_plot_slope(h, 1.0, 0.0, "unity slope") ;
    sleep(2) ;
    gnuplot_close(h) ;
  \end{verbatim}
 
 */
/*--------------------------------------------------------------------------*/
 
 
void gnuplot_plot_slope(
    gnuplot_ctrl    *   handle,
    double              a,
    double              b,
    char            *   title
) ;


/*-------------------------------------------------------------------------*/
/**
  @name     gnuplot_plot_equation
  @memo     Plot a curve of given equation y=f(x).
  @param    h           Gnuplot session control handle.
  @param    equation    Equation to plot.
  @param    title       Title of the plot.
  @return   void
  @doc
 
  Plots out a curve of given equation. The general form of the
  equation is y=f(x), you only provide the f(x) side of the equation.
 
  Example:
 
  \begin{verbatim}
        gnuplot_ctrl    *h ;
        char            eq[80] ;
 
        h = gnuplot_init() ;
        strcpy(eq, "sin(x) * cos(2*x)") ;
        gnuplot_plot_equation(h, eq, "sine wave", normal) ;
        gnuplot_close(h) ;
  \end{verbatim}
 
 */
/*--------------------------------------------------------------------------*/
 
void gnuplot_plot_equation(
    gnuplot_ctrl    *   h,
    char            *   equation,
    char            *   title
) ;


/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_set_xlabel
  @memo		Sets the x label of a gnuplot session.
  @param	h Gnuplot session control handle.
  @param	label Character string to use for X label.
  @return	void
  @doc

  Sets the x label for a gnuplot session.
 */
/*--------------------------------------------------------------------------*/

void gnuplot_set_xlabel(gnuplot_ctrl * h, char * label);


/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_set_ylabel
  @memo		Sets the y label of a gnuplot session.
  @param	h Gnuplot session control handle.
  @param	label Character string to use for Y label.
  @return	void
  @doc

  Sets the y label for a gnuplot session.
 */
/*--------------------------------------------------------------------------*/

void gnuplot_set_ylabel(gnuplot_ctrl * h, char * label);

#endif
