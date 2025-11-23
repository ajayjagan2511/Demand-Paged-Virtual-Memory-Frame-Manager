/*
	 File        : simple_disk.c

	 Author      : Riccardo Bettati
	 Modified    : 24/11/01

	 Description : Block-level READ/WRITE operations on a simple LBA28 disk
		       using Programmed I/O.

		       The disk must be MASTER or DEPENDENT on the PRIMARY IDE controller.

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

	/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "simple_disk.H"
#include "machine.H"

/*--------------------------------------------------------------------------*/
/* Class   S i m p l e   D i s k  */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

SimpleDisk::SimpleDisk(unsigned int _size) : size(_size)
{	
}

/*--------------------------------------------------------------------------*/
/* DISK CONFIGURATION */
/*--------------------------------------------------------------------------*/

unsigned int SimpleDisk::NaiveSize() {
	return size;
}

/*--------------------------------------------------------------------------*/
/* READ/WRITE FUNCTIONS */
/*--------------------------------------------------------------------------*/

void SimpleDisk::read(unsigned long _block_no, unsigned char* _buf) {
	/* Reads 512 Bytes in the given block of the given disk drive and copies them
	   to the given buffer. No error check! */

	ide_ata_issue_command(DISK_OPERATION::READ, _block_no);

	assert(ide_polling(true) == 0); // Polling

	unsigned short tmpw;
	for (int i = 0; i < 256; i++) {
		tmpw = Machine::inportw(0x1F0);
		_buf[i * 2] = (unsigned char)tmpw;
		_buf[i * 2 + 1] = (unsigned char)(tmpw >> 8);
	}
}

void SimpleDisk::write(unsigned long _block_no, unsigned char* _buf) {
	/* Writes 512 Bytes from the buffer to the given block on the given disk drive. */

	ide_ata_issue_command(DISK_OPERATION::WRITE, _block_no);

	assert(ide_polling(false) == 0); // Polling.

	unsigned short tmpw;
	for (int i = 0; i < 256; i++) {
		tmpw = _buf[2 * i] | (_buf[2 * i + 1] << 8);
		Machine::outportw(0x1F0, tmpw);
	}

	ide_write_register(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);

	assert(ide_polling(false) == 0); // Polling.
}

/*--------------------------------------------------------------------------*/
/* CODE TO DELAY READ/WRITES UNTIL DISK IS READY */
/*--------------------------------------------------------------------------*/

bool SimpleDisk::is_busy()
{
	return (get_status() & ATA_STATUS_BSY);
}

void SimpleDisk::wait_while_busy()
{
	while (is_busy()) {/* busy loop */; }
}

/*--------------------------------------------------------------------------*/
/* PRIVATE OPERATIONS (BETTER NOT TOUCH THESE!) */
/*--------------------------------------------------------------------------*/

unsigned char SimpleDisk::ide_read_register(unsigned char reg)
{
	unsigned char result;
	if (reg < 0x08)
		result = Machine::inportb(0x1F0 + reg - 0x00);
	else if (reg < 0x0C)
		result = Machine::inportb(0x1F0 + reg - 0x06);
	else if (reg < 0x0E)
		result = Machine::inportb(0x3F6 + reg - 0x0A);
	else if (reg < 0x16)
		result = Machine::inportb(0x00 + reg - 0x0E);
	//Console::puts("<R>"); Console::puti(result);
	return result;
}

void SimpleDisk::ide_write_register(unsigned char reg, unsigned char data)
{
	if (reg < 0x08)
		Machine::outportb(0x1F0 + reg - 0x00, data);
	else if (reg < 0x0C)
		Machine::outportb(0x1F0 + reg - 0x06, data);
	else if (reg < 0x0E)
		Machine::outportb(0x3F6 + reg - 0x0A, data);
	else if (reg < 0x16)
		Machine::outportb(0x00 + reg - 0x0E, data);
	//Console::puts("<W>");
}

unsigned char SimpleDisk::get_status()
{
	unsigned char status = Machine::inportb(0x1F7);
	//Console::puts(".");
	//Console::puti(status);
	return status;
}

unsigned char SimpleDisk::ide_polling(bool advanced_check)
{
	// (I) Delay 400 nanosecond for BSY to be set:
	// -------------------------------------------------
	for (int i = 0; i < 4; i++)
		ide_read_register(ATA_REG_ALTSTATUS); // Reading the Alternate Status port wastes 100ns; loop four times.

	// (II) Wait for BSY to be cleared:
	// -------------------------------------------------
	wait_while_busy();
	// Wait for BSY to be zero.

	if (advanced_check) {
		unsigned char state = get_status(); // Read Status Register.

		// (III) Check For Errors:
		// -------------------------------------------------
		if (state & ATA_STATUS_ERR)
			return 2; // Error.

		// (IV) Check If Device fault:
		// -------------------------------------------------
		if (state & ATA_STATUS_DF)
			return 1; // Device Fault.

		// (V) Check DRQ:
		// -------------------------------------------------
		// BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
		if ((state & ATA_STATUS_DRQ) == 0)
			return 3; // DRQ should be set
	}
	return 0; // No Error.
}

void SimpleDisk::ide_ata_issue_command(DISK_OPERATION _operation, unsigned int _block_no)
{
	// Wait if the drive is busy;

	wait_while_busy();
	// Wait for BSY to be zero.

	Machine::outportb(0x1F2, 0x01); /* send sector count to port 0X1F2 */
	Machine::outportb(0x1F3, (unsigned char)_block_no);
	Machine::outportb(0x1F4, (unsigned char)(_block_no >> 8));
	Machine::outportb(0x1F5, (unsigned char)(_block_no >> 16));
	Machine::outportb(0x1F6, ((unsigned char)(_block_no >> 24) & 0x0F) | 0xE0 | (0 << 4));

	// Select the command and send it;

	Machine::outportb(0x1F7, (_operation == DISK_OPERATION::READ) ? 0x20 : 0x30); 
	// READ with retry (0x20) or WRITE with retry (0x30)
}

