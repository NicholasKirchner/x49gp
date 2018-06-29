/* $Id: s3c2410_sdi.c,v 1.4 2008/12/11 12:18:17 ecd Exp $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#include <x49gp.h>
#include <s3c2410.h>

#include <block.h>

typedef struct {
	uint32_t			sdicon;
	uint32_t			sdipre;
	uint32_t			sdicarg;
	uint32_t			sdiccon;
	uint32_t			sdicsta;
	uint32_t			sdirsp0;
	uint32_t			sdirsp1;
	uint32_t			sdirsp2;
	uint32_t			sdirsp3;
	uint32_t			sdidtimer;
	uint32_t			sdibsize;
	uint32_t			sdidcon;
	uint32_t			sdidcnt;
	uint32_t			sdidsta;
	uint32_t			sdifsta;
	uint32_t			sdidat;
	uint32_t			sdiimsk;

	unsigned int		nr_regs;
	s3c2410_offset_t	*regs;

	x49gp_t			*x49gp;

	BlockDriverState	*bs;
	int			fd;

	unsigned char		*read_data;
	uint32_t		nr_read_data;
	uint32_t		read_offset;
	unsigned int		read_index;

	unsigned char		*write_data;
	uint32_t		nr_write_data;
	uint32_t		write_offset;
	unsigned int		write_index;

	int			multiple_block;
} s3c2410_sdi_t;


static int
s3c2410_sdi_data_init(s3c2410_sdi_t *sdi)
{
	s3c2410_offset_t regs[] = {
		S3C2410_OFFSET(SDI, SDICON, 0x00000000, sdi->sdicon),
		S3C2410_OFFSET(SDI, SDIPRE, 0x00000000, sdi->sdipre),
		S3C2410_OFFSET(SDI, SDICARG, 0x00000000, sdi->sdicarg),
		S3C2410_OFFSET(SDI, SDICCON, 0x00000000, sdi->sdiccon),
		S3C2410_OFFSET(SDI, SDICSTA, 0x00000000, sdi->sdicsta),
		S3C2410_OFFSET(SDI, SDIRSP0, 0x00000000, sdi->sdirsp0),
		S3C2410_OFFSET(SDI, SDIRSP1, 0x00000000, sdi->sdirsp1),
		S3C2410_OFFSET(SDI, SDIRSP2, 0x00000000, sdi->sdirsp2),
		S3C2410_OFFSET(SDI, SDIRSP3, 0x00000000, sdi->sdirsp3),
		S3C2410_OFFSET(SDI, SDIDTIMER, 0x00002000, sdi->sdidtimer),
		S3C2410_OFFSET(SDI, SDIBSIZE, 0x00000000, sdi->sdibsize),
		S3C2410_OFFSET(SDI, SDIDCON, 0x00000000, sdi->sdidcon),
		S3C2410_OFFSET(SDI, SDIDCNT, 0x00000000, sdi->sdidcnt),
		S3C2410_OFFSET(SDI, SDIDSTA, 0x00000000, sdi->sdidsta),
		S3C2410_OFFSET(SDI, SDIFSTA, 0x00000000, sdi->sdifsta),
		S3C2410_OFFSET(SDI, SDIDAT, 0x00000000, sdi->sdidat),
		S3C2410_OFFSET(SDI, SDIIMSK, 0x00000000, sdi->sdiimsk)
	};

	memset(sdi, 0, sizeof(s3c2410_sdi_t));

	sdi->regs = malloc(sizeof(regs));
	if (NULL == sdi->regs) {
		fprintf(stderr, "%s:%u: Out of memory\n",
			__FUNCTION__, __LINE__);
		return -ENOMEM;
	}

	memcpy(sdi->regs, regs, sizeof(regs));
	sdi->nr_regs = sizeof(regs) / sizeof(regs[0]);

	return 0;
}

static int
sdcard_read(s3c2410_sdi_t *sdi)
{
	uint32_t offset;
	uint32_t size;
	int error;

	offset = sdi->read_offset;
	size = sdi->nr_read_data;

#ifdef DEBUG_S3C2410_SDI
	printf("SDI: card read  %4u at %08x\n", size, offset);
#endif

	sdi->nr_read_data = 0;
	sdi->read_index = 0;

	if ((sdi->fd < 0) && (sdi->bs == NULL)) {
		return -ENODEV;
	}

	sdi->read_data = malloc(size);
	if (NULL == sdi->read_data) {
		return -ENOMEM;
	}

	if (sdi->bs) {
		sdi->nr_read_data = bdrv_pread(sdi->bs, offset, sdi->read_data, size);
	} else {
		if (sdi->read_offset != lseek(sdi->fd, offset, SEEK_SET)) {
			error = errno;
			sdi->nr_read_data = 0;
			free(sdi->read_data);
			sdi->read_data = NULL;
			return -error;
		}

		sdi->nr_read_data = read(sdi->fd, sdi->read_data, size);
	}

	if (sdi->nr_read_data != size) {
		error = errno;
		sdi->nr_read_data = 0;
		free(sdi->read_data);
		sdi->read_data = NULL;
		return -error;
	}

	return 0;
}

static int
sdcard_write_prepare(s3c2410_sdi_t *sdi)
{
	sdi->write_index = 0;

	if ((sdi->fd < 0) && (sdi->bs == NULL)) {
		sdi->nr_write_data = 0;
		return -ENODEV;
	}

	sdi->write_data = malloc(sdi->nr_write_data);
	if (NULL == sdi->write_data) {
		sdi->nr_write_data = 0;
		return -ENOMEM;
	}

	return 0;
}

static int
sdcard_write(s3c2410_sdi_t *sdi)
{
	uint32_t offset;
	uint32_t size;
	int error;

	offset = sdi->write_offset;
	size = sdi->nr_write_data;

#ifdef DEBUG_S3C2410_SDI
	printf("SDI: card write %4u at %08x\n", size, offset);
#endif

	sdi->write_index = 0;

	if (sdi->bs) {
		error = bdrv_pwrite(sdi->bs, offset, sdi->write_data, size);
	} else {
		if (sdi->fd < 0) {
			free(sdi->write_data);
			sdi->write_data = NULL;
			return -ENODEV;
		}

		if (sdi->write_offset != lseek(sdi->fd, offset, SEEK_SET)) {
			error = errno;
			free(sdi->write_data);
			sdi->write_data = NULL;
			return -error;
		}

		error = write(sdi->fd, sdi->write_data, size);
	}

	if (error != size) {
		error = errno;
		free(sdi->write_data);
		sdi->write_data = NULL;
		return -error;
	}

	free(sdi->write_data);
	sdi->write_data = NULL;
	return 0;
}

static uint32_t
s3c2410_sdi_read(void *opaque, target_phys_addr_t offset)
{
	s3c2410_sdi_t *sdi = opaque;
	s3c2410_offset_t *reg;
	unsigned int read_avail, write_avail;

#ifdef QEMU_OLD
	offset -= S3C2410_SDI_BASE;
#endif
	if (! S3C2410_OFFSET_OK(sdi, offset)) {
		return ~(0);
	}

	reg = S3C2410_OFFSET_ENTRY(sdi, offset);

	switch (offset) {
	case S3C2410_SDI_SDIDAT:
		read_avail = sdi->nr_read_data - sdi->read_index;
		if (read_avail > 0) {
			*(reg->datap) = sdi->read_data[sdi->read_index++] << 24;
			if ((sdi->nr_read_data - sdi->read_index) > 0) {
				*(reg->datap) |= sdi->read_data[sdi->read_index++] << 16;
			}
			if ((sdi->nr_read_data - sdi->read_index) > 0) {
				*(reg->datap) |= sdi->read_data[sdi->read_index++] << 8;
			}
			if ((sdi->nr_read_data - sdi->read_index) > 0) {
				*(reg->datap) |= sdi->read_data[sdi->read_index++] << 0;
			}

			if (sdi->read_index >= sdi->nr_read_data) {
				sdi->read_index = 0;
				free(sdi->read_data);
				sdi->read_data = NULL;

				if (sdi->multiple_block) {
					sdi->read_offset += sdi->nr_read_data;

					sdcard_read(sdi);

					sdi->sdidsta |= (1 << 9) | (1 << 4);
				} else {
					sdi->nr_read_data = 0;
				}
			}

			read_avail = sdi->nr_read_data - sdi->read_index;

			sdi->sdidcnt = read_avail;
		}
		break;

	case S3C2410_SDI_SDIFSTA:
		*(reg->datap) = (1 << 13) | (1 << 10);

		read_avail = sdi->nr_read_data - sdi->read_index;
		if (read_avail > 63)
			*(reg->datap) |= (1 << 12) | (1 << 8) | (1 << 7) | 0x40;
		else if (read_avail > 31)
			*(reg->datap) |= (1 << 12) | (1 << 9) | (1 << 7) | read_avail;
		else 
			*(reg->datap) |= (1 << 12) | (1 << 9) | read_avail;
		break;

	case S3C2410_SDI_SDIDSTA:
		*(reg->datap) &= ~(0x0f);

		read_avail = sdi->nr_read_data - sdi->read_index;
		if (read_avail) {
			*(reg->datap) |= (1 << 0);
		}

		write_avail = sdi->nr_write_data - sdi->write_index;
		if (write_avail) {
			*(reg->datap) |= (1 << 1);
		}
	}

#ifdef DEBUG_S3C2410_SDI
	printf("read  %s [%08x] %s [%08lx] data %08x\n",
		"s3c2410-sdi", S3C2410_SDI_BASE,
		reg->name, (unsigned long) offset, *(reg->datap));
#endif

	return *(reg->datap);
}

static void
s3c2410_sdi_write(void *opaque, target_phys_addr_t offset, uint32_t data)
{
	s3c2410_sdi_t *sdi = opaque;
	s3c2410_offset_t *reg;
	unsigned int read_avail, write_avail;

#ifdef QEMU_OLD
	offset -= S3C2410_SDI_BASE;
#endif
	if (! S3C2410_OFFSET_OK(sdi, offset)) {
		return;
	}

	reg = S3C2410_OFFSET_ENTRY(sdi, offset);

#ifdef DEBUG_S3C2410_SDI
	printf("write %s [%08x] %s [%08lx] data %08x\n",
		"s3c2410-sdi", S3C2410_SDI_BASE,
		reg->name, (unsigned long) offset, data);
#endif

	switch (offset) {
	case S3C2410_SDI_SDICON:
		*(reg->datap) = data & ~(2);
		break;

	case S3C2410_SDI_SDICCON:
		*(reg->datap) = data;

#ifdef DEBUG_S3C2410_SDI
		printf("SDI: cmd %02u, start %u, %s response %u, data %u, abort %u\n",
			data & 0x3f, (data >> 8) & 1,
			(data >> 10) & 1 ? "long" : "short",
			(data >> 9) & 1,
			(data >> 11) & 1, (data >> 12) & 1);
#endif

		if (data & (1 << 8)) {
			sdi->sdicsta |= (1 << 11);
		} else {
			sdi->sdicsta = 0;
			sdi->sdirsp0 = 0;
			sdi->sdirsp1 = 0;
			sdi->sdirsp2 = 0;
			sdi->sdirsp3 = 0;
			data = 0;
		}

		sdi->sdicsta &= ~(1 << 12);
		sdi->sdicsta &= ~(0xff);

		if (data & (1 << 9)) {
			sdi->sdicsta |= (1 << 9);

			switch (data & 0x3f) {
			case 0:
				break;

			case 1:
				sdi->sdicsta |= 0x3f;
				sdi->sdirsp0 = 0x80ff8000;
				sdi->sdirsp1 = 0xff000000;
				break;

			case 2:
				sdi->sdicsta |= 0x3f;
				sdi->sdirsp0 = 0x02000053;
				sdi->sdirsp1 = 0x444d3036;
				sdi->sdirsp2 = 0x34011234;
				sdi->sdirsp3 = 0x567813ff;
				break;

			case 3:
				sdi->sdicsta |= 3;
				sdi->sdirsp0 = 0x00000700;
				sdi->sdirsp1 = 0xff000000;
				break;

			case 7:
				sdi->sdicsta |= 7;
				sdi->sdirsp0 = 0x00000900;
				sdi->sdirsp1 = 0xff000000;
				break;

			case 9:
			{
				/*                          c_size_mult + 2
				 * blocks = (c_size + 1) * 2
				 *
				 * size = bl_len * blocks;
				 */
				uint32_t write_bl_len = 9;
				uint32_t read_bl_len = 9;
				uint32_t c_size_mult = 7;
				uint32_t c_size = 256 - 1;

				sdi->sdicsta |= 9;
				/* 127 .. 96 */
				sdi->sdirsp0 = 0x8c0f002a;
				/*  95 .. 64 */
				sdi->sdirsp1 = 0x0f508000 | ((c_size & 0xffc) >>  2) | ((read_bl_len & 0xf) << 16);
				/*  63 .. 32 */
				sdi->sdirsp2 = 0x2dd47c1f | ((c_size & 0x003) << 30) | ((c_size_mult & 7) << 15);
				/*  31 ..  0 */
				sdi->sdirsp3 = 0x8a0040ff | ((write_bl_len & 0xf) << 22);
				break;
			}

			case 10:
				sdi->sdicsta |= 10;
				sdi->sdirsp0 = 0x02000053;
				sdi->sdirsp1 = 0x444d3036;
				sdi->sdirsp2 = 0x34011234;
				sdi->sdirsp3 = 0x567813ff;
				break;

			case 12:
				sdi->multiple_block = 0;

				sdi->sdicsta |= 12;
				sdi->sdirsp0 = 0x00000900;
				sdi->sdirsp1 = 0xff000000;
				break;

			case 18:
				sdi->multiple_block = 1;
				/* fall through */
			case 17:
				sdi->read_offset = sdi->sdicarg;
				sdi->nr_read_data = sdi->sdibsize & 0xfff;

				sdcard_read(sdi);

				read_avail = sdi->nr_read_data - sdi->read_index;

				sdi->sdicsta |= data & 0x3f;
				sdi->sdirsp0 = 0x00000b00;
				sdi->sdirsp1 = 0xff000000;
				sdi->sdidcnt = (1 << 12) | read_avail;
				sdi->sdidsta |= (1 << 9) | (1 << 4);
				break;

			case 25:
				sdi->multiple_block = 1;
				/* fall through */
			case 24:
				sdi->write_offset = sdi->sdicarg;
				sdi->nr_write_data = sdi->sdibsize & 0xfff;

				sdcard_write_prepare(sdi);

				write_avail = sdi->nr_write_data - sdi->write_index;

				sdi->sdicsta |= data & 0x3f;
				sdi->sdirsp0 = 0x00000d00;
				sdi->sdirsp1 = 0xff000000;
				sdi->sdidcnt = (1 << 12) | write_avail;
				break;

			default:
				printf("unhandled SDcard CMD %u\n", (data & 0x3f));

				sdi->sdicsta |= (1 << 10);
				sdi->sdicsta &= ~(1 << 9);

//				abort();
				break;
			}
		}

		break;

	case S3C2410_SDI_SDIDAT:
		*(reg->datap) = data;

		write_avail = sdi->nr_write_data - sdi->write_index;
		if (write_avail > 0) {
			sdi->write_data[sdi->write_index++] = (*(reg->datap) >> 24) & 0xff;
			if ((sdi->nr_write_data - sdi->write_index) > 0) {
				sdi->write_data[sdi->write_index++] = (*(reg->datap) >> 16) & 0xff;
			}
			if ((sdi->nr_write_data - sdi->write_index) > 0) {
				sdi->write_data[sdi->write_index++] = (*(reg->datap) >> 8) & 0xff;
			}
			if ((sdi->nr_write_data - sdi->write_index) > 0) {
				sdi->write_data[sdi->write_index++] = (*(reg->datap) >> 0) & 0xff;
			}

			if (sdi->write_index >= sdi->nr_write_data) {
				sdcard_write(sdi);

				if (sdi->multiple_block) {
					sdi->write_offset += sdi->nr_write_data;
					sdcard_write_prepare(sdi);
				} else {
					sdi->nr_write_data = 0;
				}

				sdi->sdidsta |= (1 << 9) | (1 << 4);
			}

			write_avail = sdi->nr_write_data - sdi->write_index;

			sdi->sdidcnt = write_avail;
		}
		break;

	case S3C2410_SDI_SDICSTA:
		*(reg->datap) &= ~(data & 0xf00);
		break;

	case S3C2410_SDI_SDIDSTA:
		*(reg->datap) &= ~(data & 0x3f8);
		break;

	case S3C2410_SDI_SDIFSTA:
		/* ignore */
		break;

	default:
		*(reg->datap) = data;
		break;
	}
}

static int
s3c2410_sdi_load(x49gp_module_t *module, GKeyFile *key)
{
	s3c2410_sdi_t *sdi = module->user_data;
	s3c2410_offset_t *reg;
	char *filename;
	char vvfat_name[1024];
	struct stat st;
	int error = 0;
	int i;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	sdi->fd = -1;
	sdi->bs = NULL;

	filename = x49gp_module_get_filename(module, key, "filename");
	if (filename && (stat(filename, &st) == 0)) {
		if (S_ISDIR(st.st_mode)) {
			sprintf(vvfat_name, "fat:rw:16:%s", filename);
			sdi->bs = bdrv_new("");
			if (sdi->bs) {
				error = bdrv_open(sdi->bs, vvfat_name, 0);
				fprintf(stderr, "%s:%u: bdrv_open(%s): %d\n",
					__FUNCTION__, __LINE__, vvfat_name, error);
				if (error != 0) {
					bdrv_delete(sdi->bs);
					sdi->bs = NULL;
				}
			}
		} else {
			sdi->fd = open(filename, O_RDWR);
		}
		g_free(filename);
	}

	if ((sdi->bs != NULL) || (sdi->fd >= 0)) {
		s3c2410_io_port_f_set_bit(sdi->x49gp, 3, 1);
	} else {
		s3c2410_io_port_f_set_bit(sdi->x49gp, 3, 0);
	}

	for (i = 0; i < sdi->nr_regs; i++) {
		reg = &sdi->regs[i];

		if (NULL == reg->name)
			continue;

		if (x49gp_module_get_u32(module, key, reg->name,
					 reg->reset, reg->datap))
			error = -EAGAIN;
	}

	return error;
}

static int
s3c2410_sdi_save(x49gp_module_t *module, GKeyFile *key)
{
	s3c2410_sdi_t *sdi = module->user_data;
	s3c2410_offset_t *reg;
	int i;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	for (i = 0; i < sdi->nr_regs; i++) {
		reg = &sdi->regs[i];

		if (NULL == reg->name)
			continue;

		x49gp_module_set_u32(module, key, reg->name, *(reg->datap));
	}

	return 0;
}

static int
s3c2410_sdi_reset(x49gp_module_t *module, x49gp_reset_t reset)
{
	s3c2410_sdi_t *sdi = module->user_data;
	s3c2410_offset_t *reg;
	int i;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	for (i = 0; i < sdi->nr_regs; i++) {
		reg = &sdi->regs[i];

		if (NULL == reg->name)
			continue;

		*(reg->datap) = reg->reset;
	}

	return 0;
}

static CPUReadMemoryFunc *s3c2410_sdi_readfn[] =
{
	s3c2410_sdi_read,
	s3c2410_sdi_read,
	s3c2410_sdi_read
};

static CPUWriteMemoryFunc *s3c2410_sdi_writefn[] =
{
	s3c2410_sdi_write,
	s3c2410_sdi_write,
	s3c2410_sdi_write
};

static int
s3c2410_sdi_init(x49gp_module_t *module)
{
	s3c2410_sdi_t *sdi;
	int iotype;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	sdi = malloc(sizeof(s3c2410_sdi_t));
	if (NULL == sdi) {
		fprintf(stderr, "%s: %s:%u: Out of memory\n",
			module->x49gp->progname, __FUNCTION__, __LINE__);
		return -ENOMEM;
	}
	if (s3c2410_sdi_data_init(sdi)) {
		free(sdi);
		return -ENOMEM;
	}

	module->user_data = sdi;
	sdi->x49gp = module->x49gp;

#ifdef QEMU_OLD
	iotype = cpu_register_io_memory(0, s3c2410_sdi_readfn,
					s3c2410_sdi_writefn, sdi);
#else
	iotype = cpu_register_io_memory(s3c2410_sdi_readfn,
					s3c2410_sdi_writefn, sdi);
#endif
printf("%s: iotype %08x\n", __FUNCTION__, iotype);
	cpu_register_physical_memory(S3C2410_SDI_BASE, S3C2410_MAP_SIZE, iotype);

	bdrv_init();
	return 0;
}

static int
s3c2410_sdi_exit(x49gp_module_t *module)
{
	s3c2410_sdi_t *sdi;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	if (module->user_data) {
		sdi = module->user_data;
		if (sdi->regs)
			free(sdi->regs);
		free(sdi);
	}

	x49gp_module_unregister(module);
	free(module);

	return 0;
}

int
x49gp_s3c2410_sdi_init(x49gp_t *x49gp)
{
	x49gp_module_t *module;

	if (x49gp_module_init(x49gp, "s3c2410-sdi",
			      s3c2410_sdi_init,
			      s3c2410_sdi_exit,
			      s3c2410_sdi_reset,
			      s3c2410_sdi_load,
			      s3c2410_sdi_save,
			      NULL, &module)) {
		return -1;
	}

	return x49gp_module_register(module);
}
