/* 
    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
			
	    Collection of System Components.

*/
#ifndef SYSTEM_H
#define SYSTEM_H

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define MB * (0x1 << 20)
#define KB * (0x1 << 10)

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "frame_pool.H"
#include "mem_pool.H"
#include "simple_disk.H"
#include "nonblocking_disk.H"
#include "scheduler.H"

/*--------------------------------------------------------------------------*/
/* S y s t e m */
/*--------------------------------------------------------------------------*/
//class Scheduler;

class System {

public:

    static const unsigned int DISK_SIZE = (10 MB);
 
    static MemPool* MEMORY_POOL;

    static NonBlockingDisk* DISK;

    static Scheduler* SCHEDULER;  
};

#endif
