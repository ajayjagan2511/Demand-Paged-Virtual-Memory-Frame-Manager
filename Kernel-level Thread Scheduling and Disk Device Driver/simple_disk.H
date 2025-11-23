/*
	 File        : simple_disk.H

	 Author      : Riccardo Bettati
	 Modified    : 24/11/22

	 Description : Block-level READ/WRITE operations on a simple LBA28 disk
				   using Programmed I/O.

				   This IDE Controller only supports one disk, which is the
				   MASTER on the PRIMARY IDE channel.
*/

#ifndef _SIMPLE_DISK_H_
#define _SIMPLE_DISK_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* I D E   C o n t r o l l e r  */
/*--------------------------------------------------------------------------*/

class SimpleDisk {

private:

	// OPERATIONS

	enum class DISK_OPERATION { READ = 0, WRITE = 1 };

private:
	unsigned int size = 0; // Size of the disk, in bytes.

public:

	static const unsigned int BLOCK_SIZE = 512;

	/*--------------------------------------------------------------------------*/
	/* CONSTRUCTOR */
	/*--------------------------------------------------------------------------*/

	SimpleDisk(unsigned int _size);
	/* Creates a SimpleDisk device with the given size connected to the 
	   MASTER slot of the primary ATA controller.
	   NOTE: We are passing the _size argument out of laziness. In a real system, 
	   we would infer this information from the disk controller.
	*/

	/*--------------------------------------------------------------------------*/
	/* DISK CONFIGURATION */
	/*--------------------------------------------------------------------------*/

	virtual unsigned int NaiveSize();
	/* Returns the size of the disk, in Byte. */

	/*--------------------------------------------------------------------------*/
	/* READ/WRITE FUNCTIONS */
	/*--------------------------------------------------------------------------*/

	virtual void read(unsigned long _block_no, unsigned char* _buf);
	/* Reads 512 Bytes from the given block of the disk and copies them
	   to the given buffer. No error check! 
	*/

	virtual void write(unsigned long _block_no, unsigned char* _buf);
	/* Writes 512 Bytes from the buffer to the given block on the disk. */

protected:

	/*--------------------------------------------------------------------------*/
	/* CODE TO DELAY READ/WRITES UNTIL DISK IS READY */
	/*--------------------------------------------------------------------------*/

	virtual bool is_busy();
	/* Return if the disk is busy. If not busy, the disk is ready to transfer data 
	   from/to disk. 
	   Avoid overloading this function if you can. */

	virtual void wait_while_busy();
	/* Is called during each read/write operation to check whether the disk is busy.
	   If the disk is not busy, it is ready to star transfering the data from/to disk.
	   In SimpleDisk, this function simply loops while is_busy() return true.
	   In more sophisticated disk implementations, the thread may give up the CPU
	   and return to check later. */

private:

	/*--------------------------------------------------------------------------*/
	/* INTERNAL STUFF TO ACCESS/CONTROL IDE DISK CONTROLLER USING ATA PROTOCOL. */
	/*--------------------------------------------------------------------------*/

	// COMMANDS

	static constexpr unsigned char  ATA_CMD_READ_PIO = 0x20;
	static constexpr unsigned char  ATA_CMD_READ_PIO_EXT = 0x24;
	static constexpr unsigned char  ATA_CMD_READ_DMA = 0xC8;
	static constexpr unsigned char  ATA_CMD_READ_DMA_EXT = 0x25;
	static constexpr unsigned char  ATA_CMD_WRITE_PIO = 0x30;
	static constexpr unsigned char  ATA_CMD_WRITE_PIO_EXT = 0x34;
	static constexpr unsigned char  ATA_CMD_WRITE_DMA = 0xCA;
	static constexpr unsigned char  ATA_CMD_WRITE_DMA_EXT = 0x35;
	static constexpr unsigned char  ATA_CMD_CACHE_FLUSH = 0xE7;
	static constexpr unsigned char  ATA_CMD_CACHE_FLUSH_EXT = 0xEA;
	static constexpr unsigned char  ATA_CMD_PACKET = 0xA0;
	static constexpr unsigned char  ATA_CMD_IDENTIFY_PACKET = 0xA1;
	static constexpr unsigned char  ATA_CMD_IDENTIFY = 0xEC;

	// REGISTERS

	static constexpr unsigned char ATA_REG_DATA = 0x00;
	static constexpr unsigned char ATA_REG_ERROR = 0x01;
	static constexpr unsigned char ATA_REG_FEATURES = 0x01;
	static constexpr unsigned char ATA_REG_SECCOUNT0 = 0x02;
	static constexpr unsigned char ATA_REG_LBA0 = 0x03;
	static constexpr unsigned char ATA_REG_LBA1 = 0x04;
	static constexpr unsigned char ATA_REG_LBA2 = 0x05;
	static constexpr unsigned char ATA_REG_HDDEVSEL = 0x06;
	static constexpr unsigned char ATA_REG_COMMAND = 0x07;
	static constexpr unsigned char ATA_REG_STATUS = 0x07;
	static constexpr unsigned char ATA_REG_SECCOUNT1 = 0x08;
	static constexpr unsigned char ATA_REG_LBA3 = 0x09;
	static constexpr unsigned char ATA_REG_LBA4 = 0x0A;
	static constexpr unsigned char ATA_REG_LBA5 = 0x0B;
	static constexpr unsigned char ATA_REG_CONTROL = 0x0C;
	static constexpr unsigned char ATA_REG_ALTSTATUS = 0x0C;
	static constexpr unsigned char ATA_REG_DEVADDRESS = 0x0D;

	// STATUS
	static constexpr unsigned char ATA_STATUS_BSY = 0x80;    // Busy
	static constexpr unsigned char ATA_STATUS_DRDY = 0x40;    // Drive ready
	static constexpr unsigned char ATA_STATUS_DF = 0x20;    // Drive write fault
	static constexpr unsigned char ATA_STATUS_DSC = 0x10;    // Drive seek complete
	static constexpr unsigned char ATA_STATUS_DRQ = 0x08;    // Data request ready
	static constexpr unsigned char ATA_STATUS_CORR = 0x04;    // Corrected data
	static constexpr unsigned char ATA_STATUS_IDX = 0x02;    // Index
	static constexpr unsigned char ATA_STATUS_ERR = 0x01;    // Error

	// MANIPULATE DISK CONTROLLER REGISTERS

	unsigned char ide_read_register(unsigned char reg);

	void ide_write_register(unsigned char reg, unsigned char data);

	// CHECK STATUS OF DISK CONTROLLER

	unsigned char get_status();

	// SETUP POLLING OF DISK CONTROLLER

	unsigned char ide_polling(bool advanced_check);

	// ISSUE COMMAND TO DISK CONTROLLER

	void ide_ata_issue_command(DISK_OPERATION operation, unsigned int block_no);

};

#endif
