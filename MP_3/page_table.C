#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;



void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
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
      
   // Page Directory will be stored in kernel space - need 4KB (2^10 entries of 4byte each) => 1 frame
   unsigned long pd_frame_no = kernel_mem_pool->get_frames(1);

   //Stop if no frame available
   assert(pd_frame_no != 0);
   page_directory = (unsigned long *)(pd_frame_no * PAGE_SIZE);

   // now page directory points to an address and starting from that address we can access any of the 2^10 bits 
   // directly like an array ( were each entry in array is 4 bytes and shares an address - thats why it is unsigned long)

   unsigned long pt_frame_no  = kernel_mem_pool->get_frames(1);
   assert(pt_frame_no != 0);
   unsigned long * first_page_table = (unsigned long *)(pt_frame_no * PAGE_SIZE);
   // mapping first page table to first entry of page directory (so that it governs first 4MB of memory)
   // also need to set valid and R&W bits.
   page_directory[0] = (unsigned long)first_page_table | 3;

   // one to one mapping for all kernel physical space to the pagetable rows (and hence the pages)
   for(unsigned long pno = 0; pno < ENTRIES_PER_PAGE; pno++){
      first_page_table[pno] = (unsigned long) (pno * PAGE_SIZE) | 3;
   }

   // Map the remaining page tables to the page directory (keeping valid bit = 0)
   for(unsigned long pdno = 1; pdno < ENTRIES_PER_PAGE; pdno++){
      page_directory[pdno] = 0 | 2;
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

void PageTable::handle_fault(REGS * _r)
{
   // for now, we cant handle a protection fault
   assert(!(_r->err_code & 1)); // P bit must be 0

   // get the virtual address in question from cr2 register
   unsigned long cr2 = read_cr2();

   unsigned long * page_directory = current_page_table->page_directory;
   // check if the page directory entry (first 10 bits) for this is valid
   if(!(page_directory[(cr2>>22)] & 1)){
      // The page table for this 4MB region does not exist. We need to create it.
      // Allocate a frame for the new page table from kernel space
      unsigned long new_pt_frame = kernel_mem_pool->get_frames(1);
      assert(new_pt_frame != 0); 

      // a pointer to this new page table's physical memory.
      unsigned long * new_page_table = (unsigned long *)(new_pt_frame * PAGE_SIZE);

      for (unsigned int i = 0; i < ENTRIES_PER_PAGE; i++) {
         // Marked as invalid, but writable and user-accessible
         new_page_table[i] = 2 | 4;
      }

      // Update page table entry for this page table in the page directory
      // present, writable, and user-accessible.
      page_directory[(cr2>>22)] = (unsigned long)new_page_table | 7;
   }

   // Now we have the page table for this region. Toggling the last 12 bits off.
   unsigned long * page_table = (unsigned long *)(page_directory[(cr2>>22)] & ~0xFFF);

   // Allocate a frame
   unsigned long new_page_frame = process_mem_pool->get_frames(1);
   assert(new_page_frame != 0); 
   unsigned long pt_index = (cr2>>12) & 0x3FF; // middle 10 bits
   page_table[pt_index] = (new_page_frame * PAGE_SIZE) | 7;

   // no need to do load() - that is only for context switching

   Console::puts("handled page fault\n");
}

