#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable *PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool *PageTable::kernel_mem_pool = nullptr;
ContFramePool *PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;

void PageTable::init_paging(ContFramePool *_kernel_mem_pool,
                            ContFramePool *_process_mem_pool,
                            const unsigned long _shared_size)
{
   kernel_mem_pool = _kernel_mem_pool;
   process_mem_pool = _process_mem_pool;
   shared_size = _shared_size;
   // initializing just the one time static variables for the rpocess here.
   Console::puts("Initialized Paging System\n");
}

PageTable::PageTable()
{
   // Two modes: paging enabled and paging disabled
   // We handle only the Paging disabled mode here.
   // With paging Enabled, we wont have direct access to physical memory, so we handle that inside the interrupt.
   // Paging Disabled: initalize a frame for page directory in kernel pool and assign
   // Also initalize a page for kernel memory and do one to one mapping
   head_pool = nullptr;
   // Page Directory will be stored in kernel space - need 4KB (2^10 entries of 4byte each) => 1 frame
   unsigned long pd_frame_no = process_mem_pool->get_frames(1);

   // Stop if no frame available
   assert(pd_frame_no != 0);
   page_directory = (unsigned long *)(pd_frame_no * PAGE_SIZE);

   // now page directory points to an address and starting from that address we can access any of the 2^10 bits
   // directly like an array ( were each entry in array is 4 bytes and shares an address - thats why it is unsigned long)

   unsigned long pt_frame_no = process_mem_pool->get_frames(1);
   assert(pt_frame_no != 0);
   unsigned long *first_page_table = (unsigned long *)(pt_frame_no * PAGE_SIZE);
   // mapping first page table to first entry of page directory (so that it governs first 4MB of memory)
   // also need to set valid and R&W bits.
   page_directory[0] = (unsigned long)first_page_table | 3;

   // one to one mapping for all kernel physical space to the pagetable rows (and hence the pages)
   for (unsigned long pno = 0; pno < ENTRIES_PER_PAGE; pno++)
   {
      first_page_table[pno] = (unsigned long)(pno * PAGE_SIZE) | 3;
   }

   // Map the remaining page tables to the page directory (keeping valid bit = 0)
   for (unsigned long pdno = 1; pdno < ENTRIES_PER_PAGE; pdno++)
   {
      page_directory[pdno] = 0 | 2;
      // Recursive Mapping - last entry points to page directory itself
      if (pdno == ENTRIES_PER_PAGE - 1)
      {
         page_directory[pdno] = (unsigned long)(pd_frame_no * PAGE_SIZE) | 3;
      }
   }

   Console::puts("Constructed Page Table object\n");
}

void PageTable::load()
{
   // write the page directory address to CR3 to tell the CPU
   write_cr3((unsigned long)this->page_directory);
   // point the static variable to current page table
   current_page_table = this;
   Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
   // Get the current CR0 value
   unsigned long cr0 = read_cr0();

   // Set the paging and protection-enable bits (bits 31 and 0)
   cr0 = cr0 | 0x80000001;

   // Write the new value back to activate paging
   write_cr0(cr0);

   // Update the static variable
   paging_enabled = 1;
   Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS *_r)
{
   // for now, we cant handle a protection fault
   assert(!(_r->err_code & 1)); // P bit must be 0

   // get the virtual address in question from cr2 register
   unsigned long cr2 = read_cr2();
   // ensure the faulting address belongs to a registered VM pool
   // uncomment for part 2 and 3 when access only through VM pool.
   // assert(current_page_table != nullptr);
   // bool check = false;
   // VMPool *node = current_page_table->head_pool;
   // while (node != nullptr && !check)
   // {
   //    check = node->is_legitimate(cr2);
   //    node = node->next_pool;
   // }
   // assert(check);

   // Now since no one to one mapping exists for process memory
   // CPU cant access page directory properly, so we need to access it via recursive mapping
   // we do this by accessing the last 4MB of virtual memory which maps to page directory itself
   // needs to be (1023 | 1023 | X (PDE) | 00)
   unsigned long *pde = (unsigned long *)(((cr2 >> 22) << 2) | 0xFFFFF000);
   if (!(*pde & 1))
   {
      // The page table for this 4MB region does not exist. We need to create it.
      // Allocate a frame for the new page table from kernel space
      unsigned long new_pt_frame = process_mem_pool->get_frames(1);
      assert(new_pt_frame != 0);

      *pde = (unsigned long)(new_pt_frame * PAGE_SIZE) | 7;

      // now to traverse the PT, we get the virtual address of PT first with recursive mapping.
      // it should be (1023 | X (PDE) | 0 | 00)
      // a pointer to this new page table's virtual memory.
      unsigned long *page_table = (unsigned long *)(((cr2 >> 22) << 12) | 0xFFC00000);

      for (unsigned int i = 0; i < ENTRIES_PER_PAGE; i++)
      {
         // Marked as invalid, but writable and user-accessible
         *page_table = 2 | 4;
         // Recursive Mapping - last entry points to page table itself
         if (i == ENTRIES_PER_PAGE - 1)
         {
            *page_table = (unsigned long)(new_pt_frame * PAGE_SIZE) | 7;
         }
         page_table++;
      }
   }

   // Again for recursive mapping
   // it should be (1023 | X (PDE) | Y (PTE) | 00)
   unsigned long *pte = (unsigned long *)(((cr2 >> 12) << 2) | 0xFFC00000);

   // Allocate a frame
   unsigned long new_page_frame = process_mem_pool->get_frames(1);
   assert(new_page_frame != 0);
   *pte = (new_page_frame * PAGE_SIZE) | 7;

   // no need to do load() - that is only for context switching

   Console::puts("handled page fault\n");
}

void PageTable::register_pool(VMPool *_vm_pool)
{
   _vm_pool->next_pool = head_pool;
   head_pool = _vm_pool;
   //  assert(false);
   Console::puts("registered VM pool\n");
}

void PageTable::free_page(unsigned long _page_no)
{
   // check if _page_no is legitimate as it is coming from release
   bool check=false;
   unsigned long page_address = _page_no * PAGE_SIZE;
   VMPool * node = head_pool;
   while(node!=nullptr && !check){
      check = node->is_legitimate(page_address);
      node = node->next_pool;
   }
   // check if page not legit in any pools
   assert(check);
   // get frame number from page number
   unsigned long virtual_address = _page_no * PAGE_SIZE;
   // recursive mapping
   unsigned long *pte = (unsigned long *)(((virtual_address >> 12) << 2) | 0xFFC00000);
   //before releasing a frame, we must check if pte is valid, else we can return without freezing
   unsigned long frame_number = (*pte)/PAGE_SIZE;
   if((*pte & 0x00000001) == 0x00000001)
   {
      // release frame;
      process_mem_pool->release_frames(frame_number);
      //mark pte invalid
      *pte &= ~0x00000001;

      //flush TLB by reloading CR3 with load
      this->load();
   }

   // assert(false);
   Console::puts("freed page\n");
}
