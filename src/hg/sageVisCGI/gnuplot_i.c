
/*----------------------------------------------------------------------------
   
   File name    :   gnuplot_i.c
   Author       :   N. Devillard
   Created on   :   Fri Sept 26 1997
   Description  :   C interface to gnuplot
  
   gnuplot is a freely available, command-driven graphical display tool for
   Unix. It compiles and works quite well on a number of Unix flavours as
   well as other operating systems. The following module enables sending
   display requests to gnuplot through simple C calls.
  
 ---------------------------------------------------------------------------*/

/*
	$Id: gnuplot_i.c,v 1.1 2005/10/20 23:52:24 hiram Exp $
	$Author: hiram $
	$Date: 2005/10/20 23:52:24 $
	$Revision: 1.1 $
 */

/*---------------------------------------------------------------------------
                                Includes
 ---------------------------------------------------------------------------*/

#include "gnuplot_i.h"

/*---------------------------------------------------------------------------
                                Defines
 ---------------------------------------------------------------------------*/

/* Maximal size of a gnuplot command */
#define GP_CMD_SIZE     	2048
/* Maximal size of a plot title */
#define GP_TITLE_SIZE   	80
/* Maximal size for an equation */
#define GP_EQ_SIZE      	512


/*---------------------------------------------------------------------------
                            Function codes
 ---------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------*/
/**
  @name		check_X_display
  @memo		Checks out if the DISPLAY environment variable is set.
  @param	activate int flag
  @return	int 1 if the variable is set, 0 otherwise.
  @doc

  This function checks out the DISPLAY environment variable to see if
  it exists. It does not check if the display is actually correctly
  configured. If you do not want to activate this check (e.g. on
  systems that do not support this kind of display mechanism), pass a
  0 integer as the activate flag. Any other value will activate it.
 */
/*--------------------------------------------------------------------------*/

int check_X_display(int activate)
{
    char    *   display ;
    
	if (!activate) return 1 ;

    display = getenv("DISPLAY") ;
    if (display == NULL) {
        fprintf(stderr, "cannot find DISPLAY variable: is it set?\n") ;
        return 1 ;
    } else {
        return 0 ;
    }
}


/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_get_program_path
  @memo		Find out where a command lives in your PATH.
  @param	pname Name of the program to look for.
  @return	pointer to statically allocated character string.
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

#define MAXNAMESZ       4096
char * gnuplot_get_program_path(char * pname)
{
    int         i, j, lg;
    char    *   path;
    static char buf[MAXNAMESZ];

    /* Trivial case: try in CWD */
    sprintf(buf, "./%s", pname) ;
    if (access(buf, X_OK)==0) {
        sprintf(buf, ".");
        return buf ;
    }
    /* Try out in all paths given in the PATH variable */
    buf[0] = 0;
    path = getenv("PATH") ;
    if (path!=NULL) {
        for (i=0; path[i]; ) {
            for (j=i ; (path[j]) && (path[j]!=':') ; j++);
            lg = j - i;
            strncpy(buf, path + i, lg);
            if (lg == 0) buf[lg++] = '.';
            buf[lg++] = '/';
            strcpy(buf + lg, pname);
            if (access(buf, X_OK) == 0) {
                /* Found it! */
                break ;
            }
            buf[0] = 0;
            i = j;
            if (path[i] == ':') i++ ;
        }
    } else {
		fprintf(stderr, "PATH variable not set\n");
	}
    /* If the buffer is still empty, the command was not found */
    if (buf[0] == 0) return NULL ;
    /* Otherwise truncate the command name to yield path only */
    lg = strlen(buf) - 1 ;
    while (buf[lg]!='/') {
        buf[lg]=0 ;
        lg -- ;
    }
    buf[lg] = 0;
    return buf ;
}
#undef MAXNAMESZ



/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_init
  @memo		Opens up a gnuplot session, ready to receive commands.
  @return	Newly allocated gnuplot control structure.
  @doc

  This opens up a new gnuplot session, ready for input. The struct
  controlling a gnuplot session should remain opaque and only be
  accessed through the provided functions.
 */
/*--------------------------------------------------------------------------*/

gnuplot_ctrl * gnuplot_init(void)
{
    gnuplot_ctrl *  handle ;

    if (check_X_display(1)) return NULL ;
	if (gnuplot_get_program_path("gnuplot")==NULL) {
		fprintf(stderr, "cannot find gnuplot in your PATH");
		return NULL ;
	}

    /* 
     * Structure initialization:
     */
    handle = malloc(sizeof(gnuplot_ctrl)) ;
    handle->nplots = 0 ;
    gnuplot_setstyle(handle, "points") ;
    handle->ntmp = 0 ;

    handle->gnucmd = popen("gnuplot", "w") ;
    if (handle->gnucmd == NULL) {
        fprintf(stderr, "error starting gnuplot\n") ;
        free(handle) ;
        return NULL ;
    }
    return handle;
}


/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_close
  @memo		Closes a gnuplot session previously opened by gnuplot_init()
  @param	handle Gnuplot session control handle.
  @return	void
  @doc

  Kills the child PID and deletes all opened temporary files.
  It is mandatory to call this function to close the handle, otherwise
  temporary files are not cleaned and child process might survive.

 */
/*--------------------------------------------------------------------------*/

void gnuplot_close(gnuplot_ctrl * handle)
{
    int     i ;
	
    if (check_X_display(1)) return ;
    if (handle->ntmp) {
        for (i=0 ; i<handle->ntmp ; i++) {
            remove(handle->to_delete[i]) ;
        }
    }
    if (pclose(handle->gnucmd) == -1) {
        fprintf(stderr, "problem closing communication to gnuplot\n") ;
        return ;
    }
    free(handle) ;
    return ;
}


/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_cmd
  @memo		Sends a command to an active gnuplot session.
  @param	handle Gnuplot session control handle
  @param	cmd    Command to send, same as a printf statement.
  @return	void
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

void gnuplot_cmd(gnuplot_ctrl *  handle, char *  cmd, ...)
{
    va_list ap ;
    char    local_cmd[GP_CMD_SIZE];

    va_start(ap, cmd);
    vsprintf(local_cmd, cmd, ap);
    va_end(ap);

    strcat(local_cmd, "\n");

    fputs(local_cmd, handle->gnucmd) ;
    fflush(handle->gnucmd) ;
    return ;
}


/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_setstyle
  @memo		Change the plotting style of a gnuplot session.
  @param	h Gnuplot session control handle
  @param	plot_style Plotting-style to use (character string)
  @return	void
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

void gnuplot_setstyle(gnuplot_ctrl * h, char * plot_style) 
{
    if (strcmp(plot_style, "lines") &&
        strcmp(plot_style, "points") &&
        strcmp(plot_style, "linespoints") &&
        strcmp(plot_style, "impulses") &&
        strcmp(plot_style, "dots") &&
        strcmp(plot_style, "steps") &&
        strcmp(plot_style, "errorbars") &&
        strcmp(plot_style, "boxes") &&
        strcmp(plot_style, "boxerrorbars")) {
        fprintf(stderr, "warning: unknown requested style: using points\n") ;
        strcpy(h->pstyle, "points") ;
    } else {
        strcpy(h->pstyle, plot_style) ;
    }
    return ;
}


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

void gnuplot_set_xlabel(gnuplot_ctrl * h, char * label)
{
    char    cmd[GP_CMD_SIZE] ;

    sprintf(cmd, "set xlabel \"%s\"", label) ;
    gnuplot_cmd(h, cmd) ;
    return ;
}


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

void gnuplot_set_ylabel(gnuplot_ctrl * h, char * label)
{
    char    cmd[GP_CMD_SIZE] ;

    sprintf(cmd, "set ylabel \"%s\"", label) ;
    gnuplot_cmd(h, cmd) ;
    return ;
}


/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_resetplot
  @memo		Resets a gnuplot session (next plot will erase previous ones).
  @param	h Gnuplot session control handle.
  @return	void
  @doc

  Resets a gnuplot session, i.e. the next plot will erase all previous
  ones.
 */
/*--------------------------------------------------------------------------*/

void gnuplot_resetplot(gnuplot_ctrl * h)
{
    int     i ;
    if (h->ntmp) {
        for (i=0 ; i<h->ntmp ; i++) {
            remove(h->to_delete[i]) ;
        }
    }
    h->ntmp = 0 ;
    h->nplots = 0 ;
    return ;
}



/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_plot_x
  @memo		Plots a 2d graph from a list of doubles.
  @param	handle	Gnuplot session control handle.
  @param	d		Array of doubles.
  @param	n		Number of values in the passed array.
  @param	title	Title of the plot.
  @return	void
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
)
{
    int         i ;
    FILE    *   tmp ;
    char    *   name ;
    char        cmd[GP_CMD_SIZE] ;
    char        line[GP_CMD_SIZE] ;


	if (handle==NULL || d==NULL || (n<1)) return ;

    /* can we open one more temporary file? */
    if (handle->ntmp == GP_MAX_TMP_FILES - 1) {
        fprintf(stderr,
                "maximum # of temporary files reached (%d): cannot open more",
                GP_MAX_TMP_FILES) ;
        return ;
    }

    /* Open temporary file for output   */
    if ((name = tmpnam(NULL)) == NULL) {
        fprintf(stderr,"cannot create temporary file: exiting plot") ;
        return ;
    }
    if ((tmp = fopen(name, "w")) == NULL) {
        fprintf(stderr, "cannot create temporary file: exiting plot") ;
        return ;
    }

    /* Store file name in array for future deletion */
    strcpy(handle->to_delete[handle->ntmp], name) ;
    handle->ntmp ++ ;

    /* Write data to this file  */
    for (i=0 ; i<n ; i++) {
        fprintf(tmp, "%g\n", d[i]) ;
    }
    fflush(tmp) ;
    fclose(tmp) ;

    /* Command to be sent to gnuplot    */
    if (handle->nplots > 0) {
        strcpy(cmd, "replot") ;
    } else {
        strcpy(cmd, "plot") ;
    }
    
    if (title == NULL) {
        sprintf(line, "%s \"%s\" with %s", cmd, name, handle->pstyle) ;
    } else {
        sprintf(line, "%s \"%s\" title \"%s\" with %s", cmd, name,
                      title, handle->pstyle) ;
    }

    /* send command to gnuplot  */
    gnuplot_cmd(handle, line) ;
    handle->nplots++ ;
    return ;
}



/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_plot_xy
  @memo		Plot a 2d graph from a list of points.
  @param	handle		Gnuplot session control handle.
  @param	x			Pointer to a list of x coordinates.
  @param	y			Pointer to a list of y coordinates.
  @param	n			Number of doubles in x (assumed the same as in y).
  @param	title		Title of the plot.
  @return	void
  @doc

  Plots out a 2d graph from a list of points. Provide points through a list
  of x and a list of y coordinates. Both provided arrays are assumed to
  contain the same number of values.

  \begin{verbatim}
    gnuplot_ctrl    *h ;
	double			x[50] ;
	double			y[50] ;
    int             i ;

    h = gnuplot_init() ;
    for (i=0 ; i<50 ; i++) {
        x[i] = (double)(i)/10.0 ;
        y[i] = x[i] * x[i] ;
    }
    gnuplot_plot1_xy(h, x, y, 50, "parabola") ;
    sleep(2) ;
    gnuplot_close(h) ;
  \end{verbatim}
 */
/*--------------------------------------------------------------------------*/

void gnuplot_plot_xy(
    gnuplot_ctrl    *   handle,
	double			*	x,
	double			*	y,
    int                 n,
    char            *   title
)
{
    int         i ;
    FILE    *   tmp ;
    char    *   name ;
    char        cmd[GP_CMD_SIZE] ;
    char        line[GP_CMD_SIZE] ;

	if (handle==NULL || x==NULL || y==NULL || (n<1)) return ;

    /* can we open one more temporary file? */
    if (handle->ntmp == GP_MAX_TMP_FILES - 1) {
        fprintf(stderr,
                "maximum # of temporary files reached (%d): cannot open more",
                GP_MAX_TMP_FILES) ;
        return ;
    }

    /* Open temporary file for output   */
    if ((name = tmpnam(NULL)) == (char*)NULL) {
        fprintf(stderr,"cannot create temporary file: exiting plot") ;
        return ;
    }
    if ((tmp = fopen(name, "w")) == NULL) {
        fprintf(stderr,"cannot create temporary file: exiting plot") ;
        return ;
    }

    /* Store file name in array for future deletion */
    strcpy(handle->to_delete[handle->ntmp], name) ;
    handle->ntmp ++ ;

    /* Write data to this file  */
    for (i=0 ; i<n; i++) {
        fprintf(tmp, "%g %g\n", x[i], y[i]) ;
    }
    fflush(tmp) ;
    fclose(tmp) ;

    /* Command to be sent to gnuplot    */
    if (handle->nplots > 0) {
        strcpy(cmd, "replot") ;
    } else {
        strcpy(cmd, "plot") ;
    }
    
    if (title == NULL) {
        sprintf(line, "%s \"%s\" with %s", cmd, name, handle->pstyle) ;
    } else {
        sprintf(line, "%s \"%s\" title \"%s\" with %s", cmd, name,
                      title, handle->pstyle) ;
    }

    /* send command to gnuplot  */
    gnuplot_cmd(handle, line) ;
    handle->nplots++ ;
    return ;
}



/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_plot_once
  @memo		Open a new session, plot a signal, close the session.
  @param	title	Plot title
  @param	style	Plot style
  @param	label_x	Label for X
  @param	label_y	Label for Y
  @param	x		Array of X coordinates
  @param	y		Array of Y coordinates (can be NULL)
  @param	n		Number of values in x and y.
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
	char	*	title,
	char	*	style,
	char	*	label_x,
	char	*	label_y,
	double	*	x,
	double	*	y,
	int			n
)
{
	gnuplot_ctrl	*	handle ;

	if (x==NULL || n<1) return ;

	handle = gnuplot_init();
	if (style!=NULL) {
		gnuplot_setstyle(handle, style);
	} else {
		gnuplot_setstyle(handle, "lines");
	}
	if (label_x!=NULL) {
		gnuplot_set_xlabel(handle, label_x);
	} else {
		gnuplot_set_xlabel(handle, "X");
	}
	if (label_y!=NULL) {
		gnuplot_set_ylabel(handle, label_y);
	} else {
		gnuplot_set_ylabel(handle, "Y");
	}
	if (y==NULL) {
		gnuplot_plot_x(handle, x, n, title);
	} else {
		gnuplot_plot_xy(handle, x, y, n, title);
	}
	printf("press ENTER to continue\n");
	while (getchar()!='\n') {}
	gnuplot_close(handle);
	return ;
}




/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_plot_slope
  @memo		Plot a slope on a gnuplot session.
  @param	handle		Gnuplot session control handle.
  @param	a			Slope.
  @param	b			Intercept.
  @param	title		Title of the plot.
  @return	void
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
)
{
    char    stitle[GP_TITLE_SIZE] ;
    char    cmd[GP_CMD_SIZE] ;

    if (title == NULL) {
        strcpy(stitle, "no title") ;
    } else {
        strcpy(stitle, title) ;
    }

    if (handle->nplots > 0) {
        sprintf(cmd, "replot %g * x + %g title \"%s\" with %s",
                      a, b, title, handle->pstyle) ;
    } else {
        sprintf(cmd, "plot %g * x + %g title \"%s\" with %s",
                      a, b, title, handle->pstyle) ;
    }
    gnuplot_cmd(handle, cmd) ;
    handle->nplots++ ;
    return ;
}


/*-------------------------------------------------------------------------*/
/**
  @name		gnuplot_plot_equation
  @memo		Plot a curve of given equation y=f(x).
  @param	h			Gnuplot session control handle.
  @param	equation	Equation to plot.
  @param	title		Title of the plot.
  @return	void
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
)
{
    char    cmd[GP_CMD_SIZE];
    char    plot_str[GP_EQ_SIZE] ;
    char    title_str[GP_TITLE_SIZE] ;

    if (title == NULL) {
        strcpy(title_str, "no title") ;
    } else {
        strcpy(title_str, title) ;
    }
    if (h->nplots > 0) {
        strcpy(plot_str, "replot") ;
    } else {
        strcpy(plot_str, "plot") ;
    }

    sprintf(cmd, "%s %s title \"%s\" with %s", 
                  plot_str, equation, title_str, h->pstyle) ;
    gnuplot_cmd(h, cmd) ;
    h->nplots++ ;
    return ;
}

