#include "ns.h"
extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";
	int32_t reqType;
	int perm;	
	envid_t envid_sender;
	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	while(1)
	{
		perm = 0;
		//Read a packet from ns
		reqType = ipc_recv(&envid_sender, &nsipcbuf, &perm);
		//Check if ipc_recv has received correctly
		if(!(perm & PTE_P))
		{
			cprintf("Invalid request from network server[%08x]:no page",envid_sender);
			continue; 
		}
		if(reqType != NSREQ_OUTPUT)
		{
			cprintf("Invalid request from network server[%08x]:not a NSREQ_OUTPUT message",envid_sender);
			continue;
		}
		//Send packet to device driver.If packet send fails, give up CPU
		while(sys_send_packet(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len) < 0)
			sys_yield();
	}
}
