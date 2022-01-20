# This project is no longer maintained

## ckpt

ckpt - process checkpoint library  
www.paradyn.org/projects/legacy/ckpt  
Copyright (c) 2002-2005 Victor C. Zandy  

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

COPYING contains the distribution terms for ckpt (LGPL).
