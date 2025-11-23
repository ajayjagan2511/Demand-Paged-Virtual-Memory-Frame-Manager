/*
     File        : nonblocking_disk.H

     Author      : 

     Date        : 
     Description : 

*/

#ifndef _NONBLOCKING_DISK_H_
#define _NONBLOCKING_DISK_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"
#include "thread.H"
#include "interrupts.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* N o n B l o c k i n g D i s k  */
/*--------------------------------------------------------------------------*/

class NonBlockingDisk : public SimpleDisk, public InterruptHandler {
public:
   
   // interrupt handler for disk
   virtual void handle_interrupt(REGS *_r);

   // To manage waiting threads, doubly linked list
   struct IOThreadNode
   {
      Thread *thread;
      IOThreadNode *next;
      IOThreadNode *prev;
      IOThreadNode() : thread(nullptr), next(nullptr), prev(nullptr) {}
      IOThreadNode(Thread *x) : thread(x), next(nullptr), prev(nullptr) {}
      IOThreadNode(Thread *x, IOThreadNode *next, IOThreadNode *prev) : thread(x), next(next), prev(prev) {}
   };
   // keep track of the top and bottom of queue
   IOThreadNode *head, *tail;


   NonBlockingDisk(unsigned int _size); 
   /* Creates a NonBlockingDisk device with the given size connected to the 
      MASTER slot of the primary ATA controller.
      NOTE: We are passing the _size argument out of laziness. 
      In a real system, we would infer this information from the 
      disk controller. */

   virtual void read(unsigned long _block_no, unsigned char* _buf);
   
   virtual void write(unsigned long _block_no, unsigned char* _buf);

   virtual bool is_busy();

   void lock_queue();
   void unlock_queue();

protected:
   virtual void wait_while_busy();

};
#endif
