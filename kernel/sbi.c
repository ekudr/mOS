// SPDX-License-Identifier: GPL-2.0-only
/*
 * SBI initialilization and all extension implementation.
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 */

#include <sbi_ecall_interface.h>
#include <common.h>
#include <string.h>
#include <errno.h>
#include <mmu.h>
#include <sched.h>


static void (*__sbi_set_timer)(uint64_t stime);
static void (*__sbi_send_ipi)(unsigned int cpu);
static int (*__sbi_rfence)(int fid,
			   unsigned long start, unsigned long size,
			   unsigned long arg4, unsigned long arg5);

/* default SBI version is 0.1 */
unsigned long sbi_version = 0x1; 

struct sbiret {
	unsigned long error;
	unsigned long value;
};

struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
			unsigned long arg1, unsigned long arg2,
			unsigned long arg3, unsigned long arg4,
			unsigned long arg5)
{
	struct sbiret ret;

	register unsigned long a0 asm ("a0") = (unsigned long)(arg0);
	register unsigned long a1 asm ("a1") = (unsigned long)(arg1);
	register unsigned long a2 asm ("a2") = (unsigned long)(arg2);
	register unsigned long a3 asm ("a3") = (unsigned long)(arg3);
	register unsigned long a4 asm ("a4") = (unsigned long)(arg4);
	register unsigned long a5 asm ("a5") = (unsigned long)(arg5);
	register unsigned long a6 asm ("a6") = (unsigned long)(fid);
	register unsigned long a7 asm ("a7") = (unsigned long)(ext);
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");
	ret.error = a0;
	ret.value = a1;

	return ret;
}

static inline long sbi_get_version(void) {

    struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_GET_SPEC_VERSION, 0, 0, 0, 0, 0, 0);

	if (!ret.error)
		return ret.value;
	else
		return ret.error;
    
}

/**
 * sbi_probe_extension() - Check if an SBI extension ID is supported or not.
 * @extid: The extension ID to be probed.
 *
 * Return: 1 or an extension specific nonzero value if yes, 0 otherwise.
 */
long sbi_probe_extension(int extid)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT, extid,
			0, 0, 0, 0, 0);
	if (!ret.error)
		return ret.value;

	return 0;
}

void __sbi_set_timer_v01(uint64_t stime_value) {
#if __riscv_xlen == 32
	sbi_ecall(SBI_EXT_0_1_SET_TIMER, 0, stime_value,
		  stime_value >> 32, 0, 0, 0, 0);
#else
	sbi_ecall(SBI_EXT_0_1_SET_TIMER, 0, stime_value, 0, 0, 0, 0, 0);
#endif
}

void __sbi_set_timer_v02(uint64_t stime_value) {
#if __riscv_xlen == 32
	sbi_ecall(SBI_EXT_TIME, SBI_EXT_TIME_SET_TIMER, stime_value,
		  stime_value >> 32, 0, 0, 0, 0);
#else
	sbi_ecall(SBI_EXT_TIME, SBI_EXT_TIME_SET_TIMER, stime_value, 0,
		  0, 0, 0, 0);
#endif
}

/**
 * sbi_set_timer() - Program the timer for next timer event.
 * @stime_value: The value after which next timer event should fire.
 *
 * Return: None.
 */
void sbi_set_timer(uint64_t stime_value) {
	__sbi_set_timer(stime_value);
}

int sbi_hsm_hart_get_status(unsigned long hartid) {
    struct sbiret ret;
    ret = sbi_ecall(SBI_EXT_HSM, SBI_EXT_HSM_HART_GET_STATUS,
        hartid, 0, 0, 0, 0, 0);
    if (!ret.error)
        return ret.value;
    else
        return ret.error;
}

void sbi_console_putc(char ch) {
    sbi_ecall(SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, ch, 0, 0, 0, 0, 0);
}

int sbi_hsm_hart_start(unsigned long hartid, unsigned long saddr, unsigned long priv) {
    struct sbiret ret;

    ret = sbi_ecall(SBI_EXT_HSM, SBI_EXT_HSM_HART_START, hartid, saddr, priv, 0, 0, 0);

    if (!ret.error)
        return ret.value;
    else
        return ret.error;
}

bool sbi_debug_console_available;

int sbi_debug_console_write(const char *bytes, unsigned int num_bytes)
{
	uint64_t base_addr;
	struct sbiret ret;

	if (!sbi_debug_console_available)
		return -ENOSUPPORT;

	base_addr = DA2PA(bytes);

//    early_printf("Phys addr 0x%lX\n", (uint64_t)base_addr);

#if __riscv_xlen == 32
		ret = sbi_ecall(SBI_EXT_DBCN, SBI_EXT_DBCN_CONSOLE_WRITE,
				num_bytes, base_addr,
				base_addr >> 32, 0, 0, 0);

#else
		ret = sbi_ecall(SBI_EXT_DBCN, SBI_EXT_DBCN_CONSOLE_WRITE,
				num_bytes, base_addr, 0, 0, 0, 0);
#endif

//    early_printf("SBI ret 0x%lX:0x%lX\n", ret.error, ret.value);
	if (ret.error < 0)
		return -EIO;
	return ret.error ? -EIO : ret.value;
}

static void __sbi_send_ipi_v01(unsigned int cpu)
{
	printf("IPI extension is not available in SBI v01\n");
}

static int __sbi_rfence_v01(int fid,
			    unsigned long start, unsigned long size,
			    unsigned long arg4, unsigned long arg5)
{
	printf("remote fence extension is not available in SBI\n");

	return 0;
}

static void __sbi_send_ipi_v02(unsigned int cpu)
{
	int result;
	struct sbiret ret = {0};

	ret = sbi_ecall(SBI_EXT_IPI, SBI_EXT_IPI_SEND_IPI,
			1UL, CPU2HARTID(cpu), 0, 0, 0, 0);
	if (ret.error) {
		result = ret.error;
		printf("%s: hbase = [%lu] failed (error [%d])\n",
			__func__, CPU2HARTID(cpu), result);
	}
}

static int __sbi_rfence_v02_call(unsigned long fid, unsigned long hmask,
				 unsigned long hbase, unsigned long start,
				 unsigned long size, unsigned long arg4,
				 unsigned long arg5)
{
	struct sbiret ret = {0};
	int ext = SBI_EXT_RFENCE;
	int result = 0;

	switch (fid) {
	case SBI_EXT_RFENCE_REMOTE_FENCE_I:
		ret = sbi_ecall(ext, fid, hmask, hbase, 0, 0, 0, 0);
		break;
	case SBI_EXT_RFENCE_REMOTE_SFENCE_VMA:
		ret = sbi_ecall(ext, fid, hmask, hbase, start,
				size, 0, 0);
		break;
	case SBI_EXT_RFENCE_REMOTE_SFENCE_VMA_ASID:
		ret = sbi_ecall(ext, fid, hmask, hbase, start,
				size, arg4, 0);
		break;

	case SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA:
		ret = sbi_ecall(ext, fid, hmask, hbase, start,
				size, 0, 0);
		break;
	case SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA_VMID:
		ret = sbi_ecall(ext, fid, hmask, hbase, start,
				size, arg4, 0);
		break;
	case SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA:
		ret = sbi_ecall(ext, fid, hmask, hbase, start,
				size, 0, 0);
		break;
	case SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA_ASID:
		ret = sbi_ecall(ext, fid, hmask, hbase, start,
				size, arg4, 0);
		break;
	default:
		printf("unknown function ID [%lu] for SBI extension [%d]\n",
		       fid, ext);
		result = -EINVAL;
	}
    
	if (ret.error) {
		result = ret.error;
		printf("%s: hbase = [%lu] hmask = [0x%lx] failed (error [%d])\n",
		       __func__, hbase, hmask, result);
	}

	return result;
}

static int __sbi_rfence_v02(int fid,
			    unsigned long start, unsigned long size,
			    unsigned long arg4, unsigned long arg5)
{
	unsigned long hartid, hmask = 0, hbase = 0;
	int result;
    hbase = 0;
    for (int i = 0; i < NCPUS; i++) {
        hartid = CPU2HARTID(i);
        if (hartid != current_cpu->hartid) {
            hmask |= BIT(hartid);
        }
    }


	if (hmask) {
		result = __sbi_rfence_v02_call(fid, hmask, hbase,
					       start, size, arg4, arg5);
		if (result)
			return result;
	}

	return 0;
}

int sbi_remote_hfence_vvma(unsigned long start, unsigned long size)
{
	return __sbi_rfence(SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA,
			     start, size, 0, 0);
}



int sbi_remote_sfence_vma(
				unsigned long start,
				unsigned long size)
{

		return __sbi_rfence(SBI_EXT_RFENCE_REMOTE_SFENCE_VMA,
				    start, size, 0, 0);

}

void sbi_init (void) {
    int ret;

    ret = sbi_get_version();

    if (ret) sbi_version = ret;

    early_printf("SBI detected version %d.%d\n", (sbi_version >> 24) & 0x7f, sbi_version & 0x7f);

	if (sbi_probe_extension(SBI_EXT_TIME)) {
		__sbi_set_timer = __sbi_set_timer_v02;
		early_printf("SBI TIME extension detected\n");
	} else {
		__sbi_set_timer = __sbi_set_timer_v01;
	}

	if (sbi_probe_extension(SBI_EXT_HSM)) {
				
		early_printf("SBI HSM extension detected\n");
		for(int i=0; i<NCPUS; i++){
			early_printf("SBI HSM hart %d status %d\n", i, sbi_hsm_hart_get_status(CPU2HARTID(i)));
		}
	}

    if (sbi_probe_extension(SBI_EXT_IPI)) {
        __sbi_send_ipi	= __sbi_send_ipi_v02;
        early_printf("SBI IPI extension detected\n");
    } else {
        __sbi_send_ipi	= __sbi_send_ipi_v01;
    }
    if (sbi_probe_extension(SBI_EXT_RFENCE)) {
        __sbi_rfence	= __sbi_rfence_v02;
        early_printf("SBI RFENCE extension detected\n");
    } else {
        __sbi_rfence	= __sbi_rfence_v01;
    }

    if (sbi_probe_extension(SBI_EXT_DBCN)) {
        early_printf("SBI DBCN extension detected\n");
        sbi_debug_console_available = true;
    }
    
}

void sbi_hsm_info(void) {
		if (sbi_probe_extension(SBI_EXT_HSM)) {
				
		early_printf("SBI HSM extension detected\n");
		for(int i=0; i<NCPUS; i++){
			early_printf("SBI HSM hart %d status %d\n", i, sbi_hsm_hart_get_status(CPU2HARTID(i)));
		}
	}
}

