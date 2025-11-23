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
#include "system.H"
#include "nonblocking_disk.H"

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

void Scheduler::yield(int interrupt)
{
  // disable interrupts when yielding the CPU (context switch)
  if (Machine::interrupts_enabled())
    Machine::disable_interrupts();
  currThread = Thread::CurrentThread();
  assert(currThread != nullptr && "running thread cant be null and yield\n");
  // Determine disk state. If this yield was triggered by an interrupt
  // we assume the device signaled completion, so we treat the disk as not busy
  // and allow the scheduler to dispatch waiting IO threads.
  bool disk_busy = false;
  bool disk_queue_empty = true;

  if (interrupt == 0) {
    disk_busy = System::DISK->is_busy();
  } else {
    disk_busy = false; // on interrupt, assume device finished
  }
  // read queue emptiness under the disk queue lock
  System::DISK->lock_queue();
  disk_queue_empty = (System::DISK->head == nullptr);
  System::DISK->unlock_queue();

  // If disk is busy OR there are no waiting IO threads, service regular ready queue
  if (disk_busy || disk_queue_empty)
  {
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
      // enable interrupts after context switching
      if (!Machine::interrupts_enabled())
        Machine::enable_interrupts();
      Thread::dispatch_to(node->thread);
      // delete node
      delete node;
      // update running thread
      currThread = Thread::CurrentThread();
    }
  }
  // if the IO is free (or we were called by an interrupt), dequeue an IO waiter
  else
  {
    // remove the top element from running queue
    System::DISK->lock_queue();
    NonBlockingDisk::IOThreadNode *node = System::DISK->head;
    if (System::DISK->head == System::DISK->tail)
    {
      System::DISK->head = nullptr;
      System::DISK->tail = nullptr;
    }
    else
    {
      System::DISK->head->next->prev = nullptr;
      System::DISK->head = System::DISK->head->next;
    }
    System::DISK->unlock_queue();
    // dispatch the first thread
    // enable interrupts after context switching
    if (!Machine::interrupts_enabled())
      Machine::enable_interrupts();
    Thread::dispatch_to(node->thread);
    // delete node
    delete node;
    // update running thread
    currThread = Thread::CurrentThread();
  }
}

void Scheduler::resume(Thread *_thread)
{
  // Disable interrupts when adding to ready queue
  if (Machine::interrupts_enabled())
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
  // enable interrupts after adding to ready queue
  if (!Machine::interrupts_enabled())
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
  // delete the prior zombie thread if exists
  if (zombieThread != nullptr)
  {
    delete zombieThread; // Custom Destructor in thread. Handles stack cleanup
    zombieThread = nullptr;
  }
  // set the current thread as zombie thread for later deletion
  zombieThread = _thread;
  // assert(false);
}

void RRScheduler::yield(int interrupt)
{
  // Reset the EOQ timer
  EOQTimer::reset_ticks();
  // Call base class yield to perform context switch
  this->Scheduler::yield(interrupt);
}