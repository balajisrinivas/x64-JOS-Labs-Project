
#include "fs.h"

// Return the virtual address of this disk block.
void*
diskaddr(uint64_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

void eviction_policy()
{
	int i, r, eviction_block_count = 0;
	uint32_t *addr;
	cprintf("Block cache eviction policy in action...\n");
	for(i=3;i<super->s_nblocks && eviction_block_count < EVICT_BLOCK_CACHE_SIZE;i++)
	{
		addr = diskaddr(i);
		//cprintf("addr:%08x\nroundedaddr:%08x\n", addr, ROUNDUP(addr, BLKSIZE    ));
		if(uvpd[VPD(addr)] & PTE_P)
		{
			if(uvpt[PGNUM(addr)] & PTE_P)
			{
				if(uvpt[PGNUM(addr)] & PTE_A)
				{
					r = sys_page_map(0, addr, 0, addr, PTE_SYSCALL);
					if(r<0)
			                 	panic("panic in eviction_policy:sys_page_map: %e", r);
					else
					{
						flush_block(addr);
						r = sys_page_unmap(0, addr);
						eviction_block_count++;
						block_cache_count--;
					}
				}
			}
			else
			{
				eviction_block_count++;
				block_cache_count--;
			}
		}
		else
		{
			eviction_block_count++;
			block_cache_count--;
		}
		cprintf("Disk block number %d evicted from block cache\n",i);
	}
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (uvpml4e[VPML4E(va)] & PTE_P) && (uvpde[VPDPE(va)] & PTE_P) && (uvpd[VPD(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (uvpt[PGNUM(va)] & PTE_D) != 0;
}

// Fault any disk block that is read in to memory by
// loading it from disk.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint64_t blockno = ((uint64_t)addr - DISKMAP) / BLKSIZE;
	int r;
	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_rip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page.
	// Hint: first round addr to page boundary.
	//
	// LAB 5: you code here:

    	void *pgva=ROUNDDOWN(addr,PGSIZE);
        if((r = sys_page_alloc(0, pgva, PTE_U | PTE_P | PTE_W)) < 0)
                panic("bc_pgfault:sys_page_alloc failed");
        uint32_t sectno=blockno*BLKSECTS;
        if((r = ide_read(sectno, pgva, BLKSECTS)) < 0)
                panic("bc_pgfault:ide_read failed");
	if ((r = sys_page_map(0, pgva, 0, pgva, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);


	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	//cprintf("blockisfree:%d\n",block_is_free(blockno));
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);

	block_cache_count++;
	//cprintf("\nInside block cache\tcount:%d\n", block_cache_count);
	if(block_cache_count >= MAX_BLOCK_CACHE_SIZE)
		eviction_policy();
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	int r;
	uint64_t blockno = ((uint64_t)addr - DISKMAP) / BLKSIZE;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	// LAB 5: Your code here.
	void *pgva=(void *) ROUNDDOWN(addr,PGSIZE);
        if((va_is_dirty(pgva)==0) || (va_is_mapped(pgva)==0))
		return;                  
	uint32_t sectno=blockno*BLKSECTS;
        if((r = ide_write(sectno, pgva, BLKSECTS)) < 0)
                panic("flush_block:ide_write failed");
	if ((r = sys_page_map(0, pgva, 0, pgva,  uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
                panic("in flush_block, sys_page_map: %e", r);


}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
	struct Super super;
	set_pgfault_handler(bc_pgfault);
	// cache the super block by reading it once
	memmove(&super, diskaddr(1), sizeof super);



}

