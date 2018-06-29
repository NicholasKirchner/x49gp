/* $Id: flash.c,v 1.18 2008/12/11 12:18:17 ecd Exp $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include <x49gp.h>
#include <memory.h>
#include <byteorder.h>

#define FLASH_STATE_NORMAL		0

#define FLASH_STATE_UNLOCK1		1
#define FLASH_STATE_UNLOCK2		2

#define FLASH_STATE_ERASE1		3
#define FLASH_STATE_ERASE2		4
#define FLASH_STATE_ERASE3		5

#define FLASH_STATE_SOFTWARE_EXIT1	6
#define FLASH_STATE_SOFTWARE_EXIT2	7
#define FLASH_STATE_CFI_QUERY_EXIT1	8
#define FLASH_STATE_CFI_QUERY_EXIT2	9

#define FLASH_STATE_SOFTWARE_ID		10
#define FLASH_STATE_CFI_QUERY		11
#define FLASH_STATE_WORD_PROG		12

typedef struct {
	void			*data;
	int			state;
	unsigned short		vendor_ID;
	unsigned short		device_ID;
	const unsigned short	*cfi_data;
	uint32_t		cfi_size;
	uint32_t		sector_size;
	uint32_t		block_size;
	int			fd;
	size_t			size;

	uint32_t		iotype;
	uint32_t		offset;
} x49gp_flash_t;

#define SST29VF160_VENDOR_ID	0x00bf
#define SST29VF160_DEVICE_ID	0x2782

#define SST29VF160_SECTOR_SIZE	0x00001000
#define SST29VF160_BLOCK_SIZE	0x00010000
#define SST29VF160_SIZE		0x00200000

static const unsigned short sst29vf160_cfi_data[] =
{
	[0x10] = 0x0051,
	[0x11] = 0x0052,
	[0x12] = 0x0059,
	[0x13] = 0x0001,
	[0x14] = 0x0007,
	[0x15] = 0x0000,
	[0x16] = 0x0000,
	[0x17] = 0x0000,
	[0x18] = 0x0000,
	[0x19] = 0x0000,
	[0x1a] = 0x0000,

	[0x1b] = 0x0027,
	[0x1c] = 0x0036,
	[0x1d] = 0x0000,
	[0x1e] = 0x0000,
	[0x1f] = 0x0004,
	[0x20] = 0x0000,
	[0x21] = 0x0004,
	[0x22] = 0x0006,
	[0x23] = 0x0001,
	[0x24] = 0x0000,
	[0x25] = 0x0001,
	[0x26] = 0x0001,

	[0x27] = 0x0015,
	[0x28] = 0x0001,
	[0x29] = 0x0000,
	[0x2a] = 0x0000,
	[0x2b] = 0x0000,
	[0x2c] = 0x0002,
	[0x2d] = 0x00ff,
	[0x2e] = 0x0001,
	[0x2f] = 0x0010,
	[0x30] = 0x0000,
	[0x31] = 0x003f,
	[0x32] = 0x0000,
	[0x33] = 0x0000,
	[0x34] = 0x0001
};
#define SST29VF160_CFI_SIZE (sizeof(sst29vf160_cfi_data) / sizeof(sst29vf160_cfi_data[0]))

static void
flash_state_reset(x49gp_flash_t *flash)
{
	if (flash->state != FLASH_STATE_NORMAL) {
		cpu_register_physical_memory(0x00000000, SST29VF160_SIZE,
					     flash->offset | flash->iotype | IO_MEM_ROMD);
		flash->state = FLASH_STATE_NORMAL;
	}
}

static uint32_t
flash_get_halfword(x49gp_flash_t *flash, uint32_t offset)
{
	uint8_t *datap = flash->data;
	uint16_t data;

	switch (flash->state) {
	default:
		flash_state_reset(flash);
		/* fall through */

	case FLASH_STATE_NORMAL:
		data = lduw_p(datap + offset);
		break;

	case FLASH_STATE_SOFTWARE_ID:
		if (offset & 2) {
			data = flash->device_ID;
		} else {
			data = flash->vendor_ID;
		}
		break;

	case FLASH_STATE_CFI_QUERY:
		if (offset < flash->cfi_size) {
			data = flash->cfi_data[offset >> 1];
		} else {
			data = 0x0000;
		}
		break;
	}

	return data;
}

static void
flash_put_halfword(x49gp_flash_t *flash, uint32_t offset, uint32_t data)
{
	uint8_t *datap = flash->data;
	uint16_t temp;

	data &= 0xffff;

	switch (flash->state) {
	default:
		flash_state_reset(flash);
		/* fall through */

	case FLASH_STATE_NORMAL:
		if (((offset >> 1) == 0x5555) && ((data & 0xff) == 0xaa)) {
			flash->state = FLASH_STATE_UNLOCK1;
			cpu_register_physical_memory(0x00000000, SST29VF160_SIZE, flash->iotype);
		}
		break;

	case FLASH_STATE_UNLOCK1:
		if (((offset >> 1) == 0x2aaa) && ((data & 0xff) == 0x55)) {
			flash->state = FLASH_STATE_UNLOCK2;
		} else {
			flash_state_reset(flash);
		}
		break;

	case FLASH_STATE_UNLOCK2:
		if ((offset >> 1) == 0x5555) {
			switch (data & 0xff) {
			case 0xa0:
				flash->state = FLASH_STATE_WORD_PROG;
				break;
			case 0x80:
				flash->state = FLASH_STATE_ERASE1;
				break;
			case 0x90:
				flash->state = FLASH_STATE_SOFTWARE_ID;
				break;
			case 0x98:
				flash->state = FLASH_STATE_CFI_QUERY;
				break;
			default:
				flash_state_reset(flash);
				break;
			}
		} else {
			flash_state_reset(flash);
		}
		break;

	case FLASH_STATE_ERASE1:
		if (((offset >> 1) == 0x5555) && ((data & 0xff) == 0xaa)) {
			flash->state = FLASH_STATE_ERASE2;
		} else {
			flash_state_reset(flash);
		}
		break;

	case FLASH_STATE_ERASE2:
		if (((offset >> 1) == 0x2aaa) && ((data & 0xff) == 0x55)) {
			flash->state = FLASH_STATE_ERASE3;
		} else {
			flash_state_reset(flash);
		}
		break;

	case FLASH_STATE_SOFTWARE_EXIT1:
		if (((offset >> 1) == 0x2aaa) && ((data & 0xff) == 0x55)) {
			flash->state = FLASH_STATE_SOFTWARE_EXIT2;
		} else {
			flash->state = FLASH_STATE_SOFTWARE_ID;
		}
		break;

	case FLASH_STATE_SOFTWARE_EXIT2:
		if (((offset >> 1) == 0x5555) && ((data & 0xff) == 0xf0)) {
			flash_state_reset(flash);
		} else {
			flash->state = FLASH_STATE_SOFTWARE_ID;
		}
		break;

	case FLASH_STATE_CFI_QUERY_EXIT1:
		if (((offset >> 1) == 0x2aaa) && ((data & 0xff) == 0x55)) {
			flash->state = FLASH_STATE_CFI_QUERY_EXIT2;
		} else {
			flash->state = FLASH_STATE_CFI_QUERY;
		}
		break;

	case FLASH_STATE_CFI_QUERY_EXIT2:
		if (((offset >> 1) == 0x5555) && ((data & 0xff) == 0xf0)) {
			flash_state_reset(flash);
		} else {
			flash->state = FLASH_STATE_CFI_QUERY;
		}
		break;

	case FLASH_STATE_SOFTWARE_ID:
		if (((offset >> 1) == 0x5555) && ((data & 0xff) == 0xaa)) {
			flash->state = FLASH_STATE_SOFTWARE_EXIT1;
		} else if ((data & 0xff) == 0xf0) {
			flash_state_reset(flash);
		}
		break;

	case FLASH_STATE_CFI_QUERY:
		if (((offset >> 1) == 0x5555) && ((data & 0xff) == 0xaa)) {
			flash->state = FLASH_STATE_CFI_QUERY_EXIT1;
		} else if ((data & 0xff) == 0xf0) {
			flash_state_reset(flash);
		}
		break;

	case FLASH_STATE_WORD_PROG:
		temp = lduw_p(datap + offset);
		stw_p(datap + offset, data & temp);

#ifdef DEBUG_X49GP_FLASH_WRITE
		printf("write FLASH 2 (state %u) at offset %08x: %04x, result: %04x\n",
			flash->state, offset, data, lduw_p(datap + offset));
#endif

		flash_state_reset(flash);
		break;

	case FLASH_STATE_ERASE3:
		switch (data & 0xff) {
		case 0x10:	/* Chip Erase */
#ifdef DEBUG_X49GP_FLASH_WRITE
			printf("erase FLASH %08x %08x\n", 0, SST29VF160_SIZE);
#endif
			memset(datap, 0xff, SST29VF160_SIZE);
			break;

		case 0x30:	/* Sector Erase */
#ifdef DEBUG_X49GP_FLASH_WRITE
			printf("erase FLASH %08x %08x\n",
				(offset & ~(flash->sector_size - 1)),
				flash->sector_size);
#endif
			memset(datap + (offset & ~(flash->sector_size - 1)), 0xff, flash->sector_size);

#ifdef DEBUG_X49GP_FLASH_WRITE
			printf("erase FLASH %08x: %04x, %08x: %04x, %08x: %04x\n",
				offset, lduw_p(datap + offset),
				offset + 0x800, lduw_p(datap + offset + 0x800),
				offset + 0xffc, lduw_p(datap + offset + 0xffc));
#endif
			break;

		case 0x50:	/* Block Erase */
#ifdef DEBUG_X49GP_FLASH_WRITE
			printf("erase FLASH %08x %08x\n",
				(offset & ~(flash->block_size - 1)),
				flash->block_size);
#endif
			memset(datap + (offset & ~(flash->block_size - 1)), 0xff, flash->block_size);
			break;
		default:
			break;
		}

		flash_state_reset(flash);
		break;
	}
}

static uint32_t
flash_readb(void *opaque, target_phys_addr_t offset)
{
	x49gp_flash_t *flash = opaque;
	uint8_t *datap = flash->data;
	unsigned short temp;
	uint32_t shift;
	unsigned char data;

#ifdef QEMU_OLD
	offset -= (target_phys_addr_t) phys_ram_base + flash->offset;
#endif

	if (flash->state == FLASH_STATE_NORMAL) {
		data = *(datap + offset);
	} else {
		temp = flash_get_halfword(flash, offset & ~(1));
		shift = (offset & 1) << 3;
		data = (temp >> shift) & 0xff;
	}

#ifdef DEBUG_X49GP_FLASH_READ
	printf("read  FLASH 1 (state %u) at offset %08lx: %02x\n",
		flash->state, (unsigned long) offset, data);
#endif

	return data;
}

static uint32_t
flash_readw(void *opaque, target_phys_addr_t offset)
{
	x49gp_flash_t *flash = opaque;
	uint8_t *datap = flash->data;
	uint32_t data;

#ifdef QEMU_OLD
	offset -= (target_phys_addr_t) phys_ram_base + flash->offset;
#endif

	if (flash->state == FLASH_STATE_NORMAL) {
		data = lduw_p(datap + offset);
	} else {
		data = flash_get_halfword(flash, offset);
	}

#ifdef DEBUG_X49GP_FLASH_READ
	printf("read  FLASH 2 (state %u) at offset %08lx: %04x\n",
		flash->state, (unsigned long) offset, data);
#endif

	return data;
}

static uint32_t
flash_readl(void *opaque, target_phys_addr_t offset)
{
	x49gp_flash_t *flash = opaque;
	uint8_t *datap = flash->data;
	uint32_t data;

#ifdef QEMU_OLD
	offset -= (target_phys_addr_t) phys_ram_base + flash->offset;
#endif

	if (flash->state == FLASH_STATE_NORMAL) {
		data = ldl_p(datap + offset);
	} else {
		data = (flash_get_halfword(flash, offset + 2) << 16) |
		       (flash_get_halfword(flash, offset + 0) <<  0);
	}

#ifdef DEBUG_X49GP_FLASH_READ
	printf("read  FLASH 4 (state %u) at offset %08lx: %08x\n",
		flash->state, (unsigned long) offset, data);
#endif

	return data;
}

static void
flash_writeb(void *opaque, target_phys_addr_t offset, uint32_t data)
{
	x49gp_flash_t *flash = opaque;
	uint32_t shift;

#ifdef QEMU_OLD
	offset -= (target_phys_addr_t) phys_ram_base + flash->offset;
#endif

	data &= 0xff;

#ifdef DEBUG_X49GP_FLASH_WRITE
	printf("write FLASH 1 (state %u) at offset %08lx: %02x\n",
		flash->state, offset, data);
#endif

	/*
	 * This does not issue read-modify-write, i.e. you will get
	 * broken data in FLASH memory. This would be the case with
	 * real hardware, too.
	 */
	shift = (offset & 1) << 3;
	flash_put_halfword(flash, offset & ~(1), data << shift);
}

static void
flash_writew(void *opaque, target_phys_addr_t offset, uint32_t data)
{
	x49gp_flash_t *flash = opaque;

#ifdef QEMU_OLD
	offset -= (target_phys_addr_t) phys_ram_base + flash->offset;
#endif

	data &= 0xffff;

#ifdef DEBUG_X49GP_FLASH_WRITE
	printf("write FLASH 2 (state %u) at offset %08lx: %04x\n",
		flash->state, offset, data);
#endif

	flash_put_halfword(flash, offset, data);
}

static void
flash_writel(void *opaque, target_phys_addr_t offset, uint32_t data)
{
	x49gp_flash_t *flash = opaque;

#ifdef QEMU_OLD
	offset -= (target_phys_addr_t) phys_ram_base + flash->offset;
#endif

#ifdef DEBUG_X49GP_FLASH_WRITE
	printf("write FLASH 4 (state %u) at offset %08lx: %08x\n",
		flash->state, offset, data);
#endif

	flash_put_halfword(flash, offset + 2, (data >> 16) & 0xffff);
	flash_put_halfword(flash, offset + 0, (data >>  0) & 0xffff);
}

static int
flash_load(x49gp_module_t *module, GKeyFile *key)
{
	x49gp_flash_t *flash = module->user_data;
	char *filename;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	filename = x49gp_module_get_filename(module, key, "filename");
	if (NULL == filename) {
		fprintf(stderr, "%s: %s:%u: key \"filename\" not found\n",
			module->name, __FUNCTION__, __LINE__);
		return -1;
	}

	flash->fd = open(filename, O_RDWR);
	if (flash->fd < 0) {
		fprintf(stderr, "%s: %s:%u: open %s: %s\n",
			module->name, __FUNCTION__, __LINE__,
			filename, strerror(errno));
		g_free(filename);
		return -1;
	}

	flash->data = mmap(phys_ram_base + flash->offset, SST29VF160_SIZE,
			   PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
			   flash->fd, 0);
	if (flash->data == (void *) -1) {
		fprintf(stderr, "%s: %s:%u: mmap %s: %s\n",
			module->name, __FUNCTION__, __LINE__,
			filename, strerror(errno));
		g_free(filename);
		close(flash->fd);
		flash->fd = -1;
		return -1;
	}
	flash->size = SST29VF160_SIZE;

	g_free(filename);
	return 0;
}

static int
flash_save(x49gp_module_t *module, GKeyFile *config)
{
	x49gp_flash_t *flash = module->user_data;
	int error;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	error = msync(flash->data, flash->size, MS_ASYNC);
	if (error) {
		fprintf(stderr, "%s:%u: msync: %s\n",
			__FUNCTION__, __LINE__, strerror(errno));
		return error;
	}

	error = fsync(flash->fd);
	if (error) {
		fprintf(stderr, "%s:%u: fsync: %s\n",
			__FUNCTION__, __LINE__, strerror(errno));
		return error;
	}

	return 0;
}

static int
flash_reset(x49gp_module_t *module, x49gp_reset_t reset)
{
	x49gp_flash_t *flash = module->user_data;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	flash_state_reset(flash);
	return 0;
}

static CPUReadMemoryFunc *flash_readfn[] =
{
	flash_readb,
	flash_readw,
	flash_readl
};

static CPUWriteMemoryFunc *flash_writefn[] =
{
	flash_writeb,
	flash_writew,
	flash_writel
};

static int
flash_init(x49gp_module_t *module)
{
	x49gp_flash_t *flash;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	flash = malloc(sizeof(x49gp_flash_t));
	if (NULL == flash) {
		fprintf(stderr, "%s: %s:%u: Out of memory\n",
			module->name, __FUNCTION__, __LINE__);
		return -1;
	}
	memset(flash, 0, sizeof(x49gp_flash_t));

	flash->vendor_ID = SST29VF160_VENDOR_ID;
	flash->device_ID = SST29VF160_DEVICE_ID;
	flash->cfi_data = sst29vf160_cfi_data;
	flash->cfi_size = SST29VF160_CFI_SIZE;
	flash->sector_size = SST29VF160_SECTOR_SIZE;
	flash->block_size = SST29VF160_BLOCK_SIZE;
	flash->fd = -1;

	module->user_data = flash;

#ifdef QEMU_OLD
	flash->iotype = cpu_register_io_memory(0, flash_readfn,
					       flash_writefn, flash);
#else
	flash->iotype = cpu_register_io_memory(flash_readfn,
					       flash_writefn, flash);
#endif

	flash->data = (void *) -1;
	flash->offset = phys_ram_size;
	phys_ram_size += SST29VF160_SIZE;

	cpu_register_physical_memory(0x00000000, SST29VF160_SIZE,
				     flash->offset | flash->iotype | IO_MEM_ROMD);

	return 0;
}

static int
flash_exit(x49gp_module_t *module)
{
	x49gp_flash_t *flash;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	if (module->user_data) {
		flash = module->user_data;

		if (flash->data != (void *) -1) {
			munmap(flash->data, flash->size);
		}
		if (flash->fd) {
			close(flash->fd);
		}

		free(flash);
	}

	x49gp_module_unregister(module);
	free(module);

	return 0;
}

int
x49gp_flash_init(x49gp_t *x49gp)
{
	x49gp_module_t *module;

	if (x49gp_module_init(x49gp, "flash", flash_init, flash_exit,
			      flash_reset, flash_load, flash_save, NULL,
			      &module)) {
		return -1;
	}

	return x49gp_module_register(module);
}
