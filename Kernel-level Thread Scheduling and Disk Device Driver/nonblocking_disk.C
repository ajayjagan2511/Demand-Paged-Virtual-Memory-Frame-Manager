/*
     File        : nonblocking_disk.c

     Author      : 
     Modified    : 

     Description : 

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "nonblocking_disk.H"
#include "system.H"
#include "thread.H"
#include "machine.H"
#include "interrupts.H"


// simple disk lock class for disk hardware and queue access
class DiskLock {
public:
  void lock()
  {
    for (;;) {
      //disable interrupts to enter critical section
      if (Machine::interrupts_enabled())
        Machine::disable_interrupts();
      //disabled to make it an actomic action
      if (!locked) {
        //lock it
        locked = true;
        //re-enable interrupts
        Machine::enable_interrupts();
        return;
      }

      Machine::enable_interrupts();
      //yield CPU to other threads as we couldnt acquire lock
      System::SCHEDULER->yield();
    }
  }

  void unlock()
  {
    //disabled to make it an actomic action
    if (Machine::interrupts_enabled())
      Machine::disable_interrupts();
    //we can reach here only if we hold the lock, so just release it
    locked = false;
    Machine::enable_interrupts();
  }

private:
  volatile bool locked = false;
};

static DiskLock hw_lock;
static DiskLock queue_lock;

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

NonBlockingDisk::NonBlockingDisk(unsigned int _size) 
  : SimpleDisk(_size) {
    // initialize IOQueue is empty
    head = tail = nullptr;
}

void NonBlockingDisk::wait_while_busy() {
  while (is_busy()) {
    // Yield the CPU to other threads while waiting for the disk to be ready
    System::SCHEDULER->yield();
  }
}

void NonBlockingDisk::read(unsigned long _block_no, unsigned char* _buf) {
  //add current thread to end of IO waiting queue as we need I/O call
  IOThreadNode *node = new IOThreadNode(Thread::CurrentThread());
  //lock queue before modifying
  queue_lock.lock();
  if (tail == nullptr)
  {
    tail = node;
    head = node;
  }
  else
  {
    tail->next = node;
    node->prev = tail;
    tail = node;
  }
  //unlock queue after modifying
  queue_lock.unlock();
  //lock hardware before disk operation
  hw_lock.lock();
  SimpleDisk::read(_block_no, _buf);
  //unlock hardware after disk operation
  hw_lock.unlock();
  // yield the CPU to other threads after read request to go to end of ready queue
  System::SCHEDULER->resume(Thread::CurrentThread());
  System::SCHEDULER->yield();
}

void NonBlockingDisk::write(unsigned long _block_no, unsigned char* _buf) {
  //add current thread to end of IO waiting queue as we need I/O call
  IOThreadNode *node = new IOThreadNode(Thread::CurrentThread());
  //lock queue before modifying
  queue_lock.lock();
  if (tail == nullptr)
  {
    tail = node;
    head = node;
  }
  else
  {
    tail->next = node;
    node->prev = tail;
    tail = node;
  }
  //unlock queue after modifying
  queue_lock.unlock();
  //lock hardware before disk operation
  hw_lock.lock();
  SimpleDisk::write(_block_no, _buf);
  //unlock hardware after disk operation
  hw_lock.unlock();
  // yield the CPU to other threads after write request to go to end of ready queue
  System::SCHEDULER->resume(Thread::CurrentThread());
  System::SCHEDULER->yield();
}

bool NonBlockingDisk::is_busy(){
  return SimpleDisk::is_busy();
}

void NonBlockingDisk::lock_queue()
{
  queue_lock.lock();
}

void NonBlockingDisk::unlock_queue()
{
  queue_lock.unlock();
}

  
void NonBlockingDisk::handle_interrupt(REGS *_r) {
  Console::puts("INTERRUPT HANDLED\n");
  InterruptHandler::set_EOI(_r); 
  System::SCHEDULER->resume(Thread::CurrentThread()); // resume the currently running thread
  System::SCHEDULER->yield(1); // indicate this yield comes from an interrupt  
}



