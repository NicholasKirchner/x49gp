/* $Id: main.c,v 1.30 2008/12/11 12:18:17 ecd Exp $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <x49gp.h>
#include <x49gp_ui.h>
#include <memory.h>
#include <s3c2410.h>
#include <s3c2410_power.h>
#include <s3c2410_timer.h>
#include <x49gp_timer.h>

#include "gdbstub.h"

static x49gp_t *x49gp;

#ifdef QEMU_OLD // LD TEMPO HACK
extern
#endif
CPUState *__GLOBAL_env;

int semihosting_enabled = 1;

/* LD TEMPO HACK */
#ifndef QEMU_OLD
uint8_t *phys_ram_base;
int phys_ram_size;
ram_addr_t ram_size = 0x80000; // LD ???

/* vl.c */
int singlestep;

void *qemu_memalign(size_t alignment, size_t size)
{
#if defined(__APPLE__) || defined(_POSIX_C_SOURCE) && !defined(__sun__)
    int ret;
    void *ptr;
    ret = posix_memalign(&ptr, alignment, size);
    if (ret != 0)
        abort();
    return ptr;
#elif defined(CONFIG_BSD)
    return oom_check(valloc(size));
#else
    return oom_check(memalign(alignment, size));
#endif
}


void qemu_init_vcpu(void *_env)
{
    CPUState *env = _env;

    env->nr_cores = 1;
    env->nr_threads = 1;
}

int qemu_cpu_self(void *env)
{
    return 1;
}

void qemu_cpu_kick(void *env)
{
}

void armv7m_nvic_set_pending(void *opaque, int irq)
{
  abort();
}
int armv7m_nvic_acknowledge_irq(void *opaque)
{
  abort();
}
void armv7m_nvic_complete_irq(void *opaque, int irq)
{
  abort();
}

void gdb_register_coprocessor(CPUState * env,
                             void * get_reg, void * set_reg,
                             int num_regs, const char *xml, int g_pos)
{
  fprintf(stderr, "TODO: %s\n", __FUNCTION__);
}

#endif /* !QEMU_OLD */

void *
qemu_malloc(size_t size)
{
	return malloc(size);
}

void *
qemu_mallocz(size_t size)
{
	void *ptr;

	ptr = qemu_malloc(size);
	if (NULL == ptr)
		return NULL;
	memset(ptr, 0, size);
	return ptr;
}

void
qemu_free(void *ptr)
{
	free(ptr);
}

void *
qemu_vmalloc(size_t size)
{
#if defined(__linux__)
	void *mem;
	if (0 == posix_memalign(&mem, sysconf(_SC_PAGE_SIZE), size))
		return mem;
	return NULL;
#else
	return valloc(size);
#endif
}

#ifdef QEMU_OLD
int
term_vprintf(const char *fmt, va_list ap)
{
	return vprintf(fmt, ap);
}

int
term_printf(const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vprintf(fmt, ap);
	va_end(ap);

	return n;
}
#endif

#define SWI_Breakpoint 0x180000

#ifdef QEMU_OLD
int
do_arm_semihosting(CPUState *env, uint32_t number)
#else
uint32_t
do_arm_semihosting(CPUState *env)
#endif
{
#ifndef QEMU_OLD
  uint32_t number;
  if (env->thumb) {
    number = lduw_code(env->regs[15] - 2) & 0xff;
  } else {
    number = ldl_code(env->regs[15] - 4) & 0xffffff;
  }
#endif
	switch (number) {
	case SWI_Breakpoint:
		break;

	case 0:
#ifdef DEBUG_X49GP_SYSCALL
		printf("%s: SWI LR %08x: syscall %u: args %08x %08x %08x %08x %08x %08x %08x\n",
			__FUNCTION__, env->regs[14], env->regs[0],
			env->regs[1], env->regs[2], env->regs[3],
			env->regs[4], env->regs[5], env->regs[6],
			env->regs[7]);
#endif

#if 1
		switch (env->regs[0]) {
		case 305:	/* Beep */
			printf("%s: BEEP: frequency %u, time %u, override %u\n",
				__FUNCTION__, env->regs[1], env->regs[2], env->regs[3]);

			gdk_beep();
			env->regs[0] = 0;
			return 1;

		case 28:	/* CheckBeepEnd */
			env->regs[0] = 0;
			return 1;

		case 29:	/* StopBeep */
			env->regs[0] = 0;
			return 1;

		default:
			break;
		}
#endif
		break;

	default:
		break;
	}

	return 0;
}

void
x49gp_set_idle(x49gp_t *x49gp, x49gp_arm_idle_t idle)
{
#ifdef DEBUG_X49GP_ARM_IDLE
	if (idle != x49gp->arm_idle) {
		printf("%s: arm_idle %u, idle %u\n", __FUNCTION__, x49gp->arm_idle, idle);
	}
#endif

	x49gp->arm_idle = idle;

	if (x49gp->arm_idle == X49GP_ARM_RUN) {
		x49gp->env->halted = 0;
	} else {
		x49gp->env->halted = 1;
#ifdef QEMU_OLD
		cpu_interrupt(x49gp->env, CPU_INTERRUPT_EXIT);
#else
                cpu_exit(x49gp->env);
#endif
	}
}

static void
arm_sighnd(int sig)
{
	switch (sig) {
	case SIGUSR1:
//		stop_simulator = 1;
//		x49gp->arm->CallDebug ^= 1;
		break;
	default:
		fprintf(stderr, "%s: sig %u\n", __FUNCTION__, sig);
		break;
	}
}

void
x49gp_gtk_timer(void *data)
{
	while (gtk_events_pending()) {
// printf("%s: gtk_main_iteration_do()\n", __FUNCTION__);
		gtk_main_iteration_do(FALSE);
	}

	x49gp_mod_timer(x49gp->gtk_timer,
			x49gp_get_clock() + X49GP_GTK_REFRESH_INTERVAL);
}

void
x49gp_lcd_timer(void *data)
{
	x49gp_t *x49gp = data;
	int64_t now, expires;

// printf("%s: lcd_update\n", __FUNCTION__);
	x49gp_lcd_update(x49gp);
	gdk_flush();

	now = x49gp_get_clock();
	expires = now + X49GP_LCD_REFRESH_INTERVAL;

// printf("%s: now: %lld, next update: %lld\n", __FUNCTION__, now, expires);
	x49gp_mod_timer(x49gp->lcd_timer, expires);
}

static void
usage(const char *progname)
{
	fprintf(stderr, "usage: %s <config-file>\n",
		progname);
	exit(1);
}

void
ui_sighnd(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		x49gp->arm_exit = 1;
#ifdef QEMU_OLD
		cpu_interrupt(x49gp->env, CPU_INTERRUPT_EXIT);
#else
                cpu_exit(x49gp->env);
#endif
		break;
	}
}

int
main(int argc, char **argv)
{
	char *progname;
	int error;


	progname = strrchr(argv[0], '/');
	if (progname)
		progname++;
	else
		progname = argv[0];


	gtk_init(&argc, &argv);


	if (argc < 2)
		usage(progname);

	x49gp = malloc(sizeof(x49gp_t));
	if (NULL == x49gp) {
		fprintf(stderr, "%s: %s:%u: Out of memory\n",
			progname, __FUNCTION__, __LINE__);
		exit(1);
	}
	memset(x49gp, 0, sizeof(x49gp_t));

fprintf(stderr, "_SC_PAGE_SIZE: %08lx\n", sysconf(_SC_PAGE_SIZE));

printf("%s:%u: x49gp: %p\n", __FUNCTION__, __LINE__, x49gp);

	INIT_LIST_HEAD(&x49gp->modules);


	x49gp->progname = progname;
	x49gp->clk_tck = sysconf(_SC_CLK_TCK);

	x49gp->emulator_fclk = 75000000;
	x49gp->PCLK_ratio = 4;
	x49gp->PCLK = 75000000 / 4;

#ifdef QEMU_OLD
	x49gp->env = cpu_init();
#else
        //cpu_set_log(0xffffffff);
        cpu_exec_init_all(0);
	x49gp->env = cpu_init("arm926");
#endif
	__GLOBAL_env = x49gp->env;

//	cpu_set_log(cpu_str_to_log_mask("all"));

	x49gp_timer_init(x49gp);

	x49gp->gtk_timer = x49gp_new_timer(X49GP_TIMER_REALTIME,
					   x49gp_gtk_timer, x49gp);
	x49gp->lcd_timer = x49gp_new_timer(X49GP_TIMER_VIRTUAL,
					   x49gp_lcd_timer, x49gp);

	x49gp_ui_init(x49gp);

	x49gp_s3c2410_arm_init(x49gp);

	x49gp_flash_init(x49gp);
	x49gp_sram_init(x49gp);

	x49gp_s3c2410_init(x49gp);

	if (x49gp_modules_init(x49gp)) {
		exit(1);
	}

	error = x49gp_modules_load(x49gp, argv[1]);
	if (error) {
		if (error != -EAGAIN) {
			exit(1);
		}
		x49gp_modules_reset(x49gp, X49GP_RESET_POWER_ON);
	}
// x49gp_modules_reset(x49gp, X49GP_RESET_POWER_ON);

	signal(SIGINT, ui_sighnd);
	signal(SIGTERM, ui_sighnd);
	signal(SIGQUIT, ui_sighnd);

	signal(SIGUSR1, arm_sighnd);


	x49gp_set_idle(x49gp, 0);

// stl_phys(0x08000a1c, 0x55555555);


	x49gp_mod_timer(x49gp->gtk_timer, x49gp_get_clock());
	x49gp_mod_timer(x49gp->lcd_timer, x49gp_get_clock());


#if 0
	gdbserver_start(1234);
	gdb_handlesig(x49gp->env, 0);
#endif

	x49gp_main_loop(x49gp);


	x49gp_modules_save(x49gp, argv[1]);
	x49gp_modules_exit(x49gp);


#if 0
	printf("ClkTicks: %lu\n", ARMul_Time(x49gp->arm));
	printf("D TLB: hit0 %lu, hit1 %lu, search %lu (%lu), walk %lu\n",
		x49gp->mmu->dTLB.hit0, x49gp->mmu->dTLB.hit1,
		x49gp->mmu->dTLB.search, x49gp->mmu->dTLB.nsearch,
		x49gp->mmu->dTLB.walk);
	printf("I TLB: hit0 %lu, hit1 %lu, search %lu (%lu), walk %lu\n",
		x49gp->mmu->iTLB.hit0, x49gp->mmu->iTLB.hit1,
		x49gp->mmu->iTLB.search, x49gp->mmu->iTLB.nsearch,
		x49gp->mmu->iTLB.walk);
#endif
	return 0;
}
