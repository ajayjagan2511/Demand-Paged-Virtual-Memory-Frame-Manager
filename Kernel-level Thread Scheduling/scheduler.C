/*
 File: scheduler.C

 Author:
 Date  :

 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_timer.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler()
{
  // initially queue is empty.
  head = nullptr;
  tail = nullptr;
  // Use the static function of Thread to get the current thread
  currThread = Thread::CurrentThread();
  zombieThread = nullptr;
  // assert(false);
  Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield()
{
  //disable interrupts when yielding the CPU (context switch)
  if(Machine::interrupts_enabled())
    Machine::disable_interrupts();
  currThread = Thread::CurrentThread();
  assert(currThread != nullptr && "running thread cant be null and yield\n");
  if (head != nullptr)
  {
    // remove the top element from running queue
    ReadyThread *node = head;
    // if only one element
    if (head == tail)
    {
      head = nullptr;
      tail = nullptr;
    }
    else
    {
      // make the second element the first element
      head->next->prev = nullptr;
      head = head->next;
    }
    // dispatch the first thread
    //enable interrupts after context switching
    if(!Machine::interrupts_enabled())
      Machine::enable_interrupts();
    Thread::dispatch_to(node->thread);
    //delete node
    delete node;
    // update running thread
    currThread = Thread::CurrentThread();
  }
  // assert(false);
}

void Scheduler::resume(Thread *_thread)
{
  // Disable interrupts when adding to ready queue
  if(Machine::interrupts_enabled())
    Machine::disable_interrupts();
  // Add the thread as the tail to the linked list
  // Queue is empty
  ReadyThread *node = new ReadyThread(_thread);
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
  //enable interrupts after adding to ready queue
  if(!Machine::interrupts_enabled())
    Machine::enable_interrupts();
  // assert(false);
}

void Scheduler::add(Thread *_thread)
{
  // Since resume already has the mechanism to add a thread to end of queue, just using that
  resume(_thread);
  // assert(false);
}

void Scheduler::terminate(Thread *_thread)
{
  //delete the prior zombie thread if exists
  if (zombieThread != nullptr)
  {
    delete zombieThread;  // Custom Destructor in thread. Handles stack cleanup
    zombieThread = nullptr;
  }
  // set the current thread as zombie thread for later deletion
  zombieThread = _thread;
  // assert(false);
}


void RRScheduler::yield()
{
  // Reset the EOQ timer
  EOQTimer::reset_ticks();
  // Call base class yield to perform context switch
  this->Scheduler::yield();
}