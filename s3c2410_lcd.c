/* $Id: s3c2410_lcd.c,v 1.4 2008/12/11 12:18:17 ecd Exp $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include <gtk/gtk.h>

#include <x49gp.h>
#include <x49gp_ui.h>
#include <s3c2410.h>


typedef struct {
	uint32_t			lcdcon1;
	uint32_t			lcdcon2;
	uint32_t			lcdcon3;
	uint32_t			lcdcon4;
	uint32_t			lcdcon5;
	uint32_t			lcdsaddr1;
	uint32_t			lcdsaddr2;
	uint32_t			lcdsaddr3;
	uint32_t			redlut;
	uint32_t			greenlut;
	uint32_t			bluelut;
	uint32_t			dithmode;
	uint32_t			tpal;
	uint32_t			lcdintpnd;
	uint32_t			lcdsrcpnd;
	uint32_t			lcdintmsk;
	uint32_t			lpcsel;
	uint32_t			__unknown_68;

	unsigned int		nr_regs;
	s3c2410_offset_t	*regs;

	x49gp_t			*x49gp;
} s3c2410_lcd_t;

static int
s3c2410_lcd_data_init(s3c2410_lcd_t *lcd)
{
	s3c2410_offset_t regs[] = {
		S3C2410_OFFSET(LCD, LCDCON1, 0x00000000, lcd->lcdcon1),
		S3C2410_OFFSET(LCD, LCDCON2, 0x00000000, lcd->lcdcon2),
		S3C2410_OFFSET(LCD, LCDCON3, 0x00000000, lcd->lcdcon3),
		S3C2410_OFFSET(LCD, LCDCON4, 0x00000000, lcd->lcdcon4),
		S3C2410_OFFSET(LCD, LCDCON5, 0x00000000, lcd->lcdcon5),
		S3C2410_OFFSET(LCD, LCDSADDR1, 0x00000000, lcd->lcdsaddr1),
		S3C2410_OFFSET(LCD, LCDSADDR2, 0x00000000, lcd->lcdsaddr2),
		S3C2410_OFFSET(LCD, LCDSADDR3, 0x00000000, lcd->lcdsaddr3),
		S3C2410_OFFSET(LCD, REDLUT, 0x00000000, lcd->redlut),
		S3C2410_OFFSET(LCD, GREENLUT, 0x00000000, lcd->greenlut),
		S3C2410_OFFSET(LCD, BLUELUT, 0x00000000, lcd->bluelut),
		S3C2410_OFFSET(LCD, DITHMODE, 0x00000000, lcd->dithmode),
		S3C2410_OFFSET(LCD, TPAL, 0x00000000, lcd->tpal),
		S3C2410_OFFSET(LCD, LCDINTPND, 0x00000000, lcd->lcdintpnd),
		S3C2410_OFFSET(LCD, LCDSRCPND, 0x00000000, lcd->lcdsrcpnd),
		S3C2410_OFFSET(LCD, LCDINTMSK, 0x00000003, lcd->lcdintmsk),
		S3C2410_OFFSET(LCD, LPCSEL, 0x00000004, lcd->lpcsel),
		S3C2410_OFFSET(LCD, UNKNOWN_68, 0x00000000, lcd->__unknown_68)
	};

	memset(lcd, 0, sizeof(s3c2410_lcd_t));

	lcd->regs = malloc(sizeof(regs));
	if (NULL == lcd->regs) {
		fprintf(stderr, "%s:%u: Out of memory\n",
			__FUNCTION__, __LINE__);
		return -ENOMEM;
	}

	memcpy(lcd->regs, regs, sizeof(regs));
	lcd->nr_regs = sizeof(regs) / sizeof(regs[0]);

	return 0;
}

void
x49gp_schedule_lcd_update(x49gp_t *x49gp)
{
	unsigned long ticks;

	ticks = (x49gp->emulator_fclk / x49gp->PCLK_ratio) / 100;

	if (! x49gp_timer_pending(x49gp->lcd_timer)) {
		x49gp_mod_timer(x49gp->lcd_timer,
				x49gp_get_clock() + X49GP_LCD_REFRESH_INTERVAL);
	}
}

void
x49gp_lcd_update(x49gp_t *x49gp)
{
	x49gp_ui_t *ui = x49gp->ui;
	s3c2410_lcd_t *lcd = x49gp->s3c2410_lcd;
	GdkRectangle rect;
	uint32_t bank, addr, offset, data;
	int row, i, b, x, y;

	if (!(lcd->lcdcon1 & 1)) {
		gdk_draw_drawable(ui->lcd_pixmap, ui->window->style->bg_gc[0],
				  ui->bg_pixmap,
				  ui->lcd_x_offset, ui->lcd_y_offset,
				  0, 0, ui->lcd_width, ui->lcd_height);
		goto done;
	}

	bank = (lcd->lcdsaddr1 << 1) & 0x7fc00000;
	addr = bank | ((lcd->lcdsaddr1 << 1) & 0x003ffffe);

	gdk_draw_drawable(ui->lcd_pixmap, ui->window->style->bg_gc[0],
			  ui->bg_pixmap,
			  ui->lcd_x_offset, ui->lcd_y_offset,
			  0, 0, ui->lcd_width, ui->lcd_height);

	data = ldl_phys(addr + 16);
	if (data & (1 << 3)) {
		gdk_draw_rectangle(ui->lcd_pixmap, ui->ann_io_gc, TRUE,
				   236, 0, 15, 12);
	}
	data = ldl_phys(addr + 36);
	if (data & (1 << 3)) {
		gdk_draw_rectangle(ui->lcd_pixmap, ui->ann_left_gc, TRUE,
				   11, 0, 15, 12);
	}
	data = ldl_phys(addr + 56);
	if (data & (1 << 3)) {
		gdk_draw_rectangle(ui->lcd_pixmap, ui->ann_right_gc, TRUE,
				   56, 0, 15, 12);
	}
	data = ldl_phys(addr + 76);
	if (data & (1 << 3)) {
		gdk_draw_rectangle(ui->lcd_pixmap, ui->ann_alpha_gc, TRUE,
				   101, 0, 15, 12);
	}
	data = ldl_phys(addr + 96);
	if (data & (1 << 3)) {
		gdk_draw_rectangle(ui->lcd_pixmap, ui->ann_battery_gc, TRUE,
				   146, 0, 15, 12);
	}
	data = ldl_phys(addr + 116);
	if (data & (1 << 3)) {
		gdk_draw_rectangle(ui->lcd_pixmap, ui->ann_busy_gc, TRUE,
				   191, 0, 15, 12);
	}

	offset = 0;
	for (row = 0; row < ui->lcd_height / 2; row++) {
		y = 2 * row;
		x = 0;
		for (i = 0; i < 5; i++) {
			data = ldl_phys(addr + offset);
			offset += sizeof(uint32_t);

			for (b = 0; b < 32; b++) {
				if (data & (1 << b)) {
					gdk_draw_rectangle(ui->lcd_pixmap,
							   ui->window->style->fg_gc[0],
							   TRUE, x, y + ui->lcd_top_margin, 2, 2);
				}
				x += 2;

				if (x >= ui->lcd_width) {
					break;
				}
			}
		}
	}

	addr = bank | ((lcd->lcdsaddr2 << 1) & 0x003ffffe);

done:
	rect.x = 0;
	rect.y = 0;
	rect.width = ui->lcd_width;
	rect.height = ui->lcd_height;

	gdk_window_invalidate_rect(ui->lcd_canvas->window, &rect, FALSE);
}


static uint32_t
s3c2410_lcd_read(void *opaque, target_phys_addr_t offset)
{
	s3c2410_lcd_t *lcd = opaque;
	s3c2410_offset_t *reg;
	uint32_t linecnt;

#ifdef QEMU_OLD
	offset -= S3C2410_LCD_BASE;
#endif
	if (! S3C2410_OFFSET_OK(lcd, offset)) {
		return ~(0);
	}

	reg = S3C2410_OFFSET_ENTRY(lcd, offset);

	switch (offset) {
	case S3C2410_LCD_LCDCON1:
		linecnt = (lcd->lcdcon1 >> 18) & 0x3ff;
		if (linecnt > 0) {
			linecnt--;
		} else {
			linecnt = (lcd->lcdcon2 >> 14) & 0x3ff;
		}

		lcd->lcdcon1 &= ~(0x3ff << 18);
		lcd->lcdcon1 |= (linecnt << 18);
	}

#ifdef DEBUG_S3C2410_LCD
	printf("read  %s [%08x] %s [%08x] data %08x\n",
		"s3c2410-lcd", S3C2410_LCD_BASE,
		reg->name, offset, *(reg->datap));
#endif

	return *(reg->datap);
}

static void
s3c2410_lcd_write(void *opaque, target_phys_addr_t offset, uint32_t data)
{
	s3c2410_lcd_t *lcd = opaque;
	x49gp_t *x49gp = lcd->x49gp;
	s3c2410_offset_t *reg;

#ifdef QEMU_OLD
	offset -= S3C2410_LCD_BASE;
#endif
	if (! S3C2410_OFFSET_OK(lcd, offset)) {
		return;
	}

	reg = S3C2410_OFFSET_ENTRY(lcd, offset);

#ifdef DEBUG_S3C2410_LCD
	printf("write %s [%08x] %s [%08x] data %08x\n",
		"s3c2410-lcd", S3C2410_LCD_BASE,
		reg->name, offset, data);
#endif

	switch (offset) {
	case S3C2410_LCD_LCDCON1:
		if ((lcd->lcdcon1 ^ data) & 1) {
			x49gp_schedule_lcd_update(x49gp);
		}

		lcd->lcdcon1 = (lcd->lcdcon1 & (0x3ff << 18)) |
			       (data & ~(0x3ff << 18));
		break;
	default:
		*(reg->datap) = data;
		break;
	}
}


static int
s3c2410_lcd_load(x49gp_module_t *module, GKeyFile *key)
{
	s3c2410_lcd_t *lcd = module->user_data;
	s3c2410_offset_t *reg;
	int error = 0;
	int i;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	for (i = 0; i < lcd->nr_regs; i++) {
		reg = &lcd->regs[i];

		if (NULL == reg->name)
			continue;

		if (x49gp_module_get_u32(module, key, reg->name,
					 reg->reset, reg->datap))
			error = -EAGAIN;
	}

	return error;
}

static int
s3c2410_lcd_save(x49gp_module_t *module, GKeyFile *key)
{
	s3c2410_lcd_t *lcd = module->user_data;
	s3c2410_offset_t *reg;
	int i;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	for (i = 0; i < lcd->nr_regs; i++) {
		reg = &lcd->regs[i];

		if (NULL == reg->name)
			continue;

		x49gp_module_set_u32(module, key, reg->name, *(reg->datap));
	}

	return 0;
}

static int
s3c2410_lcd_reset(x49gp_module_t *module, x49gp_reset_t reset)
{
	s3c2410_lcd_t *lcd = module->user_data;
	s3c2410_offset_t *reg;
	int i;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	for (i = 0; i < lcd->nr_regs; i++) {
		reg = &lcd->regs[i];

		if (NULL == reg->name)
			continue;

		*(reg->datap) = reg->reset;
	}

	return 0;
}

static CPUReadMemoryFunc *s3c2410_lcd_readfn[] =
{
	s3c2410_lcd_read,
	s3c2410_lcd_read,
	s3c2410_lcd_read
};

static CPUWriteMemoryFunc *s3c2410_lcd_writefn[] =
{
	s3c2410_lcd_write,
	s3c2410_lcd_write,
	s3c2410_lcd_write
};

static int
s3c2410_lcd_init(x49gp_module_t *module)
{
	s3c2410_lcd_t *lcd;
	int iotype;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	lcd = malloc(sizeof(s3c2410_lcd_t));
	if (NULL == lcd) {
		fprintf(stderr, "%s:%u: Out of memory\n",
			__FUNCTION__, __LINE__);
		return -ENOMEM;
	}
	if (s3c2410_lcd_data_init(lcd)) {
		free(lcd);
		return -ENOMEM;
	}

	module->user_data = lcd;
	module->x49gp->s3c2410_lcd = lcd;
	lcd->x49gp = module->x49gp;

#ifdef QEMU_OLD
	iotype = cpu_register_io_memory(0, s3c2410_lcd_readfn,
					s3c2410_lcd_writefn, lcd);
#else
	iotype = cpu_register_io_memory(s3c2410_lcd_readfn,
					s3c2410_lcd_writefn, lcd);
#endif
printf("%s: iotype %08x\n", __FUNCTION__, iotype);
	cpu_register_physical_memory(S3C2410_LCD_BASE, S3C2410_MAP_SIZE, iotype);
	return 0;
}

static int
s3c2410_lcd_exit(x49gp_module_t *module)
{
	s3c2410_lcd_t *lcd;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	if (module->user_data) {
		lcd = module->user_data;
		if (lcd->regs)
			free(lcd->regs);
		free(lcd);
	}

	x49gp_module_unregister(module);
	free(module);

	return 0;
}

int
x49gp_s3c2410_lcd_init(x49gp_t *x49gp)
{
	x49gp_module_t *module;

	if (x49gp_module_init(x49gp, "s3c2410-lcd",
			      s3c2410_lcd_init,
			      s3c2410_lcd_exit,
			      s3c2410_lcd_reset,
			      s3c2410_lcd_load,
			      s3c2410_lcd_save,
			      NULL, &module)) {
		return -1;
	}

	return x49gp_module_register(module);
}
