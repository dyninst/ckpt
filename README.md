ckpt - process checkpoint library
www.cs.wisc.edu/~zandy/ckpt
Copyright (c) 2002-2005 Victor C. Zandy  zandy@cs.wisc.edu

COPYING contains the distribution terms for ckpt (LGPL).

CONTENTS

 1     RELEASE NOTES
 2     INSTALLATION
 3     QUICK EXAMPLE
 4     OVERVIEW
 5     WHAT DOES CKPT CHECKPOINT?
 6     USING CKPT
 6.1       LINKING
 6.2       TRIGGERING CHECKPOINTS
 6.3       RESTARTING CHECKPOINTS
 7     CKPT CONFIGURATION
 7.1       CONFIGURATION OPTIONS
 7.2       COMMAND LINE OPTIONS
 7.3       ENVIRONMENT VARIABLES
 8     CKPT API
 9     CONTACT


1 RELEASE NOTES

We support ckpt on x86's running Linux 2.4 and 2.6.
Updates are released incrementally.  Major improvements
since the beginning of 2004 include:

* Checkpoint files are executable.  The restart command
  and librestart.so library found in previous versions
  are obsolete.

* libckpt.so is for making executables pre-linked with
  ckpt.  It does not need to be installed to use the
  ckpt command.  It can still be linked at run time
  with LD_PRELOAD.

* You can inject ckpt into running programs (hijacking)
  with the -p option to ckpt.

* The -z option of ckpt enables periodic checkpointing.

* Open regular files are restored across checkpoints;
  see ckpt_fds in the API.


2 INSTALLATION

Edit the user options in Makefile to set the compiler, compiler
flags, and installation directories.  Then run 'make'.

The Makefile produces three files for users:

  ckpt           ckpt command 
  libckpt.so     ckpt library for building ckpt-aware applications
  ckpt.h         ckpt API C header for ckpt-aware applications


The ckpt command is a standalone program for checkpointing existing
executables.  It does not depend on libckpt.so or ckpt.h.

The libckpt.so library is for users who want to build executables
linked with ckpt.  The ckpt.h C header file is for writing programs
(linked with libckpt.so) that call ckpt services.

'make install' installs these three files under the user's home
directory.  Edit the Makefile for a different destination.


3 QUICK EXAMPLE

For users who want to use ckpt to checkpoint ordinary executables,
here is all you really need to know.

We demonstrate ckpt on a program named foo that prints integers
starting from 1.

% ckpt foo                         # run foo with checkpointing
1
2
3
4
5
^Z                                 # send checkpoint signal;
                                   # ckpt saves checkpoint to foo.ckpt
                                   # and exits the program.

% foo.ckpt                         # restart checkpoint
6
7
8

Also, you can:
- set the name of the checkpoint file (with the -n option)
- use a different checkpoint signal (with the -z option)
- checkpoint a running process (with the -p option)


4 OVERVIEW

ckpt provides user-level process checkpointing functionality to an
ordinary program.  It inserts code into the program that enables the
program to checkpoint itself.  Ckpt supports asynchronous checkpoints
triggered by signals sent by other programs.  Ckpt-aware programs can
also use the ckpt API to synchronously checkpoint themselves.

Ckpt writes its checkpoint of a program to a checkpoint file.  A
checkpoint file is an ordinary executable (in ELF format) that,
when executed, continues the program from the point at which it was
checkpointed.  Checkpoints can be restarted on a different machine
from the one on which the checkpoint was taken, however the machine
must have the same processor architecture and operating system.


5 WHAT DOES CKPT CHECKPOINT?

The state of a program in execution has many aspects.  Ckpt saves
and restores only some of them.

Ckpt saves and restores:

* The entire address space of the process: its text, data,
  stack, and heap, including shared library text and data;
* The signal state of the process;
* The environment of the process; (the environment of the process
  where it is restarted is ignored);

Ckpt does not save and restore:

- File descriptors of open files, special devices,
  pipes, and sockets;
- Interprocess communication state (such as shared memory, semaphores,
  mutex, messages);
- Kernel-level thread state;
- Process identifiers, including process id, process group;
  id, user id, or group id.

The ckpt API provides hooks for calling third-party code at stages of
a program checkpoint and restart.  Users can add their own code to
these hooks to augment the checkpointing functionality with, for
example, code to save open file descriptors.


6 USING CKPT

Using ckpt involves linking the ckpt run time code with a program,
triggering checkpoints, and restarting checkpoints.


6.1 LINKING

There are four ways to link ckpt with a program.

1. The simplest way to link ckpt is to use the ckpt command to start
the program.

    % ckpt PROGRAM ARGS ...

ckpt exec's the program, preloads the ckpt library into the process
address space, and then allows the program to run.

Note that programs spawned by PROGRAM (such as by calls to exec()
and system()) will NOT be checkpoint enabled.

2. Set the LD_PRELOAD environment variable to the
location of libckpt.so, and run the program in that environment.

    % setenv LD_PRELOAD /home/user/lib/libckpt.so
    % PROGRAM ARGS ... 

Unlike programs launched with the ckpt command, programs spawned by
PROGRAM (such as by calls to exec() and system()) will be also be
checkpoint enabled.

3. Link the program executable with libckpt.so.

For example, if the program foo is linked with this line:
     cc -o foo foo.o -lm
 
Then edit the link line to include libckpt.so:
     cc -o foo foo.o -lm -L/home/user/lib -lckpt

Note that the -L option directs the linker to include the
specified directory in its search for the library.

Note also that the -L option does not affect the set of
directories searched when the program is started.  You must also
modify the LD_LIBRARY_PATH environment variable to include the
directory containing libckpt.so.

Programs spawned by the program (such as by calls to exec() and
system()) will be also be checkpoint enabled.

4. Inject ckpt into an already running program.

     % ckpt -p PID

Programs spawned by the process (such as by calls to exec() and
system()) will NOT be checkpoint enabled.


6.2 TRIGGERING CHECKPOINTS

By default, a process linked with ckpt can be asynchronously
checkpointed by sending the SIGTSTP signal to it.  The signal
can be changed by modifying the configuration parameters described
below.

Optionally, a process can be automatically checkpointed
periodically.

User code can call routines in ckpt.h to synchronously checkpoint
itself.


6.3 RESTARTING CHECKPOINTS

Checkpoint files are executables.  To restart a checkpoint, execute
the checkpoint file like an ordinary executable.  The environment
in which the checkpoint file is executed is discarded.  The
environment of the original program is restored.


7 CKPT CONFIGURATION

Several user-configurable parameters control the behavior of ckpt.
We describe these parameters and how to set these parameters with
ckpt command line options and with environment variables.  Section
8 describes CKPT API calls that allow programs to set these
parameters.

7.1  CONFIGURATION OPTIONS

* CKPT_NAME is a string that specifies the method and name for
saving checkpoints.  Two methods are supported:

-- Local file system

If CKPT_NAME has the form

	file://FILENAME

or just

	FILENAME  (where FILENAME does not contain the string "://")

then the checkpoint is stored in the local file system in the file
named FILENAME.

The default CKPT_NAME stores checkpoints in the local file system.
The checkpoint name of the program that is being checkpointed,
followed by the string ".ckpt".

-- Remote checkpoint server

If CKPT_NAME has the form

	cssrv://HOSTNAME[:PORT]/NAME

then the checkpoint is sent over TCP to a checkpoint server running
on HOSTNAME, which may be an IP address or DNS name, that should be
listening to its default port or on PORT if specified.  The
checkpoint server stores the checkpoint with the name NAME.

The checkpoint server is a simple insecure server suitable for
only private use.  It is included in the ckpt distribution in the
program ckptsrv.  Invoke it

	% ckptsrv

to start it listening for checkpoints on port 10301 or

	% ckptsrv -p PORT

to set a different port.

ckptsrv stores checkpoint images in /tmp; use -d to
select a different directory.


* CKPT_CONTINUE is a boolean that specifies what happens to a process
following a checkpoint.  If 1 the process continues normally.
If 0 the process exits by calling _exit().

The default value of CKPT_CONTINUE is 0.


* CKPT_ASYNCSIG is an integer that identifies the signal that triggers
an asynchronous checkpoint.  If 0 asynchronous checkpoints are
disabled.  Otherwise CKPT_ASYNCSIG is integer corresponding to a
defined signal.

Asynchronous checkpoints are enabled by default: the default value of
CKPT_ASYNCSIG is the integer corresponding to the SIGTSTP signal.


* CKPT_MSPERIOD is an integer that specifies the period in
milliseconds at which to take automatic periodic checkpoints.  If 0,
periodic checkpoints are disabled.

Periodic checkpoints are disabled by default.

Note that periodic checkpoints will likely break programs that use
timer mechanisms such as the setitimer and alarm calls.  (Please
send us email if this somewhat easily resolved limitation interferes
with your use of ckpt.)


7.2 COMMAND LINE OPTIONS

The ckpt command line arguments are

  -n NAME        set CKPT_NAME to NAME
  -c             set CKPT_CONTINUE to TRUE
  -a SIGNAL      set CKPT_ASYNCSIG to SIGNAL, which may be an integer
                 or SIGname string
  -z PERIOD      set CKPT_MSPERIOD to PERIOD, an unsigned integer 
  -p PID         inject checkpoint code into process PID


7.3 ENVIRONMENT VARIABLES

Ckpt recognizes environment variables with the names of parameters.
For example,

    setenv CKPT_ASYNCSIG SIGUSR1

The environment variables are read once and only once: the first
time the checkpoint library is loaded.  Programs that wish to
change checkpoint parameters at run time must call the ckpt API.

The set of environment variables that is interpreted depends on
how ckpt is loaded into a program:

  execution time: ckpt consults the environment of the invoking
                  ckpt command.

  link time:      ckpt consults the environment of the program when
                  it is executed. ???
  

8 CKPT API

Programs linked with the checkpoint library can call the
functions defined in ckpt.h.

* CALLBACKS

Ckpt enables ckpt-aware programs to register callbacks to be called
when checkpoints are taken or restarted.  Beware that functions
registered with these interfaces may be called in the context of a
signal handler.

void ckpt_on_preckpt(void (*f)(void *), void *arg);

    Register F to be called when a checkpoint is triggered.
    Registered functions are called before the checkpoint begins
    in the order they were registered.  F is passed the ARG
    argument.

void ckpt_on_postckpt(void (*f)(void *), void *arg);

    Register F to be called when a checkpoint is taken and
    CKPT_CONTINUE is set.  Registered functions are called
    after the checkpoint completes in the order they were
    registered.  F is passed the ARG argument.

void ckpt_on_restart(void (*f)(void *), void *arg);

    Register F to be called when a process is restarted from a
    checkpoint.  Registered functions are called after the
    checkpoint has been completely restored, just before control
    returns to the program, in the reverse of the order they were
    registered.  F is passed the ARG argument.

* CONFIGURATION

Ckpt-aware programs can configure ckpt by calling ckpt_config.

void ckpt_config(struct ckptconfig *cfg, struct ckptconfig *old)

    The struct ckptconfig represents the ckpt parameters.  It includes
    the following fields.  (The use of other fields is undefined.)

    struct ckptconfig {
           int flags;
           char name[CKPT_MAXNAME];    /* CKPT_NAME */
	   unsigned int asyncsig;      /* CKPT_ASYNCSIG */
	   unsigned int continues;     /* CKPT_CONTINUE */
	   unsigned int msperiod;      /* CKPT_MSPERIOD */
    };

    If OLD is non-NULL it is filled with a copy of the current
    checkpoint parameters. 
    
    If CFG is non-NULL the configuration is updated (after possibly
    copying the current configuration to OLD).  FLAGS is a bitmask.
    For each of the bit positions CKPT_NAME, CKPT_ASYNCSIG,
    CKPT_MSPERIOD, and CKPT_CONTINUE, if the bit is set in FLAGS,
    then the corresponding value in the configuration is updated.

* INVOKING CHECKPOINTS 

void ckpt_ckpt(char *name)

    Take a checkpoint.  If NAME is non-null, CKPT_NAME is set to
    NAME for the execution of the call, otherwise the current value
    of CKPT_NAME is used.  (Return value?)

void ckpt_restart(char *name)

    Restarts the checkpoint named NAME.  NAME should follow the
    syntax of checkpoint names described in section 7.1.
    ckpt_restart does not return unless the checkpoint image cannot
    be found or fails to restart.

* FILE DESCRIPTORS

struct ckptfdtbl * ckpt_fds();

    ckpt_fds returns a list of file descriptors held open by the
    process.  It should only be called in callbacks registered with
    ckpt_on_preckpt, ckpt_on_postckpt, or ckpt_on_restart; results
    in other contexts are undefined.

    ckpt_fds returns a pointer to ckpt's internal table of file
    descriptor state.  The table is completely defined in ckpt.h;
    here we highlight the more interesting fields.

    struct ckptfdtbl {
              unsigned nfd;           /* number of fds */
              struct ckptfd *fds;     /* array of fds */
    };

    struct ckptfd {
              int fd;                 /* file descriptor number */
              int treatment;          /* CKPT_FD_RESTORE or CKPT_FD_IGNORE */
              int type;               /* CKPT_FD_REGULAR, CKPT_FD_PIPE, ... */
	      union {
                      struct regular *reg;   /* CKPT_FD_REGULAR */
              } u;
    };

    struct regular {
              char *path;
              int mode;
              off_t offset;
    };

    Each of the NFD file descriptors held open by the process is
    represented by an entry in FDS, an array of struct ckptfds.  The
    order of file descriptors in this array is not defined; to find
    an entry for a particular file descriptor, search for the entry
    with matching FD field.

    If the TREATMENT field is CKPT_FD_RESTORE, ckpt will attempt to
    restore the file descriptor when the checkpoint is restart.
    Otherwise (TREATMENT is CKPT_FD_IGNORE), ckpt not ignore
    the file descriptor when restarting.

    TYPE defines the kind of file resource the descriptor
    represents.  Each TYPE has a corresponding structure in U
    containing additional information about the resource.

    For a regular file the saved state includes the absolute path,
    the mode (e.g., RD_ONLY), and the file offset.

    By default, regular file descriptors are restored on checkpoint
    restart.  Programs can change the value of TREATMENT to affect
    whether a file descriptor is restored.  Ckpt only knows how to
    restore regular file descriptors, and will abort if told to
    restore non-regular files.  Programs can also modified the
    fields of a struct regular to tell ckpt to open a different file
    on restart.  The path field points to heap allocated memory; if
    changed, the old value should be freed, and the new value should
    be a pointer to newly allocated memory, which ckpt will free later. 

    When restarting a checkpoint, file descriptors are restored
    after the ckpt_on_restart callbacks are run.


9 CONTACT

Victor Zandy wrote and maintains ckpt.  Please report bugs to
zandy@cs.wisc.edu.  Feedback and experience reports are welcome.
The ckpt webpage is http://www.cs.wisc.edu/~zandy/ckpt.
