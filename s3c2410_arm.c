/* $Id: s3c2410_arm.c,v 1.7 2008/12/11 12:18:17 ecd Exp $
 */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <cpu.h>

#include <x49gp.h>
#include <s3c2410.h>

#ifdef QEMU_OLD
extern void tlb_flush(struct CPUState *, int global);
#else
#include "cpu-all.h"
#endif

static int
s3c2410_arm_load(x49gp_module_t *module, GKeyFile *key)
{
	struct CPUARMState *env = module->user_data;
	char name[32];
	int error = 0;
	int i;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	cpu_reset(env);
	tlb_flush(env, 1);

	for (i = 0; i < (sizeof(env->regs) / sizeof(env->regs[0])); i++) {
		sprintf(name, "reg-%02u", i);
		if (x49gp_module_get_u32(module, key, name, 0, &env->regs[i]))
			error = -EAGAIN;
	}
	if (x49gp_module_get_u32(module, key, "cpsr", 0, &env->uncached_cpsr))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "spsr", 0, &env->spsr))
		error = -EAGAIN;
	for (i = 0; i < (sizeof(env->banked_spsr) / sizeof(env->banked_spsr[0])); i++) {
		sprintf(name, "banked-spsr-%02u", i);
		if (x49gp_module_get_u32(module, key, name, 0, &env->banked_spsr[i]))
			error = -EAGAIN;
	}
	for (i = 0; i < (sizeof(env->banked_r13) / sizeof(env->banked_r13[0])); i++) {
		sprintf(name, "banked-r13-%02u", i);
		if (x49gp_module_get_u32(module, key, name, 0, &env->banked_r13[i]))
			error = -EAGAIN;
	}
	for (i = 0; i < (sizeof(env->banked_r14) / sizeof(env->banked_r14[0])); i++) {
		sprintf(name, "banked-r14-%02u", i);
		if (x49gp_module_get_u32(module, key, name, 0, &env->banked_r14[i]))
			error = -EAGAIN;
	}
	for (i = 0; i < (sizeof(env->usr_regs) / sizeof(env->usr_regs[0])); i++) {
		sprintf(name, "reg-usr-%02u", i);
		if (x49gp_module_get_u32(module, key, name,
				         0, &env->usr_regs[i]))
			error = -EAGAIN;
	}
	for (i = 0; i < (sizeof(env->fiq_regs) / sizeof(env->fiq_regs[0])); i++) {
		sprintf(name, "reg-fiq-%02u", i);
		if (x49gp_module_get_u32(module, key, name,
				         0, &env->fiq_regs[i]))
			error = -EAGAIN;
	}

	if (x49gp_module_get_u32(module, key, "CF", 0, &env->CF))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "VF", 0, &env->VF))
		error = -EAGAIN;
#ifdef QEMU_OLD
	if (x49gp_module_get_u32(module, key, "NZF", 0, &env->NZF))
		error = -EAGAIN;
#else
	if (x49gp_module_get_u32(module, key, "NF", 0, &env->NF))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "ZF", 0, &env->ZF))
		error = -EAGAIN;
#endif
	if (x49gp_module_get_u32(module, key, "QF", 0, &env->QF))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "thumb", 0, &env->thumb))
		error = -EAGAIN;

	if (x49gp_module_get_u32(module, key, "cp15-c0-cpuid", 0, &env->cp15.c0_cpuid))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c1-sys", 0, &env->cp15.c1_sys))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c1-coproc", 0, &env->cp15.c1_coproc))
		error = -EAGAIN;
#ifdef QEMU_OLD
	if (x49gp_module_get_u32(module, key, "cp15-c2", 0, &env->cp15.c2))
		error = -EAGAIN;
#else
	if (x49gp_module_get_u32(module, key, "cp15-c2-base0", 0, &env->cp15.c2_base0))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c2-base1", 0, &env->cp15.c2_base1))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c2-control", 0, &env->cp15.c2_control))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c2-mask", 0, &env->cp15.c2_mask))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c2-base-mask", 0, &env->cp15.c2_base_mask))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c2-data", 0, &env->cp15.c2_data))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c2-insn", 0, &env->cp15.c2_insn))
		error = -EAGAIN;
#endif
	if (x49gp_module_get_u32(module, key, "cp15-c3", 0, &env->cp15.c3))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c5-insn", 0, &env->cp15.c5_insn))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c5-data", 0, &env->cp15.c5_data))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c6-insn", 0, &env->cp15.c6_insn))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c6-data", 0, &env->cp15.c6_data))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c9-insn", 0, &env->cp15.c9_insn))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c9-data", 0, &env->cp15.c9_data))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c13-fcse", 0, &env->cp15.c13_fcse))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "cp15-c13-context", 0, &env->cp15.c13_context))
		error = -EAGAIN;

	if (x49gp_module_get_u32(module, key, "features", 0, &env->features))
		error = -EAGAIN;

	if (x49gp_module_get_int(module, key, "exception-index", 0, &env->exception_index))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "interrupt-request", 0, &env->interrupt_request))
		error = -EAGAIN;
	if (x49gp_module_get_u32(module, key, "halted", 0, &env->halted))
		error = -EAGAIN;

	env->exception_index = -1;

	if (0 == error) {
		if (env->halted)
			module->x49gp->arm_idle = 1;
	} else {
		memset(&env->cp15, 0, sizeof(env->cp15));
	}

//	s3c2410_arm_dump_state(state);

	return error;
}

static int
s3c2410_arm_save(x49gp_module_t *module, GKeyFile *key)
{
	struct CPUARMState *env = module->user_data;
	char name[32];
	int i;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	for (i = 0; i < (sizeof(env->regs) / sizeof(env->regs[0])); i++) {
		sprintf(name, "reg-%02u", i);
		x49gp_module_set_u32(module, key, name, env->regs[i]);
	}
	x49gp_module_set_u32(module, key, "cpsr", env->uncached_cpsr);
	x49gp_module_set_u32(module, key, "spsr", env->spsr);
	for (i = 0; i < (sizeof(env->banked_spsr) / sizeof(env->banked_spsr[0])); i++) {
		sprintf(name, "banked-spsr-%02u", i);
		x49gp_module_set_u32(module, key, name, env->banked_spsr[i]);
	}
	for (i = 0; i < (sizeof(env->banked_r13) / sizeof(env->banked_r13[0])); i++) {
		sprintf(name, "banked-r13-%02u", i);
		x49gp_module_set_u32(module, key, name, env->banked_r13[i]);
	}
	for (i = 0; i < (sizeof(env->banked_r14) / sizeof(env->banked_r14[0])); i++) {
		sprintf(name, "banked-r14-%02u", i);
		x49gp_module_set_u32(module, key, name, env->banked_r14[i]);
	}
	for (i = 0; i < (sizeof(env->usr_regs) / sizeof(env->usr_regs[0])); i++) {
		sprintf(name, "reg-usr-%02u", i);
		x49gp_module_set_u32(module, key, name, env->usr_regs[i]);
	}
	for (i = 0; i < (sizeof(env->fiq_regs) / sizeof(env->fiq_regs[0])); i++) {
		sprintf(name, "reg-fiq-%02u", i);
		x49gp_module_set_u32(module, key, name, env->fiq_regs[i]);
	}

	x49gp_module_set_u32(module, key, "CF", env->CF);
	x49gp_module_set_u32(module, key, "VF", env->VF);
#ifdef QEMU_OLD
	x49gp_module_set_u32(module, key, "NZF", env->NZF);
#else
	x49gp_module_set_u32(module, key, "NF", env->NF);
	x49gp_module_set_u32(module, key, "ZF", env->ZF);
#endif
	x49gp_module_set_u32(module, key, "QF", env->QF);
	x49gp_module_set_int(module, key, "thumb", env->thumb);

	x49gp_module_set_u32(module, key, "cp15-c0-cpuid", env->cp15.c0_cpuid);
	x49gp_module_set_u32(module, key, "cp15-c1-sys", env->cp15.c1_sys);
	x49gp_module_set_u32(module, key, "cp15-c1-coproc", env->cp15.c1_coproc);
#ifdef QEMU_OLD
	x49gp_module_set_u32(module, key, "cp15-c2", env->cp15.c2);
#else
	x49gp_module_set_u32(module, key, "cp15-c2-base0", env->cp15.c2_base0);
	x49gp_module_set_u32(module, key, "cp15-c2-base1", env->cp15.c2_base1);
	x49gp_module_set_u32(module, key, "cp15-c2-control", env->cp15.c2_control);
	x49gp_module_set_u32(module, key, "cp15-c2-mask", env->cp15.c2_mask);
	x49gp_module_set_u32(module, key, "cp15-c2-base-mask", env->cp15.c2_base_mask);
	x49gp_module_set_u32(module, key, "cp15-c2-data", env->cp15.c2_data);
	x49gp_module_set_u32(module, key, "cp15-c2-insn", env->cp15.c2_insn);
#endif
	x49gp_module_set_u32(module, key, "cp15-c3", env->cp15.c3);
	x49gp_module_set_u32(module, key, "cp15-c5-insn", env->cp15.c5_insn);
	x49gp_module_set_u32(module, key, "cp15-c5-data", env->cp15.c5_data);
	x49gp_module_set_u32(module, key, "cp15-c6-insn", env->cp15.c6_insn);
	x49gp_module_set_u32(module, key, "cp15-c6-data", env->cp15.c6_data);
	x49gp_module_set_u32(module, key, "cp15-c9-insn", env->cp15.c9_insn);
	x49gp_module_set_u32(module, key, "cp15-c9-data", env->cp15.c9_data);
	x49gp_module_set_u32(module, key, "cp15-c13-fcse", env->cp15.c13_fcse);
	x49gp_module_set_u32(module, key, "cp15-c13-context", env->cp15.c13_context);

	x49gp_module_set_u32(module, key, "features", env->features);

	x49gp_module_set_int(module, key, "exception-index", env->exception_index);
	x49gp_module_set_int(module, key, "interrupt-request", env->interrupt_request);
	x49gp_module_set_int(module, key, "halted", env->halted);

	return 0;
}

static int
s3c2410_arm_reset(x49gp_module_t *module, x49gp_reset_t reset)
{
	struct CPUARMState *env = module->user_data;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	cpu_reset(env);
	tlb_flush(env, 1);

	return 0;
}

static int
s3c2410_arm_init(x49gp_module_t *module)
{
	x49gp_t *x49gp = module->x49gp;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

#ifdef QEMU_OLD
	cpu_arm_set_model(x49gp->env, ARM_CPUID_ARM926);
#endif

	module->user_data = x49gp->env;
	return 0;
}

static int
s3c2410_arm_exit(x49gp_module_t *module)
{
#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	return 0;
}

int
x49gp_s3c2410_arm_init(x49gp_t *x49gp)
{
	x49gp_module_t *module;

	if (x49gp_module_init(x49gp, "s3c2410-arm",
			      s3c2410_arm_init,
			      s3c2410_arm_exit,
			      s3c2410_arm_reset,
			      s3c2410_arm_load,
			      s3c2410_arm_save,
			      NULL, &module)) {
		return -1;
	}

	return x49gp_module_register(module);
}
