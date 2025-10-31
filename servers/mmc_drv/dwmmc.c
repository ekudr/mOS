#include <string.h>
#include <mosstd.h>
#include <libsys/timer.h>
#include "mmc.h"
#include "mmio.h"
#include "jh7110.h"

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

static inline int __test_and_clear_bit_1(int nr, void *addr)
{
	int mask, retval;
	unsigned int *a = (unsigned int *)addr;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *a) != 0;
	*a &= ~mask;

	return retval;
}
 
static int dw_wait_reset(struct mmc_host *host, uint32_t value)
{
	unsigned long timeout = 1000;
	uint32_t ctrl;

	writel(host, value, DWMCI_CTRL);

	while (timeout--) {
		ctrl = readl(host, DWMCI_CTRL);
		if (!(ctrl & DWMCI_RESET_ALL))
			return SUCCESS;
	}
	return -ETIMEOUT;
}

static int dw_fifo_ready(struct mmc_host *host, uint32_t bit, uint32_t *len)
{
	u32 timeout = 20000;

	*len = readl(host, DWMCI_STATUS);
	while (--timeout && (*len & bit)) {
		udelay(200);
		*len = readl(host, DWMCI_STATUS);
	}

	if (!timeout) {
		debug("[DWMMC] %s.%d: FIFO underflow timeout\n", __func__, __LINE__);
		return -ETIMEOUT;
	}

	return SUCCESS;
}

static unsigned int dw_get_timeout(struct mmc_host *host, const unsigned int size)
{
	unsigned int timeout;

	timeout = size * 8;	/* counting in bits */
	timeout *= 10;		/* wait 10 times as long */
	timeout /= host->cur_clock;
	timeout /= host->buswidth;
	timeout /= host->card->ddr_mode ? 2 : 1;
	timeout *= 1000;	/* counting in msec  */
	timeout = (timeout < 1000) ? 1000 : timeout;

	return timeout;
}

static int dw_data_transfer(struct mmc_host *host, struct mmc_data *data)
{
	int ret = 0;
	uint32_t timeout, mask, size, i, len = 0;
	uint32_t *buf = NULL;
	uint64_t start = get_timer(0);
	uint32_t fifo_depth = (((host->fifoth_val & RX_WMARK_MASK) >>
			    RX_WMARK_SHIFT) + 1) * 2;

	size = data->blocksize * data->blocks;
	if (data->flags == MMC_DATA_READ)
		buf = (unsigned int *)data->dest;
	else
		buf = (unsigned int *)data->src;

	timeout = dw_get_timeout(host, size);

	size /= 4;

	for (;;) {
		mask = readl(host, DWMCI_RINTSTS);
		/* Error during data transfer. */
		if (mask & (DWMCI_DATA_ERR | DWMCI_DATA_TOUT)) {
			debug("[DWMMC] %s.%d: DATA ERROR!\n", __func__, __LINE__);
			ret = -EINVAL;
			break;
		}

		if (host->fifo_mode && size) {
			len = 0;
			if (data->flags == MMC_DATA_READ &&
			    (mask & (DWMCI_INTMSK_RXDR | DWMCI_INTMSK_DTO))) {
				writel(host, DWMCI_INTMSK_RXDR | DWMCI_INTMSK_DTO, DWMCI_RINTSTS);
				while (size) {
					ret = dw_fifo_ready(host,
							DWMCI_FIFO_EMPTY,
							&len);
					if (ret < 0)
						break;

					len = (len >> DWMCI_FIFO_SHIFT) &
						    DWMCI_FIFO_MASK;
					len = min(size, len);
					for (i = 0; i < len; i++)
						*buf++ =
						readl(host, DWMCI_DATA);
					size = size > len ? (size - len) : 0;
				}
			} else if (data->flags == MMC_DATA_WRITE &&
				   (mask & DWMCI_INTMSK_TXDR)) {
				while (size) {
					ret = dw_fifo_ready(host,
							DWMCI_FIFO_FULL,
							&len);
					if (ret < 0)
						break;

					len = fifo_depth - ((len >>
						   DWMCI_FIFO_SHIFT) &
						   DWMCI_FIFO_MASK);
					len = min(size, len);
					for (i = 0; i < len; i++)
						writel(host, *buf++, DWMCI_DATA);
					size = size > len ? (size - len) : 0;
				}
				writel(host, DWMCI_INTMSK_TXDR, DWMCI_RINTSTS);
			}
		}

		/* Data arrived correctly. */
		if (mask & DWMCI_INTMSK_DTO) {
			ret = 0;
			break;
		}

		/* Check for timeout. */
		if ((get_timer(start) ) > timeout) {
			debug("[DWMMC] %s.%d: Timeout waiting for data!\n",
			      __func__, __LINE__);
			ret = -ETIMEOUT;
			break;
		}
	}

	writel(host, mask, DWMCI_RINTSTS);

	return ret;
}

static int dw_set_transfer_mode(struct mmc_host *host, struct mmc_data *data) 
{
	unsigned long mode;

	mode = DWMCI_CMD_DATA_EXP;
	if (data->flags & MMC_DATA_WRITE)
		mode |= DWMCI_CMD_RW;

	return mode;
}

int dw_send_cmd(struct mmc_host *host, struct mmc_cmd *cmd, struct mmc_data *data)
{
    int ret = 0, flags = 0, i;
	uint32_t timeout = 500;
	uint32_t retry = 100000;
	uint32_t mask, ctrl;
	uint64_t start = get_timer(0);
//  struct bounce_buffer bbstate;

	while (readl(host, DWMCI_STATUS) & DWMCI_BUSY) {
		if (get_timer(start) > timeout) {
			debug("[DWMMC] %s.%d: Timeout on data busy\n", __func__, __LINE__);
			return -ETIMEOUT;
		}
	}

    writel(host, DWMCI_INTMSK_ALL, DWMCI_RINTSTS);

// usig fifo mode

	if (data) {
		if (host->fifo_mode) {
			writel(host, data->blocksize, DWMCI_BLKSIZ);
			writel(host, data->blocksize * data->blocks, DWMCI_BYTCNT);
			dw_wait_reset(host, DWMCI_CTRL_FIFO_RESET);
		} else {
			panic("MMC not fifo");
/*			
			if (data->flags == MMC_DATA_READ) {
				ret = bounce_buffer_start(&bbstate,
						(void*)data->dest,
						data->blocksize *
						data->blocks, GEN_BB_WRITE);
			} else {
				ret = bounce_buffer_start(&bbstate,
						(void*)data->src,
						data->blocksize *
						data->blocks, GEN_BB_READ);
			}

			if (ret)
				return ret;

			dwmci_prepare_data(host, data, cur_idmac,
					   bbstate.bounce_buffer);
*/
		}
	}

	writel(host, cmd->cmdarg, DWMCI_CMDARG);

	if (data)
		flags = dw_set_transfer_mode(host, data);

	if ((cmd->resp_type & MMC_RSP_136) && (cmd->resp_type & MMC_RSP_BUSY))
		return -1;

	if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION)
		flags |= DWMCI_CMD_ABORT_STOP;
	else
		flags |= DWMCI_CMD_PRV_DAT_WAIT;

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		flags |= DWMCI_CMD_RESP_EXP;
		if (cmd->resp_type & MMC_RSP_136)
			flags |= DWMCI_CMD_RESP_LENGTH;
	}

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= DWMCI_CMD_CHECK_CRC;

	if (__test_and_clear_bit_1(DW_MMC_CARD_NEED_INIT, &host->flags))
		flags |= DWMCI_CMD_SEND_INIT;

	flags |= (cmd->cmdidx | DWMCI_CMD_START | DWMCI_CMD_USE_HOLD_REG);

//	debug("[MMC] Sending CMD%d\n",cmd->cmdidx);

	writel(host, flags, DWMCI_CMD);

	for (i = 0; i < retry; i++) {
		mask = readl(host, DWMCI_RINTSTS);
		if (mask & DWMCI_INTMSK_CDONE) {
			if (!data)
				writel(host, mask, DWMCI_RINTSTS);
			break;
		}
	}

	if (i == retry) {
		debug("[DWMMC] %s.%d: Timeout.\n", __func__, __LINE__);
		return -ETIMEOUT;
	}

	if (mask & DWMCI_INTMSK_RTO) {
		/*
		 * Timeout here is not necessarily fatal. (e)MMC cards
		 * will splat here when they receive CMD55 as they do
		 * not support this command and that is exactly the way
		 * to tell them apart from SD cards. Thus, this output
		 * below shall be debug(). eMMC cards also do not favor
		 * CMD8, please keep that in mind.
		 */
		debug("[DWMMC] %s.%d: Response Timeout.\n", __func__, __LINE__);
		return -ETIMEOUT;
	} else if (mask & DWMCI_INTMSK_RE) {
		debug("[DWMMC] %s.%d: Response Error.\n", __func__, __LINE__);
		return -EIO;
	} else if ((cmd->resp_type & MMC_RSP_CRC) &&
		   (mask & DWMCI_INTMSK_RCRC)) {
		debug("[MMC] %s.%d: Response CRC Error.\n", __func__, __LINE__);
		return -EIO;
	}


	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			cmd->response[0] = readl(host, DWMCI_RESP3);
			cmd->response[1] = readl(host, DWMCI_RESP2);
			cmd->response[2] = readl(host, DWMCI_RESP1);
			cmd->response[3] = readl(host, DWMCI_RESP0);
		} else {
			cmd->response[0] = readl(host, DWMCI_RESP0);
		}
	}

	if (data) {
		ret = dw_data_transfer(host, data);

		/* only dma mode need it */
// 		if (!host->fifo_mode) {
// 			if (data->flags == MMC_DATA_READ)
// 				mask = DWMCI_IDINTEN_RI;
// 			else
// 				mask = DWMCI_IDINTEN_TI;
// 			ret = wait_for_bit_le32(host->ioaddr + DWMCI_IDSTS,
// 						mask, true, 1000, false);
// 			if (ret)
// 				printf("[MMC] %s: DWMCI_IDINTEN mask 0x%x timeout.\n",
// 				      __func__, mask);
// 			/* clear interrupts */
// 			writel(host, DWMCI_IDSTS, DWMCI_IDINTEN_MASK);

// 			ctrl = readl(host, DWMCI_CTRL);
// 			ctrl &= ~(DWMCI_DMA_EN);
// 			writel(host, DWMCI_CTRL, ctrl);
// //			bounce_buffer_stop(&bbstate);
// 		}
	}

	udelay(100);

    return ret;
}


static int dw_setup_bus(struct mmc_host *host, uint32_t freq)
{
	uint32_t div, status;
	int timeout = 10000;
	unsigned long sclk;

	if ((freq == host->cur_clock) || (freq == 0))
		return 0;
    if (host->bus_hz)
		sclk = host->bus_hz;
	else {
		debug("[DWMMC] %s.%d: Didn't get source clock value.\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (sclk == freq)
		div = 0;	/* bypass mode */
	else
		div = DIV_ROUND_UP(sclk, 2 * freq);

	writel(host, 0, DWMCI_CLKENA);
	writel(host, 0, DWMCI_CLKSRC);

	writel(host, div, DWMCI_CLKDIV);
	writel(host, DWMCI_CMD_PRV_DAT_WAIT |
			DWMCI_CMD_UPD_CLK | DWMCI_CMD_START, DWMCI_CMD);

	do {
		status = readl(host, DWMCI_CMD);
		if (timeout-- < 0) {
			debug("[DWMMC] %s.%d: Timeout!\n", __func__, __LINE__);
			return -ETIMEOUT;
		}
	} while (status & DWMCI_CMD_START);

	writel(host, DWMCI_CLKEN_ENABLE |
			DWMCI_CLKEN_LOW_PWR, DWMCI_CLKENA);

	writel(host, DWMCI_CMD_PRV_DAT_WAIT |
			DWMCI_CMD_UPD_CLK | DWMCI_CMD_START, DWMCI_CMD);

	timeout = 10000;
	do {
		status = readl(host, DWMCI_CMD);
		if (timeout-- < 0) {
			debug("[DWMMC] %s.%d: Timeout!\n", __func__, __LINE__);
			return -ETIMEOUT;
		}
	} while (status & DWMCI_CMD_START);

	host->cur_clock = freq;

	return SUCCESS;
}

int dw_set_ios(struct mmc_host *host)
{
	uint32_t ctype, regs;

	debug("[DWMMC] Buswidth = %d, clock: %d\n", host->buswidth, host->clock);

	dw_setup_bus(host, host->clock);
	switch (host->buswidth) {
	case 8:
		ctype = DWMCI_CTYPE_8BIT;
		break;
	case 4:
		ctype = DWMCI_CTYPE_4BIT;
		break;
	default:
		ctype = DWMCI_CTYPE_1BIT;
		break;
	}

	writel(host, ctype, DWMCI_CTYPE);

	regs = readl(host, DWMCI_UHS_REG);
	if (host->card->ddr_mode)
		regs |= DWMCI_DDR_MODE;
	else
		regs &= ~DWMCI_DDR_MODE;

	writel(host, regs, DWMCI_UHS_REG);


	// if (host->clksel) {
	// 	int ret;

	// 	ret = host->clksel(host);
	// 	if (ret)
	// 		return ret;
	// }
/*
#if CONFIG_IS_ENABLED(DM_REGULATOR)
	if (mmc->vqmmc_supply) {
		int ret;

		if (mmc->signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			regulator_set_value(mmc->vqmmc_supply, 1800000);
		else
			regulator_set_value(mmc->vqmmc_supply, 3300000);

		ret = regulator_set_enable_if_allowed(mmc->vqmmc_supply, true);
		if (ret)
			return ret;
	}
#endif
*/
	return SUCCESS;
}

int dw_getcd(struct mmc_host *host) {
    debug("[DWMMC] Card Detect Register 0x%X\n", readl(host, DWMCI_CDETECT));
	return !(readl(host, DWMCI_CDETECT) & 1);
}

int dw_mmc_init(struct mmc_host *host) 
{
    debug("[DWMMC] Hardware Configuration Register 0x%X\n",readl(host, DWMCI_HCON)); 

    writel(host, 1, DWMCI_PWREN);

    if (dw_wait_reset(host, DWMCI_RESET_ALL) < 0) {
		debug("%s[%d] Fail-reset!!\n", __func__, __LINE__);
		return -EIO;
	}

	host->flags = 1 << DW_MMC_CARD_NEED_INIT;

	/* Enumerate at 400KHz */
	dw_setup_bus(host, 4000000);

	writel(host, 0xFFFFFFFF, DWMCI_RINTSTS);
	writel(host, 0, DWMCI_INTMASK);

	writel(host, 0xFFFFFFFF, DWMCI_TMOUT);

	writel(host, 0, DWMCI_IDINTEN);
	writel(host, 1, DWMCI_BMOD);

	if (!host->fifoth_val) {
		uint32_t fifo_size;

		fifo_size = readl(host, DWMCI_FIFOTH);
		fifo_size = ((fifo_size & RX_WMARK_MASK) >> RX_WMARK_SHIFT) + 1;
		host->fifoth_val = MSIZE(0x2) | RX_WMARK(fifo_size / 2 - 1) |
				TX_WMARK(fifo_size / 2);
	}
    debug("[DWMMC] FIFOTH VAL 0x%X\n", host->fifoth_val);
	writel(host, host->fifoth_val, DWMCI_FIFOTH);

	writel(host, 0, DWMCI_CLKENA);
	writel(host, 0, DWMCI_CLKSRC);

	if (!host->fifo_mode)
		writel(host, DWMCI_IDINTEN_MASK, DWMCI_IDINTEN);

	

    return SUCCESS;
}

struct mmc_ops dw_mmc_ops = {
	.send_cmd	= dw_send_cmd,
	.set_ios	= dw_set_ios,
	.init		= dw_mmc_init,
	.getcd		= dw_getcd,

};

kerrno_t board_init_host(struct mmc_host *host)
{
    int err;

    void *base = mmap(NULL, 4096, MAP_MEMIO | MAP_READ | MAP_WRITE, (void*)SDIO1_BASE);
    if(base == 0)
        panic("[DWMMC] init error");

    host->ioaddr = base;

    host->buswidth	= 4;
    host->max_clk = 50000000;
    host->caps = MMC_CAP(MMC_LEGACY) | MMC_MODE_1BIT  | MMC_CAP_CD_ACTIVE_HIGH | MMC_CAP(SD_HS) | MMC_MODE_4BIT;

	// bus frequency of mmc sdio on JH7110
    host->bus_hz = 50000000;

	uint32_t fifo_depth = 32;
	host->fifoth_val = MSIZE(0x2) | RX_WMARK(fifo_depth / 2 - 1) | TX_WMARK(fifo_depth / 2);
	host->fifo_mode = 1;


    struct mmc_config *cfg = host->cfg;
    
	cfg->f_min = 400000;
	cfg->f_max = 50000000;
	cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
	cfg->host_caps = MMC_CAP(MMC_LEGACY) | MMC_MODE_1BIT | MMC_MODE_4BIT | MMC_CAP(SD_HS);

	if (host->buswidth == 8) {
		cfg->host_caps |= MMC_MODE_8BIT;
		cfg->host_caps &= ~MMC_MODE_4BIT;
	} else {
		cfg->host_caps |= MMC_MODE_4BIT;
		cfg->host_caps &= ~MMC_MODE_8BIT;
	}
	cfg->host_caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz | MMC_MODE_HS200;

	cfg->b_max = 1024;

    host->ops = &dw_mmc_ops;

	/* Setup dsr related values */
	host->card->dsr_imp = 0;
	host->card->dsr = 0xffffffff;

    err = dw_mmc_init(host);
    if(err < 0) return err;

    return SUCCESS;
}
