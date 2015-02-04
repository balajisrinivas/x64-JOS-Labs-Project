#include <kern/e1000.h>
#include <kern/pci.h>
#include <inc/string.h>

// LAB 6: Your driver code here
volatile uint32_t *pci_mmio_base;

uint32_t e1000_mac_address[2] = {0x12005452, 0x5634};

static void descriptors_init()
{
	int a;
	//Initialize the transmit descriptor ring
	for(a=0;a<NUM_TX_DESC;a++)
	{
		tranDescRing[a].addr = PADDR(&tranBuf[a]);
		tranDescRing[a].length = 0;
		tranDescRing[a].cso = 0;		
		tranDescRing[a].status = TXD_STAT_DD;
		tranDescRing[a].cmd = TXD_CMD_EOP | TXD_CMD_RS;
		tranDescRing[a].css = 0;
		tranDescRing[a].special = 0;
	}
	//Initialize the receive descriptor ring
	for(a=0;a<NUM_RX_DESC;a++)
	{
		recvDescRing[a].addr = PADDR(&recvBuf[a]);
		recvDescRing[a].length = 0;
		recvDescRing[a].cs = 0;
		//Zero the status indicating it is available for hardware to use
		recvDescRing[a].status = 0;
		recvDescRing[a].errors = 0;
		recvDescRing[a].special = 0;
	}
}

int transmit_packet_e1000(void *addr, size_t len)
{
	uint32_t tail = pci_mmio_base[E1000_TDT/4];
	struct tx_desc *t = &tranDescRing[tail];

	//Check if the transmit descriptor ring is full
	if((t->status & TXD_STAT_DD) == 0)
	{
		//cprintf("transmit descriptor ring full...dropping packet...\n");
		return -1;
	}
	
	//Truncate the packet length if >TBUFFERSIZE
	if(len > TBUFFERSIZE)
		len = TBUFFERSIZE;

	//Copy the data to the packet header and update the descriptor length and status
	memmove(&tranBuf[tail], addr, len);
	t->length = (uint16_t)len;
	t->status = t->status & (!TXD_STAT_DD);

	//Update tail pointer
	tail = (tail+1)%NUM_TX_DESC;

	//Update TDT register	
	pci_mmio_base[E1000_TDT/4] = tail;
	return 0;
} 

int receive_packet_e1000(void *addr, size_t len)
{
	uint32_t tail = pci_mmio_base[E1000_RDT/4];
	struct rx_desc *r = &recvDescRing[tail];
	
	//Check if the receive descriptor ring is empty
	if((r->status & E1000_RXD_STAT_DD) == 0)
	{
		//cprintf("receive descriptor ring empty...dropping packet...\n");		
		return -1;
	}
	
	//Truncate the received packet if packet buffer length is lesser-sized
	if(r->length < len)
		len = r->length;

	//Copy the data from packet buffer to the address argument and update the descriptor status
	memmove(addr, &recvBuf[tail], len);
	r->status = r->status & (!E1000_RXD_STAT_DD);
	
	//Update tail pointer
	tail = (tail+1)%NUM_RX_DESC;

	//Update TDT register
	pci_mmio_base[E1000_RDT/4] = tail;
	return len;
}

int attach_e1000(struct pci_func *pcif)
{
	//uint32_t *devStatusReg,*TDBAH,*TDBAL,*TDLEN,*TDH,*TDT,*TCTL_EN,*TCTL_PSP,*TCTL_CT,*TCTL_COLD,*TIPG;
	//int i;
	
	pci_func_enable(pcif);
	descriptors_init();

	physaddr_t bar0_base = (physaddr_t)pcif->reg_base[0];
	size_t bar0_size = (size_t)pcif->reg_size[0];
	pci_mmio_base = mmio_map_region(bar0_base, bar0_size);

	//Device status register
	cprintf("e1000 device status register:%x\n", pci_mmio_base[E1000_STATUS/4]);
	
	//Initialize the card to transmit. Set the necessary registers
	pci_mmio_base[E1000_TDBAL/4] = PADDR(tranDescRing);
	pci_mmio_base[E1000_TDBAH/4] = 0;
	pci_mmio_base[E1000_TDLEN/4] = NUM_TX_DESC * sizeof(struct tx_desc);
	pci_mmio_base[E1000_TDH/4] = 0;
	pci_mmio_base[E1000_TDT/4] = 0;
	pci_mmio_base[E1000_TCTL/4] = E1000_TCTL_EN|E1000_TCTL_PSP|(E1000_TCTL_CT & (0x10<<4))|(E1000_TCTL_COLD & (0x40<<12));
	pci_mmio_base[E1000_TIPG/4] = 10|(8<<10)|(6<<20);
	
	//Test transmit_packet_e1000
	/*	
	char* testData1="Hello apcket";
	int res = transmit_packet_e1000(testData1, strlen(testData1));
	if(res != 0)
		panic("panic in attach_e1000:transmit_packet_e1000:%e",res);
	char *testData2="This is Balaji's packet";
	res = transmit_packet_e1000(testData2, strlen(testData2));
        if(res != 0)
                panic("panic in attach_e1000:transmit_packet_e1000:%e",res);
	*/

	//Initialize the card to receive. Set necessary registers
	//Set RAH and  RAL to filter out only packets destined for E1000's MACID
	pci_mmio_base[E1000_RA/4] = e1000_mac_address[0];
	pci_mmio_base[E1000_RA/4+1] = e1000_mac_address[1];
	pci_mmio_base[E1000_RA/4+1] |= E1000_RAH_AV;
	cprintf("e1000 mac_address:%x:%x\n",e1000_mac_address[1],e1000_mac_address[0]);
	
	pci_mmio_base[E1000_RDBAL/4] = PADDR(recvDescRing);
	pci_mmio_base[E1000_RDBAH/4] = 0;
	pci_mmio_base[E1000_RDLEN/4] = NUM_RX_DESC * sizeof(struct rx_desc);
	pci_mmio_base[E1000_RDH/4] = 0;
	pci_mmio_base[E1000_RDT/4] = 0;
	pci_mmio_base[E1000_RCTL/4] = E1000_RCTL_EN | E1000_RCTL_SZ_2048 | E1000_RCTL_SECRC;
	
	return 0;	
}
