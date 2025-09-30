CSCE 410/611/613: MP1 -- README.TXT

This file describes the content of this directory.
A file marked with (*) may be of some interest to the 
student, and he/she may want to have a look at it.
A file marked with (**) is important for this MP, and the
student is strongly encouraged to study the contents of this file.

COMPILATION:
===========

FILE: 			DESCRIPTION:

makefile 		Makefile for Linux and Max OS environment.
linker.ld		Linker script.

OS COMPONENTS:
=============

FILE: 			DESCRIPTION:

start.asm (*)		The bootloader starts code in this file, which in turn
			    	jumps to the main entry in File "kernel.C".
kernel.C (**)		Main file, where the OS components are set up, and the
                    system gets going.
utils.H/C			Various utilities (e.g. memcpy, strlen, etc..)
simple_console.H/C	Routines to print to the screen.
