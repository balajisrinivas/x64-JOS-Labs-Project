#include "ns.h"

extern union Nsipc nsipcbuf;


void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	struct jif_pkt *packet;
	uintptr_t addr;
	size_t len;
	uint32_t reqType = NSREQ_INPUT;
	int r, recvBuffLen=0;
	packet = (struct jif_pkt *)&(nsipcbuf);
	//Allocate a page for receiving the packet data
        if ((r = sys_page_alloc(0,(void *)((uintptr_t)packet), PTE_P|PTE_U|PTE_W)) < 0)
        	panic("panic in input environment:sys_page_alloc: %e\n", r);
        while(1)
        {
		//Receive the packet from the device driver into the page allocated
		addr = (uintptr_t)packet + sizeof(packet->jp_len);
		len = (size_t)(PGSIZE - sizeof(packet->jp_len));
		while((recvBuffLen = sys_receive_packet((void *)addr,len)) < 0)
                	sys_yield();
                packet->jp_len = recvBuffLen;
		//Send the NSREQ_INPUT IPC message to ns with packet in page
                ipc_send(ns_envid, reqType, packet, PTE_P|PTE_U);
                sys_yield();
        }

}
