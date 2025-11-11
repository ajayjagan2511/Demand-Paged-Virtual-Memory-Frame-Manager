/*
 File: vm_pool.C

 Author:
 Date  : 2024/09/20

 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

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
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long _base_address,
               unsigned long _size,
               ContFramePool *_frame_pool,
               PageTable *_page_table)
{
    // Initialize the data structures
    base_address = _base_address;
    // Round up size to multiple of page size
    if (_size % PageTable::PAGE_SIZE != 0)
    {
        _size = (_size / PageTable::PAGE_SIZE + 1) * PageTable::PAGE_SIZE;
    }
    size = _size;
    frame_pool = _frame_pool;
    page_table = _page_table;
    next_pool = nullptr;

    // register this pool with the page table
    page_table->register_pool(this);
    // get frames for the 2 list
    alloc_list = (unsigned long *)base_address;
    free_list = (unsigned long *)(base_address + PageTable::PAGE_SIZE);
    // Accessing these addresses triggers page faults that alots frames to these pages
    // start_address and length for alloc_list page
    alloc_list[0] = base_address;
    alloc_list[1] = PageTable::PAGE_SIZE;
    // start_address and length for free_list page
    alloc_list[2] = base_address + PageTable::PAGE_SIZE;
    alloc_list[3] = PageTable::PAGE_SIZE;
    // set other entries to 0
    for (unsigned long i = 4; i < PageTable::ENTRIES_PER_PAGE - 1; i += 2)
    {
        alloc_list[i] = 0;
        alloc_list[i + 1] = 0;
    }
    // Remove these from free_list
    free_list[0] = base_address + (2 * PageTable::PAGE_SIZE);
    free_list[1] = size - (2 * PageTable::PAGE_SIZE);
    // set other entries to 0
    for (unsigned long i = 2; i < PageTable::ENTRIES_PER_PAGE - 1; i += 2)
    {
        free_list[i] = 0;
        free_list[i + 1] = 0;
    }

    // assert(false);
    Console::puts("Constructed VMPool object.\n");
}

unsigned long VMPool::allocate(unsigned long _size)
{
    if (_size % PageTable::PAGE_SIZE != 0)
    {
        _size = (_size / PageTable::PAGE_SIZE + 1) * PageTable::PAGE_SIZE;
    }

    // Traverse throught the free list to get a chunk that fits it requirement;
    unsigned long iter = 0;
    unsigned long allocated_start;
    // find a chunk that fits
    while (free_list[iter + 1] < _size)
    {
        iter += 2;
        // Assert to ensure we dont exit the page
        assert(iter < PageTable::ENTRIES_PER_PAGE - 1);
    }
    // save the chunk that fits
    allocated_start = free_list[iter];
    // if less than size of free list entry, just edit the chunk out;
    if (free_list[iter + 1] > _size)
    {
        free_list[iter] += _size;
        free_list[iter + 1] -= _size;
    }
    // else is equal to free list entry, shift everything up;
    else
    {
        for (unsigned long i = iter + 2; i < PageTable::ENTRIES_PER_PAGE - 1; i += 2)
        {
            free_list[i - 2] = free_list[i];
            free_list[i - 1] = free_list[i + 1];
            // clear the last entry if we have shifted everything
            if (i + 2 >= PageTable::ENTRIES_PER_PAGE - 1)
            {
                free_list[i] = 0;
                free_list[i + 1] = 0;
            }
        }
    }
    // traverse to the end of alloc list and add this chunk there
    iter = 0;
    while (alloc_list[iter + 1] != 0)
    {
        iter += 2;
        // atleast one space must be free (to add this chunk)
        assert(iter < PageTable::ENTRIES_PER_PAGE - 3);
    }
    alloc_list[iter] = allocated_start;
    alloc_list[iter + 1] = _size;
    // access all pages in this region to trigger page faults and allocate frames (in case we want eager allocation)
    // for (unsigned long i = allocated_start; i < allocated_start + _size; i += PageTable::PAGE_SIZE)
    // {
    //     unsigned long temp = *((unsigned long *)i);
    // }
    // assert(false);
    Console::puts("Allocated region of memory.\n");
    return allocated_start;
}

void VMPool::release(unsigned long _start_address)
{
    unsigned long iter = 0;
    while (alloc_list[iter] != _start_address)
    {
        iter += 2;
        // Assert to ensure we dont exit the page
        assert(iter < PageTable::ENTRIES_PER_PAGE - 1);
    }
    // save the chunk to be removed
    unsigned long free_start = alloc_list[iter];
    unsigned long free_length = alloc_list[iter + 1];
    assert(free_start % PageTable::PAGE_SIZE == 0);
    assert(free_length % PageTable::PAGE_SIZE == 0);
    
    // freeing the frames for these pages  
    for (unsigned long i = free_start; i < free_start + free_length; i += PageTable::PAGE_SIZE)
    {
        unsigned long page_no = i / PageTable::PAGE_SIZE;
        page_table->free_page(page_no);
    }
    
    // remove chunk form alloc list and shift entries up
    alloc_list[iter] = 0;
    alloc_list[iter + 1] = 0;
    for (unsigned long i = iter + 2; i < PageTable::ENTRIES_PER_PAGE - 1; i += 2)
    {
        alloc_list[i - 2] = alloc_list[i];
        alloc_list[i - 1] = alloc_list[i + 1];
        // clear the last entry if we have shifted everything
        if (i + 2 >= PageTable::ENTRIES_PER_PAGE - 1)
        {
            alloc_list[i] = 0;
            alloc_list[i + 1] = 0;
        }
    }
    // add chunk to free list
    iter = 0;
    while (free_list[iter + 1] != 0)
    {
        iter += 2;
        // atleast one space must be free (to add this chunk)
        assert(iter < PageTable::ENTRIES_PER_PAGE - 3);
    }
    // assign chunk to first empty space in free list
    free_list[iter] = free_start;
    free_list[iter+1]= free_length;

    
    // assert(false);
    Console::puts("Released region of memory.\n");
}

bool VMPool::is_legitimate(unsigned long _address)
{
    // Metadata pages (alloc_list and free_list) must always be treated as valid
    unsigned long alloc_base = (unsigned long)alloc_list;
    if (_address >= alloc_base && _address < alloc_base + PageTable::PAGE_SIZE)
        return true;
    unsigned long free_base = (unsigned long)free_list;
    if (_address >= free_base && _address < free_base + PageTable::PAGE_SIZE)
        return true;

    // Traverse through the allocated list to check if _address is allocated
    for (unsigned long i = 0; i < PageTable::ENTRIES_PER_PAGE - 1; i += 2)
    {
        if ((_address >= alloc_list[i]) && (_address < alloc_list[i] + alloc_list[i + 1]))
            return true;
    }
    // assert(false);
    Console::puts("Checked whether address is part of an allocated region.\n");
    return false;
}
