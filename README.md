# C++ Kernel from Scratch: VM & Thread Scheduling

## Introduction

This project is an academic exploration into building a minimal, x86-based kernel from the ground up in **C++**.  
It chronicles the development of two fundamental operating system pillars: a complete **Virtual Memory (VM)** subsystem and a **preemptive, kernel-level Thread Scheduler**.

The project begins with managing raw physical memory and culminates in a system that can run multiple preemptive, round-robin scheduled threads with graceful termination and full memory virtualization.

---

## Problem Statement

Building a functional kernel from scratch requires solving several fundamental challenges in resource management.  
This project addresses the two primary domains: **memory** and **computation**.

### 1. Virtual Memory Challenges

A modern kernel cannot safely or efficiently manage memory without abstraction.  
The core problems are:

- **Physical Memory Management** — How to track and allocate raw physical memory frames, especially for contiguous hardware-level requests?
- **Address Translation** — How to implement a virtual address space to isolate the kernel and processes, and how to translate virtual addresses to physical ones?
- **Efficient Memory Usage** — How to avoid allocating physical memory for an entire process upfront? This requires a *demand paging* system that allocates frames only when memory is first accessed.
- **Scalability** — The x86 paging structures (Page Directory, Page Tables) can themselves become large. How can the kernel manage these structures without reserving a large, fixed, and unscalable portion of direct-mapped memory for them?
- **High-Level Allocation** — How to provide a simple, `malloc` / `free`-like interface for processes to request and release regions of virtual memory?

### 2. Threading & Scheduling Challenges

A kernel must manage concurrent execution to utilize the CPU effectively.  
The key challenges are:

- **Manual vs. Managed Execution** — Moving from a rigid, `dispatch_to(other_thread)` model to an automated, managed scheduling system.
- **Fairness and Preemption** — A cooperative (FIFO) scheduler can be starved by a single non-cooperative thread. A preemptive system is needed to enforce fair CPU time-sharing.
- **Graceful Termination** — A thread cannot safely deallocate its own stack while it is still running on it. A “zombie” mechanism is required for clean and safe resource cleanup.
- **Concurrency & Safety** — In a preemptive, interrupt-driven environment, the scheduler’s own data structures (like the ready queue) become critical sections. How to protect them from corruption without disabling interrupts entirely?

---

## Our Methods

To solve these problems, we implemented a complete, layered system.  
The solutions for the VM and Scheduler are detailed below.

---

## 1. The Virtual Memory Subsystem

A four-component system was built to manage memory from the physical-bit level to high-level virtual regions.

### **Component 1: Physical Frame Manager (`ContFramePool`)**

This class is the foundation of the memory system.  
It tracks physical memory using a **bitmap** where each frame is represented by 2 bits.  
This allows for three states:
- `Free (01)`
- `Used (00)`
- `Head of Section (HoS) (10)`

The **HoS** marker is critical for managing contiguous blocks, allowing the static `release_frames` function to identify a block’s start and size.

---

### **Component 2: Two-Level Paging & Demand Paging (`PageTable`)**

This class implements the x86 two-level paging mechanism.

- The first 4MB of virtual memory are **direct-mapped** (`virtual == physical`) for the kernel’s code.
- All memory above 4MB is handled via **demand paging**.
- Any access to an unmapped page triggers a **Page Fault (Exception 14)**.
- The `handle_fault` ISR allocates a physical frame from the `ContFramePool`, updates the Page Table Entry (PTE) to map it, and returns, allowing the CPU to resume execution.

---

### **Component 3: Recursive Page Table Mapping**

To solve the scalability problem, the paging structures are moved out of the kernel’s small 4MB pool.  
To access them, the last entry of the Page Directory (PDE) is set to point to the physical address of the Page Directory itself.

This **“recursive trick”** creates a virtual window (e.g., at `0xFFFFF000`) that the kernel’s fault handler can use to find and modify any PDE or PTE using virtual addresses — even when the tables themselves are in paged memory.

---

### **Component 4: Virtual Memory Allocator (`VMPool`)**

This is the high-level `malloc` / `free` equivalent.  
It maintains its own `alloc_list` and `free_list` of virtual memory regions.

- When `allocate()` is called, it **pre-faults** the entire new region by writing to each page, forcing the `handle_fault` mechanism to allocate and map all required physical frames immediately.
- When `release()` is called, it iterates the region, calling `PageTable::free_page()` on each page, which releases the physical frame and flushes the TLB (by reloading `CR3`) to prevent stale translations.

---

## 2. The Kernel-Level Thread Scheduler

This subsystem, built on top of the VM, provides concurrent execution.

### **Component 1: FIFO Scheduler (`Scheduler`)**

This is the base scheduler, implemented with a **doubly linked list** as a ready queue.  
- `Scheduler::resume(thread)` adds to the tail of the queue.  
- `Scheduler::yield()` dispatches the thread from the head, providing a simple FIFO, cooperative multitasking model.

---

### **Component 2: Preemptive Round-Robin Scheduler (`RRScheduler`)**

This subclass of `Scheduler` implements **preemption**.

- A high-frequency `EOQTimer` fires an interrupt (e.g., every **5ms**).
- The timer’s ISR calls `resume()` on the current thread (moving it to the back of the queue) and `yield()` to the next (at the front), forcing a **context switch**.
- To ensure fairness, the overridden `yield()` function resets the timer’s tick count when a thread gives up the CPU voluntarily, granting the next thread a full time slice.

---

### **Component 3: Graceful Thread Termination (Zombie Threads)**

To solve the “can’t free my own stack” problem, a **deferred deletion mechanism** is used.

When a thread function returns, it calls `Scheduler::terminate(this)`.

- The `Scheduler` maintains a `zombieThread` pointer.
- The `terminate` function first deletes the previous zombie (if one exists), then sets the current thread as the new zombie.
- This “parked” zombie’s resources are safely freed by the **next thread** that terminates.

---

### **Component 4: Interrupt-Safe Critical Sections**

To prevent the timer interrupt from corrupting the ready queue, the `resume()` and `yield()` functions are wrapped with:

```cpp
Machine::disable_interrupts();
...
Machine::enable_interrupts();
```

---

## Component 5: I/O-Aware Yield Logic

The scheduler’s `yield(int interrupt)` function was extended so that it can:

- Detect whether a yield came from:
  - a **voluntary thread yield**, or
  - a **disk interrupt bottom-half**.
- Give **priority** to threads waiting on disk operations:
  - If the disk is **free** and the **disk-waiting queue** is non-empty, the scheduler immediately dispatches an I/O-blocked thread.
  - Otherwise, it falls back to the **normal ready-queue** scheduling.

This effectively converts the scheduler into a **dual-queue system**:

- **Ready Queue** — CPU-bound threads  
- **Disk Queue** — managed by the `NonBlockingDisk` driver for I/O-blocked threads  

This mechanism enables **near-zero CPU waste** during disk latency, eliminating all busy-waiting in the driver or scheduler.

---

## Component 6: Integration With Disk Driver Locks

The NonBlockingDisk driver introduces two new locks:

- `hw_lock` — protects ATA **hardware register access**
- `queue_lock` — protects the disk’s **per-device I/O wait queue**

The scheduler must interact safely with these locks. Thus, it now:

- Acquires `queue_lock` internally when inspecting or dequeuing from the disk queue  
- Ensures `yield()` never preempts a thread while it is inside a **disk-critical section**
- Temporarily **disables interrupts** around queue manipulations to prevent races between:
  - the **disk interrupt handler (ISR, bottom half)**, and  
  - the **scheduler’s top-half** logic

These changes create a **unified, interrupt-safe scheduling environment** for both CPU tasks and asynchronous I/O.

---

## Component 7: ISR-Driven Rescheduling

Under **Bonus Option 3** (interrupt-driven disk):

- The ATA controller raises **IRQ14** when a disk operation completes.
- The NonBlockingDisk bottom-half ISR performs:

```cpp
System::SCHEDULER->resume(waitingThread);
System::SCHEDULER->yield(1);   // indicates an interrupt-triggered yield
```
(These backticks are safely closed here — they will NOT break the surrounding block.)

Passing `interrupt = 1` signals to the scheduler:

> “A disk request completed — immediately schedule from the disk queue.”

This ensures that:

- The waiting I/O thread is resumed promptly  
- CPU-bound threads cannot delay I/O completion  
- Each disk interrupt forces a direct, immediate context switch into the next I/O thread  

This replicates real OS device-driver semantics:  
**top-half → initiates I/O**, **bottom-half (ISR) → wakes the blocked thread**.

---

## Component 8: Cooperative–Preemptive Hybrid Behavior

Asynchronous I/O introduces a hybrid scheduling model:

- Threads performing disk I/O voluntarily yield after issuing a request.
- These threads stay suspended until the hardware IRQ wakes them.
- Timer interrupts still enforce standard **preemptive round-robin** among CPU-bound threads.
- Disk interrupts have **higher priority** than timer interrupts:
  - A disk IRQ immediately forces rescheduling.
  - The awakened I/O thread runs at the earliest safe opportunity.

This results in:

- **CPU-bound threads → preemptive RR scheduling**
- **I/O-bound threads → event-driven hardware-IRQ dispatch**

Leading to:  
**maximum CPU utilization**, **zero busy waiting**, and **minimal I/O wake-up latency**.
