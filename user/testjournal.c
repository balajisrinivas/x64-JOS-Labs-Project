#include <inc/lib.h>

char *msg1 = "This is the NEW message of the day!";
char *msg2 = "abbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggghhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiijjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk\n";

#define FVA ((struct Fd*)0xCCCCC000)

static int
xopen(const char *path, int mode)
{
        extern union Fsipc fsipcbuf;
        envid_t fsenv;

        strcpy(fsipcbuf.open.req_path, path);
        fsipcbuf.open.req_omode = mode;

        fsenv = ipc_find_env(ENV_TYPE_FS);
        ipc_send(fsenv, FSREQ_OPEN, &fsipcbuf, PTE_P | PTE_W | PTE_U);
        return ipc_recv(NULL, FVA, NULL);
}

void
umain(int argc, char **argv)
{
        int64_t r, f, i;
        struct Fd *fd;
        struct Fd fdcopy;
        struct Stat st;
        char buf[1024];
	
	if ((r = xopen("/new-file", O_RDWR|O_CREAT)) < 0)
                panic("serve_open /new-file: %e", r);

        if ((r = devfile.dev_write(FVA, msg1, strlen(msg1))) != strlen(msg1))
                panic("file_write: %e", r);
	if ((r = devfile.dev_write(FVA, msg2, strlen(msg2))) != strlen(msg2))
                panic("file_write: %e", r);
        cprintf("file_write is good\n");

        FVA->fd_offset = 0;
        memset(buf, 0, sizeof buf);
        if ((r = devfile.dev_read(FVA, buf, sizeof buf)) < 0)
                panic("file_read after file_write: %e", r);
	//cprintf("result:%d\nstrlen sum:%d\n",r,strlen(msg1)+strlen(msg2));
        if (r != (strlen(msg1)+strlen(msg2)))
                panic("file_read after file_write returned wrong length: %d", r);
	strcat(msg1, msg2);
        if (strcmp(buf, msg1) != 0)
                panic("file_read after file_write returned wrong data");
        cprintf("file_read after file_write is good\n");

}

