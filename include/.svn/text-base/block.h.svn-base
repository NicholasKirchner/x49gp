/* $Id: block.h,v 1.1 2008/12/11 12:18:17 ecd Exp $
 */

#ifndef BLOCK_H
#define BLOCK_H 1

#include <stdint.h>

#define BDRV_O_RDONLY      0x0000
#define BDRV_O_RDWR        0x0002
#define BDRV_O_ACCESS      0x0003
#define BDRV_O_CREAT       0x0004 /* create an empty file */
#define BDRV_O_SNAPSHOT    0x0008 /* open the file read only and save
				     writes in a snapshot */
#define BDRV_O_FILE        0x0010 /* open as a raw file (do not try to
                                     use a disk image format on top of
				     it (default for
				     bdrv_file_open()) */

#ifdef QEMU_OLD
typedef struct BlockDriverState BlockDriverState;
#endif
typedef struct BlockDriver BlockDriver;
typedef struct SnapshotInfo QEMUSnapshotInfo;
typedef struct BlockDriverInfo BlockDriverInfo;
typedef struct BlockDriverAIOCB BlockDriverAIOCB;
typedef void BlockDriverCompletionFunc(void *opaque, int ret);

extern BlockDriver bdrv_raw;
extern BlockDriver bdrv_host_device;
extern BlockDriver bdrv_qcow;
extern BlockDriver bdrv_vvfat;

void bdrv_init(void);
int bdrv_create(BlockDriver *drv,
                const char *filename, int64_t size_in_sectors,
		const char *backing_file, int flags);
BlockDriverState *bdrv_new(const char *device_name);
void bdrv_delete(BlockDriverState *bs);
int bdrv_file_open(BlockDriverState **pbs, const char *filename, int flags);
int bdrv_open(BlockDriverState *bs, const char *filename, int flags);

int bdrv_read(BlockDriverState *bs, int64_t sector_num,
              uint8_t *buf, int nb_sectors); 
int bdrv_pread(BlockDriverState *bs, int64_t offset,
	       void *buf, int count);
int bdrv_pwrite(BlockDriverState *bs, int64_t offset,
		const void *buf, int count);

#ifdef QEMU_OLD
struct BlockDriver {
    const char *format_name;
    int instance_size;
    int (*bdrv_probe)(const uint8_t *buf, int buf_size, const char *filename);
    int (*bdrv_open)(BlockDriverState *bs, const char *filename, int flags);
    int (*bdrv_read)(BlockDriverState *bs, int64_t sector_num, 
                     uint8_t *buf, int nb_sectors);
    int (*bdrv_write)(BlockDriverState *bs, int64_t sector_num, 
                      const uint8_t *buf, int nb_sectors);
    void (*bdrv_close)(BlockDriverState *bs);
    int (*bdrv_create)(const char *filename, int64_t total_sectors, 
                       const char *backing_file, int flags);
    void (*bdrv_flush)(BlockDriverState *bs);
    int (*bdrv_is_allocated)(BlockDriverState *bs, int64_t sector_num,
                             int nb_sectors, int *pnum);
    int (*bdrv_set_key)(BlockDriverState *bs, const char *key);
    int (*bdrv_make_empty)(BlockDriverState *bs);
    /* aio */
    BlockDriverAIOCB *(*bdrv_aio_read)(BlockDriverState *bs,
        int64_t sector_num, uint8_t *buf, int nb_sectors,
        BlockDriverCompletionFunc *cb, void *opaque);
    BlockDriverAIOCB *(*bdrv_aio_write)(BlockDriverState *bs,
        int64_t sector_num, const uint8_t *buf, int nb_sectors,
        BlockDriverCompletionFunc *cb, void *opaque);
    void (*bdrv_aio_cancel)(BlockDriverAIOCB *acb);
    int aiocb_size;

    const char *protocol_name;
    int (*bdrv_pread)(BlockDriverState *bs, int64_t offset, 
                      uint8_t *buf, int count);
    int (*bdrv_pwrite)(BlockDriverState *bs, int64_t offset, 
                       const uint8_t *buf, int count);
    int (*bdrv_truncate)(BlockDriverState *bs, int64_t offset);
    int64_t (*bdrv_getlength)(BlockDriverState *bs);
    int (*bdrv_write_compressed)(BlockDriverState *bs, int64_t sector_num, 
                                 const uint8_t *buf, int nb_sectors);

    int (*bdrv_snapshot_create)(BlockDriverState *bs, 
                                QEMUSnapshotInfo *sn_info);
    int (*bdrv_snapshot_goto)(BlockDriverState *bs, 
                              const char *snapshot_id);
    int (*bdrv_snapshot_delete)(BlockDriverState *bs, const char *snapshot_id);
    int (*bdrv_snapshot_list)(BlockDriverState *bs, 
                              QEMUSnapshotInfo **psn_info);
    int (*bdrv_get_info)(BlockDriverState *bs, BlockDriverInfo *bdi);

    /* removable device specific */
    int (*bdrv_is_inserted)(BlockDriverState *bs);
    int (*bdrv_media_changed)(BlockDriverState *bs);
    int (*bdrv_eject)(BlockDriverState *bs, int eject_flag);
    int (*bdrv_set_locked)(BlockDriverState *bs, int locked);
    
    BlockDriverAIOCB *free_aiocb;
    struct BlockDriver *next;
};

struct BlockDriverState {
	int64_t total_sectors; /* if we are reading a disk image, give its
				  size in sectors */
	int read_only; /* if true, the media is read only */
	int removable; /* if true, the media can be removed */
	int locked;    /* if true, the media cannot temporarily be ejected */
	int encrypted; /* if true, the media is encrypted */
	/* event callback when inserting/removing */
	void (*change_cb)(void *opaque);
	void *change_opaque;

	BlockDriver *drv; /* NULL means no media */
	void *opaque;

	int boot_sector_enabled;
	uint8_t boot_sector_data[512];

	char filename[1024];
	char backing_file[1024]; /* if non zero, the image is a diff of
				    this file image */
	int is_temporary;
	int media_changed;

	BlockDriverState *backing_hd;
//	/* async read/write emulation */
//
//	void *sync_aiocb;
    
	/* NOTE: the following infos are only hints for real hardware
		 drivers. They are not used by the block driver */
	int cyls, heads, secs, translation;
	int type;
	char device_name[32];
	BlockDriverState *next;
};

void *qemu_malloc(size_t size);
void *qemu_mallocz(size_t size);
void qemu_free(void *ptr);

void pstrcpy(char *buf, int buf_size, const char *str);
char *pstrcat(char *buf, int buf_size, const char *s);
int strstart(const char *str, const char *val, const char **ptr);
int stristart(const char *str, const char *val, const char **ptr);
#endif /* QEMU_OLD */

#ifndef QEMU_OLD
int bdrv_truncate(BlockDriverState *bs, int64_t offset);
int64_t bdrv_getlength(BlockDriverState *bs);
void bdrv_flush(BlockDriverState *bs);

/* timers */

typedef struct QEMUClock QEMUClock;
typedef void QEMUTimerCB(void *opaque);

/* The real time clock should be used only for stuff which does not
   change the virtual machine state, as it is run even if the virtual
   machine is stopped. The real time clock has a frequency of 1000
   Hz. */
extern QEMUClock *rt_clock;

int64_t qemu_get_clock(QEMUClock *clock);

QEMUTimer *qemu_new_timer(QEMUClock *clock, QEMUTimerCB *cb, void *opaque);
void qemu_del_timer(QEMUTimer *ts);
void qemu_mod_timer(QEMUTimer *ts, int64_t expire_time);
int qemu_timer_pending(QEMUTimer *ts);

extern int64_t ticks_per_sec;

struct BlockDriverInfo {
    /* in bytes, 0 if irrelevant */
    int cluster_size;
    /* offset at which the VM state can be saved (0 if not possible) */
    int64_t vm_state_offset;
};

#endif

#endif /* !(BLOCK_H) */
