/*************************************************************************
 *
 * Copyright (c) 1998 by Bjorn Reese <breese@mail1.stofanet.dk>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS AND
 * CONTRIBUTORS ACCEPT NO RESPONSIBILITY IN ANY CONCEIVABLE MANNER.
 *
 ************************************************************************
 *
 * 2008/12/28 - JMP
 *  - Modified to avoid '(gdb) Hangup detected on fd 0' 
 *    by getting GDB commands from file
 * 
 * 1999/08/19 - breese
 *  - Cleaned up the interface
 *
 * 1999/08/06 - breese
 *  - Added U_STACK_TRACE for HP/UX
 *
 * 1999/01/24 - breese
 *  - Added GCC_DumpStack
 *
 * 1998/12/21 - breese
 *  - Fixed include files and arguments for waitpid()
 *  - Made an AIX workaround for waitpid() exiting with EINTR
 *  - Added a missing 'quit' command for OSF dbx
 *
 ************************************************************************/

#if defined(unix) || defined(__unix) || defined(__xlC__)
# define PLATFORM_UNIX
#elif defined(WIN32) || defined(_WIN32)
# define PLATFORM_WIN32
#else
# warning "No platform defined, automatic stack trace will fail!"
#endif // defined(unix) || defined(__unix) || defined(__xlC__)

#if defined(_AIX) || defined(__xlC__)
# define PLATFORM_AIX
#elif defined(__FreeBSD__)
# define PLATFORM_FREEBSD
#elif defined(hpux) || defined(__hpux) || defined(_HPUX_SOURCE)
# define PLATFORM_HPUX
#elif defined(sgi) || defined(mips) || defined(_SGI_SOURCE)
# define PLATFORM_IRIX
#elif defined(__osf__)
# define PLATFORM_OSF
#elif defined(M_I386) || defined(_SCO_DS) || defined(_SCO_C_DIALECT)
# define PLATFORM_SCO
#elif defined(sun) || defined(__sun__) || defined(__SUNPRO_C)
# if defined(__SVR4) || defined(__svr4__)
#  define PLATFORM_SOLARIS
# endif
#endif // defined(_AIX) || defined(__xlC__)

/* ANSI C includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#if defined(PLATFORM_UNIX)
# include <unistd.h>
# include <sys/types.h>
# include <sys/wait.h>
# if defined(PLATFORM_IRIX) && defined(USE_BUILTIN)
/* Compile with -DUSE_BUILTIN and -lexc */
#  include <libexc.h>
# elif defined(PLATFORM_HPUX) && defined(USE_BUILTIN)
/* Compile with -DUSE_BUILTIN and -lcl */
extern void U_STACK_TRACE(void);
# endif
#endif // defined(PLATFORM_UNIX)

#include "stacktrace.h"

#ifndef FALSE
# define FALSE (0 == 1)
# define TRUE (! FALSE)
#endif

#define SYS_ERROR -1

#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

#define MAX_BUFFER_SIZE 512
#if defined(__GNUC__)
/* Change the code if ADDRESSLIST_SIZE is increased */
# define ADDRESSLIST_SIZE 20
#endif

/*************************************************************************
 * Globals
 *
 * We cannot pass custom arguments to signal handlers so we store
 * them as global variables (but limit their scope to this file.)
 */
static const char *global_progname;
static int global_output = STDOUT_FILENO;


#if defined(PLATFORM_UNIX)
/*************************************************************************
 * my_pclose [private]
 */
static void my_pclose(int fd, int pid)
{
  close(fd);
  /* Make sure the the child process has terminated */
  (void)kill(pid, SIGTERM);
}

/*************************************************************************
 * my_popen [private]
 */
static int my_popen(const char *command, pid_t *pid)
{
  int rc;
  //int pipefd[2];

  //FIXME: deactivated separate piping of debugger output, as it caused problems in conjunction with threads
  //rc = pipe(pipefd);
  rc = SYS_ERROR;
  //if (SYS_ERROR != rc)
    {
      *pid = fork();
      switch (*pid)
	{
	case SYS_ERROR:
	  //rc = SYS_ERROR;
	  //close(pipefd[0]);
	  //close(pipefd[1]);
	  break;

	case 0: /* Child */
	  //close(pipefd[0]);
	  //close(STDOUT_FILENO);
	  //close(STDERR_FILENO);
	  //dup2(pipefd[1], STDOUT_FILENO);
	  //dup2(pipefd[1], STDERR_FILENO);
	  /*
	   * The System() call assumes that /bin/sh is
	   * always available, and so will we.
	   */
	  execl("/bin/sh", "/bin/sh", "-c", command, NULL);
	  _exit(EXIT_FAILURE);
	  break;

	default: /* Parent */
	  //close(pipefd[1]);
	  //rc = pipefd[0];
	  rc = 0; // success
	  break;
	} /* switch */
    }
  return rc;
}

/*************************************************************************
 * my_getline [private]
 */
static int my_getline(int fd, char *buffer, int max)
{
  char c;
  int i = 0;

  do {
    if (read(fd, &c, 1) < 1)
      return 0;
    if (i < max)
      buffer[i++] = c;
  } while (c != '\n');
  buffer[i] = (char)0;
  return i;
}

# if defined(__GNUC__) && defined(USE_BUILTIN)
/*************************************************************************
 * GCC_DumpStack [private]
 *
 *
 * This code is still experimental.
 *
 * Stackbased arrays are used to prevent allocating from the heap.
 * Warning: sprintf/sscanf are not ASync safe. They were used for
 *  convenience.
 *
 * 'nm' is used because it is most widespread
 *  GNU:     nm [-B]
 *  Solaris: nm -x -p
 *  IRIX:    nm -x -B  (but __builtin_return_address() always returns NULL)
 *  AIX:     nm -x -B
 *  OSF/1:   nm -B
 *  SCO/OpenServer: nm -x -p
 *  HP/UX    nm -x -p
 */
typedef struct {
  unsigned long realAddress;
  unsigned long closestAddress;
  char name[MAX_BUFFER_SIZE + 1];
  char type;
} address_T;


static void GCC_DumpStack(void)
{
  int i;
  void *p = &p; /* dummy start value */
  address_T syms[ADDRESSLIST_SIZE + 1];
  char buffer[MAX_BUFFER_SIZE];
  int fd;
  pid_t pid;
  unsigned long addr;
  unsigned long highestAddress;
  unsigned long lowestAddress;
  char type;
  char *pname;
  char name[MAX_BUFFER_SIZE];
  int number;

  for (i = 0; p; i++)
    {
      /*
       * This is based on code by Steve Coleman <steve.colemanjhuapl.edu>
       *
       * __builtin_return_address() only accepts a constant as argument.
       */
      switch (i)
	{
	case 0:
	  p = __builtin_return_address(0);
	  break;
	case 1:
	  p = __builtin_return_address(1);
	  break;
	case 2:
	  p = __builtin_return_address(2);
	  break;
	case 3:
	  p = __builtin_return_address(3);
	  break;
	case 4:
	  p = __builtin_return_address(4);
	  break;
	case 5:
	  p = __builtin_return_address(5);
	  break;
	case 6:
	  p = __builtin_return_address(6);
	  break;
	case 7:
	  p = __builtin_return_address(7);
	  break;
	case 8:
	  p = __builtin_return_address(8);
	  break;
	case 9:
	  p = __builtin_return_address(9);
	  break;
	case 10:
	  p = __builtin_return_address(10);
	  break;
	case 11:
	  p = __builtin_return_address(11);
	  break;
	case 12:
	  p = __builtin_return_address(12);
	  break;
	case 13:
	  p = __builtin_return_address(13);
	  break;
	case 14:
	  p = __builtin_return_address(14);
	  break;
	case 15:
	  p = __builtin_return_address(15);
	  break;
	case 16:
	  p = __builtin_return_address(16);
	  break;
	case 17:
	  p = __builtin_return_address(17);
	  break;
	case 18:
	  p = __builtin_return_address(18);
	  break;
	case 19:
	  p = __builtin_return_address(19);
	  break;
	default:
	  /* Change ADDRESSLIST_SIZE if more are added */
	  p = NULL;
	  break;
	}
      if ((p) && (i < ADDRESSLIST_SIZE))
	{
	  syms[i].realAddress = (unsigned long)p;
	  syms[i].closestAddress = 0;
	  syms[i].name[0] = (char)0;
	  syms[i].type = ' ';
	}
      else
	{
	  syms[i].realAddress = 0;
	  break; /* for */
	}
    } /* for */


  /* First find out if we are using GNU or vendor nm */
  number = 0;
  strcpy(buffer, "nm -V 2>/dev/null | grep GNU | wc -l");
  fd = my_popen(buffer, &pid);
  if (SYS_ERROR != fd)
    {
      if (my_getline(fd, buffer, sizeof(buffer)))
	{
	  sscanf(buffer, "%d", &number);
	}
      my_pclose(fd, pid);
    }
  if (number == 0) /* vendor nm */
    {
#   if defined(PLATFORM_SOLARIS) || defined(PLATFORM_SCO) || defined(PLATFORM_HPUX)
      strcpy(buffer, "nm -x -p ");
#   elif defined(PLATFORM_AIX) || defined(PLATFORM_IRIX) || defined(PLATFORM_OSF)
      strcpy(buffer, "nm -x -B ");
#   else
      strcpy(buffer, "nm -B ");
#   endif
    }
  else /* GNU nm */
    strcpy(buffer, "nm -B ");
  strcat(buffer, global_progname);

  lowestAddress = ULONG_MAX;
  highestAddress = 0;
  fd = my_popen(buffer, &pid);
  if (SYS_ERROR != fd)
    {
      while (my_getline(fd, buffer, sizeof(buffer)))
	{
	  if (buffer[0] == '\n')
	    continue;
	  if (3 == sscanf(buffer, "%lx %c %s", &addr, &type, name))
	    {
	      if ((type == 't') || type == 'T')
		{
		  if (addr == 0)
		    continue; /* while */
		  if (addr < lowestAddress)
		    lowestAddress = addr;
		  if (addr > highestAddress)
		    highestAddress = addr;
		  for (i = 0; syms[i].realAddress != 0; i++)
		    {
		      if ((addr <= syms[i].realAddress) &&
			  (addr > syms[i].closestAddress))
			{
			  syms[i].closestAddress = addr;
			  strncpy(syms[i].name, name, MAX_BUFFER_SIZE);
			  syms[i].name[MAX_BUFFER_SIZE] = (char)0;
			  syms[i].type = type;
			}
		    }
		}
	    }
	}
      my_pclose(fd, pid);

      for (i = 0; syms[i].realAddress != 0; i++)
	{
	  if ((syms[i].name[0] == (char)0) ||
	      (syms[i].realAddress <= lowestAddress) ||
	      (syms[i].realAddress >= highestAddress))
	    {
	      sprintf(buffer, "[%d] 0x%08lx ???\n", i, syms[i].realAddress);
	    }
	  else
	    {
	      sprintf(buffer, "[%d] 0x%08lx <%s + 0x%lx> %c\n",
		      i,
		      syms[i].realAddress,
		      syms[i].name,
		      syms[i].realAddress - syms[i].closestAddress,
		      syms[i].type);
	    }
	  write(global_output, buffer, strlen(buffer));
	}
    }
}
# endif // defined(__GNUC__) && defined(USE_BUILTIN)

/*************************************************************************
 * DumpStack [private]
 */
static int DumpStack(char *format, ...)
{
  int gotSomething = FALSE;
  int fd;
  pid_t pid;
  int status = EXIT_FAILURE;
  int rc;
  va_list args;
  char cmd[MAX_BUFFER_SIZE];

  /*
   * Please note that vsprintf() is not ASync safe (ie. cannot safely
   * be used from a signal handler.) If this proves to be a problem
   * then the cmd string can be built by more basic functions such as
   * strcpy, strcat, and a homemade integer-to-ascii function.
   */
  va_start(args, format);
  vsprintf(cmd, format, args);
  va_end(args);
  
  fd = my_popen(cmd, &pid);
  if (SYS_ERROR != fd)
    {
      /*
       * Wait for the child to exit. This must be done
       * to make the debugger attach successfully.
       * The output from the debugger is buffered on
       * the pipe.
       *
       * AIX needs the looping hack
       */
      do
	{
	  rc = waitpid(pid, &status, 0);
	}
      while ((SYS_ERROR == rc) && (EINTR == errno));

#define NO_SEPARATE_DEBUG_PIPING
#ifdef NO_SEPARATE_DEBUG_PIPING
      //FIXME: deactivated separate piping of debugger output, as it caused problems in conjunction with threads
      gotSomething = TRUE; //HACK:
      my_getline(-1, NULL, 0); // avoid 'unused-function' compiler warning
#else
      char *buffer;
      char buf[MAX_BUFFER_SIZE];
      if ((WIFEXITED(status)) && (WEXITSTATUS(status) == EXIT_SUCCESS))
	{
	  while (my_getline(fd, buf, sizeof(buf)))
	    {
	      buffer = buf;
	      if (! gotSomething)
		{
		  write(global_output, "Output from ",
			strlen("Output from "));
		  strtok(cmd, " ");
		  write(global_output, cmd, strlen(cmd));
		  write(global_output, "\n", strlen("\n"));
		  gotSomething = TRUE;
		}
	      if ('\n' == buf[strlen(buf)-1])
		{
		  buf[strlen(buf)-1] = (char)0;
		}
	      write(global_output, buffer, strlen(buffer));
	      write(global_output, "\n", strlen("\n"));
	    }
	}
#endif // NO_SEPARATE_DEBUG_PIPING

      my_pclose(fd, pid);
    }
  return gotSomething;
}
#endif // defined(PLATFORM_UNIX)

/*************************************************************************
 * StackTrace
 */
void StackTrace(char *gdb_command_file)
{
#if defined(PLATFORM_UNIX)
  /*
   * In general dbx seems to do a better job than gdb.
   *
   * Different dbx implementations require different flags/commands.
   */

# if defined(PLATFORM_AIX)

  if (DumpStack("dbx -a %d 2>/dev/null <<EOF\n"
		"where\n"
		"detach\n"
		"EOF\n",
		(int)getpid()))
    return;

  if (DumpStack("gdb -q %s %d 2>/dev/null <<EOF\n"
		"set prompt\n"
		"where\n"
		"detach\n"
		"quit\n"
		"EOF\n",
		global_progname, (int)getpid()))
    return;

# elif defined(PLATFORM_FREEBSD)

  /*
   * FreeBSD insists on sending a SIGSTOP to the process we
   * attach to, so we let the debugger send a SIGCONT to that
   * process after we have detached.
   */
  if (DumpStack("gdb -q %s %d 2>/dev/null <<EOF\n"
		"set prompt\n"
		"where\n"
		"detach\n"
		"shell kill -CONT %d\n"
		"quit\n"
		"EOF\n",
		global_progname, (int)getpid(), (int)getpid()))
    return;

# elif defined(PLATFORM_HPUX)

  /*
   * HP decided to call their debugger xdb.
   *
   * This does not seem to work properly yet. The debugger says
   * "Note: Stack traces may not be possible until you are
   *  stopped in user code." on HP-UX 09.01
   *
   * -L = line-oriented interface.
   * "T [depth]" gives a stacktrace with local variables.
   * The final "y" is confirmation to the quit command.
   */

  if (DumpStack("xdb -P %d -L %s 2>&1 <<EOF\n"
		"T 50\n"
		"q\ny\n"
		"EOF\n",
		(int)getpid(), global_progname))
    return;

  if (DumpStack("gdb -q %s %d 2>/dev/null <<EOF\n"
		"set prompt\n"
		"where\n"
		"detach\n"
		"quit\n"
		"EOF\n",
		global_progname, (int)getpid()))
    return;

#  if defined(PLATFORM_HPUX) && defined(USE_BUILTIN)
  U_STACK_TRACE();
  return;
#  endif

# elif defined(PLATFORM_IRIX)

  /*
   * "set $page=0" drops hold mode
   * "dump ." displays the contents of the variables
   */
  if (DumpStack("dbx -p %d 2>/dev/null <<EOF\n"
		"set \\$page=0\n"
		"where\n"
#  if !defined(__GNUC__)
		/* gcc does not generate this information */
		"dump .\n"
#  endif
		"detach\n"
		"EOF\n",
		(int)getpid()))
    return;

#  if defined(USE_BUILTIN)
  if (trace_back_stack_and_print())
    return;
#  endif

  if (DumpStack("gdb -q %s %d 2>/dev/null <<EOF\n"
		"set prompt\n"
		"echo --- Stacktrace\\n\n"
		"where\n"
		"echo --- Symbols\\n\n"
		"frame 5\n"      /* Skip signal handler frames */
		"set \\$x = 50\n"
		"while (\\$x)\n" /* Print local variables for each frame */
		"info locals\n"
		"up\n"
		"set \\$x--\n"
		"end\n"
		"echo ---\\n\n"
		"detach\n"
		"quit\n"
		"EOF\n",
		global_progname, (int)getpid()))
    return;

# elif defined(PLATFORM_OSF)

  if (DumpStack("dbx -pid %d %s 2>/dev/null <<EOF\n"
		"where\n"
		"detach\n"
		"quit\n"
		"EOF\n",
		(int)getpid(), global_progname))
    return;

  if (DumpStack("gdb -q %s %d 2>/dev/null <<EOF\n"
		"set prompt\n"
		"where\n"
		"detach\n"
		"quit\n"
		"EOF\n",
		global_progname, (int)getpid()))
    return;

# elif defined(PLATFORM_SCO)

  /*
   * SCO OpenServer dbx is like a catch-22. The 'detach' command
   * depends on whether ptrace(S) support detaching or not. If it
   * is supported then 'detach' must be used, otherwise the process
   * will be killed upon dbx exit. If it isn't supported then 'detach'
   * will cause the process to be killed. We do not want it to be
   * killed.
   *
   * Out of two evils, the omission of 'detach' was chosen because
   * it worked on our system.
   */
  if (DumpStack("dbx %s %d 2>/dev/null <<EOF\n"
		"where\n"
		"quit\nEOF\n",
		global_progname, (int)getpid()))
    return;

  if (DumpStack("gdb -q %s %d 2>/dev/null <<EOF\n"
		"set prompt\n"
		"where\n"
		"detach\n"
		"quit\n"
		"EOF\n",
		global_progname, (int)getpid()))
    return;

# elif defined(PLATFORM_SOLARIS)

  if (DumpStack("dbx %s %d 2>/dev/null <<EOF\n"
		"where\n"
		"detach\n"
		"EOF\n",
		global_progname, (int)getpid()))
    return;

  if (DumpStack("gdb -q %s %d 2>/dev/null <<EOF\n"
		"set prompt\n"
		"echo --- Stacktrace\\n\n"
		"where\n"
		"echo --- Symbols\\n\n"
		"frame 5\n"      /* Skip signal handler frames */
		"set \\$x = 50\n"
		"while (\\$x)\n" /* Print local variables for each frame */
		"info locals\n"
		"up\n"
		"set \\$x--\n"
		"end\n"
		"echo ---\\n\n"
		"detach\n"
		"quit\n"
		"EOF\n",
		global_progname, (int)getpid()))
    return;

  if (DumpStack("/usr/proc/bin/pstack %d",
		(int)getpid()))
    return;

  /*
   * Other Unices (AIX, HPUX, SCO) also have adb, but
   * they seem unable to attach to a running process.
   */
  if (DumpStack("adb %s 2>&1 <<EOF\n"
		"0t%d:A\n" /* Attach to pid */
		"\\$c\n"   /* print stacktrace */
		":R\n"     /* Detach */
		"\\$q\n"   /* Quit */
		"EOF\n",
		global_progname, (int)getpid()))
    return;

# else /* All other Unix platforms */

  /*
   * TODO: SCO/UnixWare 7 must be something like (not tested)
   *  debug -i c <pid> <<EOF\nstack -f 4\nquit\nEOF\n
   */

#  if !defined(__GNUC__)
  if (DumpStack("dbx %s %d 2>/dev/null <<EOF\n"
		"where\n"
		"detach\n"
		"EOF\n",
		global_progname, (int)getpid()))
    return;
#  endif

  // This version works (piping from a file)
  if (DumpStack("gdb -q %s %d 2>/dev/null <%s >fweelin-stackdump",
		global_progname, (int)getpid(), gdb_command_file))
    return;

#if 0
  // This version does not work (piping from stdin)
  if (DumpStack("gdb -q %s %d 2>/dev/null <<EOF\n"
		"set prompt\n"
		"echo --- Stacktrace\\n\n"
		"where\n"
		"echo --- Symbols\\n\n"
		"frame 4\n"
		"set \\$x = 50\n"
		"while (\\$x)\n" /* Print local variables for each frame */
		"info locals\n"
		"up\n"
		"set \\$x--\n"
		"end\n"
		"echo ---\\n\n"
		"detach\n"
		"quit\n"
		"EOF\n",
		global_progname, (int)getpid()))
    return;
#endif

# endif // defined(PLATFORM_AIX)

# if defined(__GNUC__) && defined(USE_BUILTIN)

  GCC_DumpStack();

# endif

  char *err_msg =	"No debugger found\n";
  int io_err = write(global_output, err_msg, strlen(err_msg));
  if (io_err) printf("DEBUG: I/O error writing to global_output - err: (%s)\n" , err_msg);

#elif defined(PLATFORM_WIN32)
  /* Use StackWalk() */
#endif // defined(PLATFORM_UNIX)
}

/*************************************************************************
 * StackTraceInit
 */
void StackTraceInit(const char *in_name, int in_handle)
{
  global_progname = in_name;
  global_output = (in_handle == -1) ? STDOUT_FILENO : in_handle;
}

/*************************************************************************
 * test
 */
#if defined(STANDALONE)

void CrashHandler(int sig)
{
  /* Reinstall default handler to prevent race conditions */
  signal(sig, SIG_DFL);
  /* Print the stack trace */
  StackTrace();
  /* And exit because we may have corrupted the internal
   * memory allocation lists. Use abort() if we want to
   * generate a core dump. */
  _exit(EXIT_FAILURE);
}


void Crash(void)
{
  /* Force a crash */
  strcpy(NULL, "");
}

int main(int argc, char *argv[])
{
  struct sigaction sact;

  StackTraceInit(argv[0], -1);

  sigemptyset(&sact.sa_mask);
  sact.sa_flags = 0;
  sact.sa_handler = CrashHandler;
  sigaction(SIGSEGV, &sact, NULL);
  sigaction(SIGBUS, &sact, NULL);

  Crash();
  return EXIT_SUCCESS;
}
#endif // defined(STANDALONE)
