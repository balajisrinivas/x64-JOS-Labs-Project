#include <inc/string.h>

#include "fs.h"

// --------------------------------------------------------------
// Super block
// --------------------------------------------------------------

// Validate the file system super-block.
void
check_super(void)
{
	if (super->s_magic != FS_MAGIC)
		panic("bad file system magic number");

	if (super->s_nblocks > DISKSIZE/BLKSIZE)
		panic("file system is too large");

	check_root_dir(super->s_root);	

	//cprintf("Number of blocks in FS:%d\n",super->s_nblocks);
	cprintf("Superblock is good\n");
}

void check_root_dir(struct File f)
{
	int i, j;
	struct File rootdir = f;
	cprintf("Checking root directory files...\n");
	cprintf("name:%s\tsize:%d\ttype:%d\n", rootdir.f_name, rootdir.f_size, rootdir.f_type);
	if(rootdir.f_type == FTYPE_DIR){
	for(i=0; i<NDIRECT; i++)
	{
		//cprintf("direct block pointer %d :%08x\n", i, rootdir.f_direct[i]);		
		if(rootdir.f_direct[i] != 0)
		{
			assert(rootdir.f_direct[i]!=1);
			assert(rootdir.f_direct[i]<JOURNALSTARTBLOCK || rootdir.f_direct[i]>JOURNALSTARTBLOCK+MAXJOURNALS);
			
			struct File * rootdirfile = (struct File *)(diskaddr(rootdir.f_direct[i]));
			int sizeSum = 0;
			while(sizeSum < rootdir.f_size)
			{
				cprintf("\tname:%s\tsize:%d\ttype:%d\n", rootdirfile->f_name, rootdirfile->f_size, rootdirfile->f_type);
				sizeSum += sizeof(struct File);
				rootdirfile++;
				//if(rootdirfile->f_type == 1)
				//	check_root_dir(*rootdirfile, level+1);
			}
		}
	}
	}
	//cprintf("indirect block pointer:%08x\n", rootdir.f_indirect);
	if(rootdir.f_indirect != 0)
	{
		struct File *indir_block = (struct File *)(diskaddr(rootdir.f_indirect));
		//check_root_dir(*indir_block, level+1);			
	}
	cprintf("Root directory files are good\n");
}

// --------------------------------------------------------------
// Free block bitmap
// --------------------------------------------------------------

// Check to see if the block bitmap indicates that block 'blockno' is free.
// Return 1 if the block is free, 0 if not.
bool
block_is_free(uint32_t blockno)
{
	if (super == 0 || blockno >= super->s_nblocks)
		return 0;
	if (bitmap[blockno / 32] & (1 << (blockno % 32)))
		return 1;
	return 0;
}

// Mark a block free in the bitmap
void
free_block(uint32_t blockno)
{
	// Blockno zero is the null pointer of block numbers.
	if (blockno == 0)
		panic("attempt to free zero block");
	bitmap[blockno/32] |= 1<<(blockno%32);
}

// Search the bitmap for a free block and allocate it.  When you
// allocate a block, immediately flush the changed bitmap block
// to disk.
//
// Return block number allocated on success,
// -E_NO_DISK if we are out of blocks.
//
// Hint: use free_block as an example for manipulating the bitmap.
int
alloc_block(void)
{
	// The bitmap consists of one or more blocks.  A single bitmap block
	// contains the in-use bits for BLKBITSIZE blocks.  There are
	// super->s_nblocks blocks in the disk altogether.

	// LAB 5: Your code here.

	int i;
        i=1;
        while(i< super->s_nblocks)
        {
                if(block_is_free(i)==1)
                {
                        bitmap[i/32] &= ~(1<<(i%32));
                        flush_block(diskaddr(2));
                        return i;
                }

                i++;
        }

	return -E_NO_DISK;


}	

// Validate the file system bitmap.
//
// Check that all reserved blocks -- 0, 1, and the bitmap blocks themselves --
// are all marked as in-use.
void
check_bitmap(void)
{
	uint32_t i;

	// Make sure all bitmap blocks are marked in-use
	for (i = 0; i * BLKBITSIZE < super->s_nblocks; i++)
		assert(!block_is_free(2+i));

	// Make sure the reserved and root blocks are marked in-use.
	assert(!block_is_free(0));
	assert(!block_is_free(1));

	cprintf("Free block bitmap is good\n");
}


void check_journal(void)
{
	int i, journal_count =get_journal_size();
	cprintf("Journal record count:%d\n",journal_count);		
	assert(journal_count < MAXJOURNALS);
	
	for(i = JOURNALSTARTBLOCK; i < JOURNALSTARTBLOCK + MAXJOURNALS; i++)
		assert(!block_is_free(i));

	cprintf("Journal blocks are good\n");
}

// --------------------------------------------------------------
// File system structures
// --------------------------------------------------------------

// Initialize the file system
void
fs_init(void)
{
	static_assert(sizeof(struct File) == 256);

	// Find a JOS disk.  Use the second IDE disk (number 1) if available.
	if (ide_probe_disk1())
		ide_set_disk(1);
	else
		ide_set_disk(0);

	bc_init();

	// Set "super" to point to the super block.
	super = diskaddr(1);

	// Set "bitmap" to the beginning of the first bitmap block.
	bitmap = diskaddr(2);

	//Set "journal" to point to the beginning of the first journal block
	journal = diskaddr(5000);
	
	fs_check();
	
	replay_journal();	
}

void fs_check(void)
{
	cprintf("\n\n\nStarting file system check...\n");
	cprintf("Checking super block...\n");
	check_super();

	cprintf("Checking free block bitmap...\n");
        check_bitmap();

	cprintf("Checking journal blocks...\n");
        check_journal();

	cprintf("File system check complete...\n\n\n");

}
// Find the disk block number slot for the 'filebno'th block in file 'f'.
// Set '*ppdiskbno' to point to that slot.
// The slot will be one of the f->f_direct[] entries,
// or an entry in the indirect block.
// When 'alloc' is set, this function will allocate an indirect block
// if necessary.
//
//  Note, for the read-only file system (lab 5 without the challenge), 
//        alloc will always be false.
//
// Returns:
//	0 on success (but note that *ppdiskbno might equal 0).
//	-E_NOT_FOUND if the function needed to allocate an indirect block, but
//		alloc was 0.
//	-E_NO_DISK if there's no space on the disk for an indirect block.
//	-E_INVAL if filebno is out of range (it's >= NDIRECT + NINDIRECT).
//
// Analogy: This is like pgdir_walk for files.
// Hint: Don't forget to clear any block you allocate.
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
  
	uint32_t *indirectblkptr;
        int blockno,allocated_block;
        if(filebno>=NDIRECT+NINDIRECT)
                return -E_INVAL;
        if(filebno<NDIRECT)
        {
                *ppdiskbno=&f->f_direct[filebno];
                return 0;
        }
        else
        {
                if(f->f_indirect==0)
                {
                        if(alloc == false)
                                return -E_NOT_FOUND;
                }
                if(f->f_indirect==0)
                {
                        allocated_block = alloc_block();
                        if(allocated_block < 0)
                        {
                                return -E_NO_DISK;

                        }
                        f->f_indirect = allocated_block;
                        memset((uint32_t*)diskaddr(f->f_indirect),0,BLKSIZE);


                }
                indirectblkptr=(uint32_t*)diskaddr(f->f_indirect);
                blockno=filebno-NDIRECT;
                *ppdiskbno=&indirectblkptr[blockno];
                return 0;
        }

}

// Set *blk to the address in memory where the filebno'th
// block of file 'f' would be mapped.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_NO_DISK if a block needed to be allocated but the disk is full.
//	-E_INVAL if filebno is out of range.
//
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{

 // LAB 5: Your code here.
        uint32_t *ppdiskbno;
        int r,allocated_block;
        r = file_block_walk(f, filebno, &ppdiskbno, 1);
        if(r<0)
                return r;


        if ((*ppdiskbno) == 0)
        {
                allocated_block = alloc_block();
                if(allocated_block < 0)
                {
                                return -E_NO_DISK;
                }
                *ppdiskbno = allocated_block;
        }
         *blk=(char*)diskaddr(*ppdiskbno);
	return 0;

}


// Try to find a file named "name" in dir.  If so, set *file to it.
//
// Returns 0 and sets *file on success, < 0 on error.  Errors are:
//	-E_NOT_FOUND if the file is not found
static int
dir_lookup(struct File *dir, const char *name, struct File **file)
{
	int r;
	uint32_t i, j, nblock;
	char *blk;
	struct File *f;

	// Search dir for name.
	// We maintain the invariant that the size of a directory-file
	// is always a multiple of the file system's block size.
	assert((dir->f_size % BLKSIZE) == 0);
	nblock = dir->f_size / BLKSIZE;
	for (i = 0; i < nblock; i++) {
		if ((r = file_get_block(dir, i, &blk)) < 0)
			return r;
		f = (struct File*) blk;
		for (j = 0; j < BLKFILES; j++)
			if (strcmp(f[j].f_name, name) == 0) {
				*file = &f[j];
				return 0;
			}
	}
	return -E_NOT_FOUND;
}

// Set *file to point at a free File structure in dir.  The caller is
// responsible for filling in the File fields.
static int
dir_alloc_file(struct File *dir, struct File **file)
{
	int r;
	uint32_t nblock, i, j;
	char *blk;
	struct File *f;

	assert((dir->f_size % BLKSIZE) == 0);
	nblock = dir->f_size / BLKSIZE;
	for (i = 0; i < nblock; i++) {
		if ((r = file_get_block(dir, i, &blk)) < 0)
			return r;
		f = (struct File*) blk;
		for (j = 0; j < BLKFILES; j++)
			if (f[j].f_name[0] == '\0') {
				*file = &f[j];
				return 0;
			}
	}
	dir->f_size += BLKSIZE;
	if ((r = file_get_block(dir, i, &blk)) < 0)
		return r;
	f = (struct File*) blk;
	*file = &f[0];
	return 0;
}

// Skip over slashes.
static const char*
skip_slash(const char *p)
{
	while (*p == '/')
		p++;
	return p;
}

// Evaluate a path name, starting at the root.
// On success, set *pf to the file we found
// and set *pdir to the directory the file is in.
// If we cannot find the file but find the directory
// it should be in, set *pdir and copy the final path
// element into lastelem.
static int
walk_path(const char *path, struct File **pdir, struct File **pf, char *lastelem)
{
	const char *p;
	char name[MAXNAMELEN];
	struct File *dir, *f;
	int r;
	// if (*path != '/')
	//	return -E_BAD_PATH;
	path = skip_slash(path);
	f = &super->s_root;
	dir = 0;
	name[0] = 0;

	if (pdir)
		*pdir = 0;
	*pf = 0;
	while (*path != '\0') {
		dir = f;
		p = path;
		while (*path != '/' && *path != '\0')
			path++;
		if (path - p >= MAXNAMELEN)
			return -E_BAD_PATH;
		memmove(name, p, path - p);
		name[path - p] = '\0';
		path = skip_slash(path);

		if (dir->f_type != FTYPE_DIR)
			return -E_NOT_FOUND;

		if ((r = dir_lookup(dir, name, &f)) < 0) {
			if (r == -E_NOT_FOUND && *path == '\0') {
				if (pdir)
					*pdir = dir;
				if (lastelem)
					strcpy(lastelem, name);
				*pf = 0;
			}
			return r;
		}
	}

	if (pdir)
		*pdir = dir;
	*pf = f;
	return 0;
}

// --------------------------------------------------------------
// File operations
// --------------------------------------------------------------

// Create "path".  On success set *pf to point at the file and return 0.
// On error return < 0.
int
file_create(const char *path, struct File **pf)
{

	char name[MAXNAMELEN];
	int r;
	struct File *dir, *f;
	if ((r = walk_path(path, &dir, &f, name)) == 0)
		//return -1;
		return -E_FILE_EXISTS;
	

	if (r != -E_NOT_FOUND || dir == 0)
		return r;
	if ((r = dir_alloc_file(dir, &f)) < 0)
		return r;
	strcpy(f->f_name, name);
	*pf = f;
	int num;
	//cprintf("NAME OF FILE: %s",(*pf)->f_name);
	file_flush(dir);
	//cprintf("Creation of file was succcessfull!");
	return 0;
}

// Open "path".  On success set *pf to point at the file and return 0.
// On error return < 0.
int
file_open(const char *path, struct File **pf)
{
	return walk_path(path, 0, pf, 0);
}

// Read count bytes from f into buf, starting from seek position
// offset.  This meant to mimic the standard pread function.
// Returns the number of bytes read, < 0 on error.
ssize_t
file_read(struct File *f, void *buf, size_t count, off_t offset)
{
	int r, bn;
	off_t pos;
	char *blk;

	if (offset >= f->f_size)
		return 0;

	count = MIN(count, f->f_size - offset);

	for (pos = offset; pos < offset + count; ) {
		if ((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
			return r;
		bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
		memmove(buf, blk + pos % BLKSIZE, bn);
		pos += bn;
		buf += bn;
	}

	return count;
}


// Write count bytes from buf into f, starting at seek position
// offset.  This is meant to mimic the standard pwrite function.
// Extends the file if necessary.
// Returns the number of bytes written, < 0 on error.
int
file_write(struct File *f, const void *buf, size_t count, off_t offset)
{
	int r, bn;
	off_t pos;
	char *blk;
	
	r = journal_write(f, buf, count, offset);
        if(r < 0)
                return r;
	
	r = journal_commit(f, buf, count, offset);
	if(r < 0)
                return r;

	r = journal_checkpoint();
	if(r < 0)
		return r;

// /*	
	// Extend file if necessary
	if (offset + count > f->f_size)
		if ((r = file_set_size(f, offset + count)) < 0)
			return r;

	for (pos = offset; pos < offset + count; ) {
		if ((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
			return r;	
		bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
		memmove(blk + pos % BLKSIZE, buf, bn);
		pos += bn;
		buf += bn;
	}
// */	
	return count;
}

// Remove a block from file f.  If it's not there, just silently succeed.
// Returns 0 on success, < 0 on error.
static int
file_free_block(struct File *f, uint32_t filebno)
{
	int r;
	uint32_t *ptr;

	if ((r = file_block_walk(f, filebno, &ptr, 0)) < 0)
		return r;
	if (*ptr) {
		free_block(*ptr);
		*ptr = 0;
	}
	return 0;
}

// Remove any blocks currently used by file 'f',
// but not necessary for a file of size 'newsize'.
// For both the old and new sizes, figure out the number of blocks required,
// and then clear the blocks from new_nblocks to old_nblocks.
// If the new_nblocks is no more than NDIRECT, and the indirect block has
// been allocated (f->f_indirect != 0), then free the indirect block too.
// (Remember to clear the f->f_indirect pointer so you'll know
// whether it's valid!)
// Do not change f->f_size.
static void
file_truncate_blocks(struct File *f, off_t newsize)
{
	int r;
	uint32_t bno, old_nblocks, new_nblocks;

	old_nblocks = (f->f_size + BLKSIZE - 1) / BLKSIZE;
	new_nblocks = (newsize + BLKSIZE - 1) / BLKSIZE;
	for (bno = new_nblocks; bno < old_nblocks; bno++)
		if ((r = file_free_block(f, bno)) < 0)
			cprintf("warning: file_free_block: %e", r);

	if (new_nblocks <= NDIRECT && f->f_indirect) {
		free_block(f->f_indirect);
		f->f_indirect = 0;
	}
}

// Set the size of file f, truncating or extending as necessary.
int
file_set_size(struct File *f, off_t newsize)
{
	if (f->f_size > newsize)
		file_truncate_blocks(f, newsize);
	f->f_size = newsize;
	flush_block(f);
	return 0;
}

// Flush the contents and metadata of file f out to disk.
// Loop over all the blocks in file.
// Translate the file block number into a disk block number
// and then check whether that disk block is dirty.  If so, write it out.
void
file_flush(struct File *f)
{
	int i;
	uint32_t *pdiskbno;

	for (i = 0; i < (f->f_size + BLKSIZE - 1) / BLKSIZE; i++) {
		if (file_block_walk(f, i, &pdiskbno, 0) < 0 ||
		    pdiskbno == NULL || *pdiskbno == 0)
			continue;
		flush_block(diskaddr(*pdiskbno));
	}
	flush_block(f);
	if (f->f_indirect)
		flush_block(diskaddr(f->f_indirect));
}

// Remove a file by truncating it and then zeroing the name.
int
file_remove(const char *path)
{
	int r;
	struct File *f;

	if ((r = walk_path(path, 0, &f, 0)) < 0)
		return r;

	file_truncate_blocks(f, 0);
	f->f_name[0] = '\0';
	f->f_size = 0;
	flush_block(f);

	return 0;
}

// Sync the entire file system.  A big hammer.
void
fs_sync(void)
{
	int i;
	for (i = 1; i < super->s_nblocks; i++)
		flush_block(diskaddr(i));
}

//----------------------------
// Journal methods
//----------------------------
	
//Method to perform a journal write of transaction data and metadata
int journal_write(struct File *f, const void *buf, size_t count, off_t offset)
{
	cprintf("[1/3] Starting journal write...\n");

	//uint32_t *journal_count = (uint32_t *)journal;
	//cprintf("journal count:%d\n", journal_count);
	//cprintf("count:%d\n",count);
	uint32_t i;
	struct Journal *cur_journal = (struct Journal *)(journal);
	cur_journal += journal_count;
        
	cur_journal->txBegin = 1;
	
	for(i = 0; i < super->s_nblocks; i+=32)
		cur_journal->bmap[i/32] =  bitmap[i/32]; 
	/*for(i = 0; i < super->s_nblocks; i+=32)
	{
                cprintf("bitmap[%d]:%08x\n",i/32, bitmap[i/32]);
		cprintf("bmap[%d]:%08x\n",i/32, cur_journal->bmap[i/32]);
	}*/
	
	strcpy(cur_journal->file.f_name, f->f_name);
	cur_journal->file.f_size = f->f_size;
	cur_journal->file.f_type = f->f_type;
	for(i = 0; i < NDIRECT ; i++)
		cur_journal->file.f_direct[i] = f->f_direct[i];
	cur_journal->file.f_indirect = f->f_indirect;
	for(i = 0; i< (256 - MAXNAMELEN - 8 - 4*NDIRECT - 4); i++)
		cur_journal->file.f_pad[i] = f->f_pad[i];

	cur_journal->count = count;

	memmove(cur_journal->buffer, buf, count);
	//cprintf("journal buffer:%s\nbuffer:%s\n", cur_journal->buffer, buf, count);
	
	cur_journal->offset = offset;
	
	cur_journal->txEnd = 0;	
	
	journal_count++;
	cprintf("[1/3] Journal write complete...\n");
	//cprintf("journal count:%d\n", journal_count);

	return 0;
}

//Method to perform a journal commit
int journal_commit(struct File *f, const void *buf, size_t count, off_t offset)
{
	cprintf("[2/3] Starting journal commit...\n");
	journal_count--;
	struct Journal *prev_journal = (struct Journal *)(journal);
	prev_journal += journal_count;
	uint32_t i;
        
	assert(prev_journal->txBegin == 1);
	
	assert(strcmp(f->f_name, prev_journal->file.f_name) == 0);
	assert(f->f_size == prev_journal->file.f_size);
	assert(f->f_type == prev_journal->file.f_type);
	for(i = 0; i < NDIRECT ; i++)
                assert(prev_journal->file.f_direct[i] == f->f_direct[i]);
        assert(prev_journal->file.f_indirect == f->f_indirect);
        for(i = 0; i< (256 - MAXNAMELEN - 8 - 4*NDIRECT - 4); i++)
                assert(prev_journal->file.f_pad[i] == f->f_pad[i]);

	assert(prev_journal->count == count);	

	char *charbuf = (char *)buf;
	for(i = 0; i < count; i++)
		assert(*(charbuf+i) == prev_journal->buffer[i]);  

	assert(prev_journal->offset == offset);

	assert(prev_journal->txEnd == 0);
	cprintf("[2/3] Journal record is in a consistent state\n");
	
	prev_journal->txEnd = 1;
	journal_count++;
	cprintf("[2/3] Journal commit complete...\n");
	return 0;
}

//Method to perform a journal checkpoint.Flushes the journal blocks to disk
int journal_checkpoint()
{
	cprintf("[3/3] Starting journal checkpointing...\n");
	journal_count--;
        struct Journal *prev_journal = (struct Journal *)(journal);
	prev_journal += journal_count;
	assert(prev_journal->txBegin == 1);
	assert(prev_journal->txEnd == 1);
	//cprintf("journal start addr:%08x\nisdirty:%d\n", prev_journal, va_is_dirty(prev_journal));
	//cprintf("journal end addr:%08x\nisdirty:%d\n", prev_journal+1, va_is_dirty(prev_journal));
	flush_block(prev_journal);
	flush_block(prev_journal+1);
	//cprintf("journal start addr:%08x\nisdirty:%d\n", prev_journal, va_is_dirty(prev_journal));
        //cprintf("journal end addr:%08x\nisdirty:%d\n", prev_journal+1, va_is_dirty(prev_journal));	
	journal_count++;
	cprintf("[3/3] Journal checkpointing complete...\n\n\n");
	return 0; 
}

//Method that replays the journal records from journal blocks during FS initialization
void replay_journal()
{
	int journal_counter, success_counter, r, i;
	cprintf("Replaying journal records...\n");
	struct Journal *itjournal = (struct Journal *)(journal);
	for(journal_counter = 0, success_counter = 0;itjournal->txBegin==1;itjournal++, journal_counter++)
	{
		cprintf("Replaying journal record %d...\n",journal_counter);
		//cprintf("txbegin:%d\n",itjournal->txBegin);
		//i++;
		assert(itjournal->txBegin == itjournal->txEnd);

		//Replaying bitmap block
		for(i = 0; i < super->s_nblocks; i+=32)
                	bitmap[i/32] = itjournal->bmap[i/32];
	
	
		cprintf("\t[1/3] Writing file %s with data %s at offset %d...\n", itjournal->file.f_name, itjournal->buffer, itjournal->offset);
		r = journal_file_write(&itjournal->file, itjournal->buffer, itjournal->count, itjournal->offset);
		file_flush(&itjournal->file);
		if(r < 0)
		{
			cprintf("journal_file-write failed:%e\n",r);
			continue;
		}
		//cprintf("return value of journal_file_write:%d\n", r);
		cprintf("\t[2/3] File %s written with data %s at offset %d\n", itjournal->file.f_name, itjournal->buffer, itjournal->offset);
		success_counter += 1;
		
		char resultBuf[BLKSIZE/4];
		r = file_read(&itjournal->file, resultBuf, itjournal->count, itjournal->offset);
		if(r<0 && (strcmp(resultBuf, itjournal->buffer) != 0))
		{
			cprintf("file_read after journal_file_write failed\n");
			continue;
		}
		cprintf("\tresult of file_read after journal write:%s\n", resultBuf);
		cprintf("\t[3/3] file_read after journal_file_write successful\n");
		cprintf("Replaying journal record %d complete\n\n\n",journal_counter);
	}
	cprintf("Number of journal records\t\t\t:%d\n", journal_counter);
	cprintf("Number of records successfully replayed\t\t:%d\n", success_counter);
	cprintf("Number of records unsuccessfully replayed\t:%d\n\n\n", journal_counter - success_counter); 
}

int
journal_file_write(struct File *f, const void *buf, size_t count, off_t offset)
{
        int r, bn;
        off_t pos;
        char *blk;

	//cprintf("Received journal_file_write request for file pointer:%08x\tbuffer:%08x\tcount:%d\toffset:%d\n", f, buf, count, offset); 
        // Extend file if necessary
        if (offset + count > f->f_size)
	{
		//cprintf("offset + count > f->f_size");
                if ((r = file_set_size(f, offset + count)) < 0)
                        return r;
	}
        for (pos = offset; pos < offset + count; ) 
	{
                if ((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
                        return r;
		//cprintf("file_get_block successful. blk:%08x\n",blk);
                bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
		//cprintf("blk+pos%BLKSIZE=%08x\t buf=%08x\t bn=%d\n", blk+pos%BLKSIZE, buf, bn);
                memmove(blk + pos % BLKSIZE, buf, bn);
		//cprintf("memmove successful\n");
                pos += bn;
                buf += bn;
        }

        return count;
}

int get_journal_size()
{
	struct Journal *itjournal = (struct Journal *)(journal);
	int journal_counter;
        for(journal_counter = 0; itjournal->txBegin==1; itjournal++,journal_counter++);
        return journal_counter;
}

