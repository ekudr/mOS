#include <string.h>
#include <mosstd.h>
#include <libsys/timer.h>
#include "mmc.h"

#define __swab32(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
		(((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
		(((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
		(((__u32)(x) & (__u32)0xff000000UL) >> 24) ))

#define __be32_to_cpu(x) __swab32((__u32)(x))

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct mmc_host sd_mmc;
struct mmc_config mmc_cfg;
struct mmc_card card;



kerrno_t mmc_init_host()
{
    kerrno_t err;
    memset(&sd_mmc, 0, sizeof(sd_mmc));
    memset(&mmc_cfg, 0, sizeof(mmc_cfg));
    memset(&card, 0, sizeof(card));

    sd_mmc.cfg = &mmc_cfg;
    sd_mmc.card = &card;


    err = board_init_host(&sd_mmc);
    if(err < 0) return err;

    return SUCCESS;
}

void mmmc_trace_before_send(struct mmc_host *host, struct mmc_cmd *cmd) 
{
	debug("[MMC] CMD_SEND:%d\n", cmd->cmdidx);
	debug("[MMC] \t\tARG\t\t\t 0x%08x\n", cmd->cmdarg);
}

void mmmc_trace_after_send(struct mmc_host *host, struct mmc_cmd *cmd, int ret) 
{
	int i;
	uint8_t *ptr;

	if (ret) {
		debug("[MMC] \t\tRET\t\t\t %d\n", ret);
	} else {
		switch (cmd->resp_type) {
		case MMC_RSP_NONE:
			debug("[MMC] \t\tMMC_RSP_NONE\n");
			break;
		case MMC_RSP_R1:
			debug("[MMC] \t\tMMC_RSP_R1,5,6,7 \t 0x%08x \n",
				cmd->response[0]);
			break;
		case MMC_RSP_R1b:
			debug("[MMC] \t\tMMC_RSP_R1b\t\t 0x%08x \n",
				cmd->response[0]);
			break;
		case MMC_RSP_R2:
			debug("[MMC] \t\tMMC_RSP_R2\t\t 0x%08x \n",
				cmd->response[0]);
			debug("[MMC] \t\t          \t\t 0x%08x \n",
				cmd->response[1]);
			debug("[MMC] \t\t          \t\t 0x%08x \n",
				cmd->response[2]);
			debug("[MMC] \t\t          \t\t 0x%08x \n",
				cmd->response[3]);
			debug("[MMC] \n");
			debug("[MMC] \t\t\t\t\tDUMPING DATA\n");
			for (i = 0; i < 4; i++) {
				int j;
				debug("[MMC] \t\t\t\t\t%03d - ", i*4);
				ptr = (uint8_t *)&cmd->response[i];
				ptr += 3;
				for (j = 0; j < 4; j++)
					debug("%02x ", *ptr--);
				debug("\n");
			}
			break;
		case MMC_RSP_R3:
			debug("[MMC] \t\tMMC_RSP_R3,4\t\t 0x%08x \n",
				cmd->response[0]);
			break;
		default:
			debug("[MMC] \t\tERROR MMC rsp not supported\n");
			break;
		}
	}
}

void mmc_trace_state(struct mmc_host *host, struct mmc_cmd *cmd) 
{
	int status;

	status = (cmd->response[0] & MMC_STATUS_CURR_STATE) >> 9;
	debug("[MMC] CURR STATE:%d\n", status);
}

const char *mmc_mode_name(enum bus_mode mode) 
{
	static const char *const names[] = {
	      [MMC_LEGACY]	= "MMC legacy",
	      [MMC_HS]		= "MMC High Speed (26MHz)",
	      [SD_HS]		= "SD High Speed (50MHz)",
	      [UHS_SDR12]	= "UHS SDR12 (25MHz)",
	      [UHS_SDR25]	= "UHS SDR25 (50MHz)",
	      [UHS_SDR50]	= "UHS SDR50 (100MHz)",
	      [UHS_SDR104]	= "UHS SDR104 (208MHz)",
	      [UHS_DDR50]	= "UHS DDR50 (50MHz)",
	      [MMC_HS_52]	= "MMC High Speed (52MHz)",
	      [MMC_DDR_52]	= "MMC DDR52 (52MHz)",
	      [MMC_HS_200]	= "HS200 (200MHz)",
	      [MMC_HS_400]	= "HS400 (200MHz)",
	      [MMC_HS_400_ES]	= "HS400ES (200MHz)",
	};

	if (mode >= MMC_MODES_END)
		return "Unknown mode";
	else
		return names[mode];
}

static uint32_t mmc_mode2freq(struct mmc_host *host, enum bus_mode mode) 
{
	static const int freqs[] = {
	      [MMC_LEGACY]	= 25000000,
	      [MMC_HS]		= 26000000,
	      [SD_HS]		= 50000000,
	      [MMC_HS_52]	= 52000000,
	      [MMC_DDR_52]	= 52000000,
	      [UHS_SDR12]	= 25000000,
	      [UHS_SDR25]	= 50000000,
	      [UHS_SDR50]	= 100000000,
	      [UHS_DDR50]	= 50000000,
	      [UHS_SDR104]	= 208000000,
	      [MMC_HS_200]	= 200000000,
	      [MMC_HS_400]	= 200000000,
	      [MMC_HS_400_ES]	= 200000000,
	};

	if (mode == MMC_LEGACY)
		return host->card->legacy_speed;
	else if (mode >= MMC_MODES_END)
		return 0;
	else
		return freqs[mode];
}

/*
 * helper function to display the capabilities in a human
 * friendly manner. The capabilities include bus width and
 * supported modes.
 */
void mmc_dump_capabilities(const char *text, uint32_t caps)
{
	enum bus_mode mode;

	debug("[MMC] %s: widths [", text);
	if (caps & MMC_MODE_8BIT)
		debug("8, ");
	if (caps & MMC_MODE_4BIT)
		debug("4, ");
	if (caps & MMC_MODE_1BIT)
		debug("1, ");
	debug("\b\b] modes [");
	for (mode = MMC_LEGACY; mode < MMC_MODES_END; mode++)
		if (MMC_CAP(mode) & caps)
			debug("%s, ", mmc_mode_name(mode));
	debug("\b\b]\n");
}

/* frequency bases */
/* divided by 10 to be nice to platforms without floating point */
static const int fbase[] = {
	10000,
	100000,
	1000000,
	10000000,
};

/* Multiplier values for TRAN_SPEED.  Multiplied by 10 to be nice
 * to platforms without floating point.
 */
static const uint8_t multipliers[] = {
	0,	/* reserved */
	10,
	12,
	13,
	15,
	20,
	25,
	30,
	35,
	40,
	45,
	50,
	55,
	60,
	70,
	80,
};

static inline int bus_width(uint32_t cap)
{
	if (cap == MMC_MODE_8BIT)
		return 8;
	if (cap == MMC_MODE_4BIT)
		return 4;
	if (cap == MMC_MODE_1BIT)
		return 1;
	debug("[MMC] invalid bus witdh capability 0x%x\n", cap);
	return 0;
}

int mmc_send_cmd(struct mmc_host *host, struct mmc_cmd *cmd, struct mmc_data *data)
{
	int ret;

//	mmmc_trace_before_send(host, cmd);
	ret = host->ops->send_cmd(host, cmd, data);
//	mmmc_trace_after_send(host, cmd, ret);

	return ret;
}

static int 
mmc_send_cmd_retry(struct mmc_host *host, struct mmc_cmd *cmd,
			      struct mmc_data *data, uint32_t retries) 
{
	int ret;

	do {
		ret = mmc_send_cmd(host, cmd, data);
	} while (ret && retries--);

	return ret;
}


static int 
mmc_send_cmd_quirks(struct mmc_host *host, struct mmc_cmd *cmd,
			       struct mmc_data *data, uint32_t quirk, uint32_t retries) 
{
//	if (CONFIG_IS_ENABLED(MMC_QUIRKS) && host->quirks & quirk)
//		return mmc_send_cmd_retry(mmc, cmd, data, retries);
//	else
		return mmc_send_cmd(host, cmd, data);
}


static int sd_switch(struct mmc_host *host, int mode, int group, uint8_t value, uint8_t *resp)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	/* Switch the frequency */
	cmd.cmdidx = SD_CMD_SWITCH_FUNC;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = (mode << 31) | 0xffffff;
	cmd.cmdarg &= ~(0xf << (group * 4));
	cmd.cmdarg |= value << (group * 4);

	data.dest = (char *)resp;
	data.blocksize = 64;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	return mmc_send_cmd(host, &cmd, &data);
}

static int sd_get_capabilities(struct mmc_host *host)
{
	int err;
	struct mmc_cmd cmd;
    //rewrite allocation
    char __scr[16] __ALIGN(32); // __be32[2]
    __ALIGN(32) uint32_t *scr =  (uint32_t*)__scr; 
	char __switch_status[128] __ALIGN(32);   // __be32[16]
    __ALIGN(32) uint32_t *switch_status = (uint32_t*)__switch_status;
	struct mmc_data data;
	int timeout;
/*    
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
	u32 sd3_bus_mode;
#endif
*/
	host->card->caps = MMC_MODE_1BIT | MMC_CAP(MMC_LEGACY);

	// if (mmc_host_is_spi(mmc))
	// 	return 0;

	/* Read the SCR to find out if this card supports higher speeds */
	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = host->card->rca << 16;

	err = mmc_send_cmd(host, &cmd, NULL);

	if (err < 0)
		return err;

	cmd.cmdidx = SD_CMD_APP_SEND_SCR;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	data.dest = (char *)scr;
	data.blocksize = 8;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd_retry(host, &cmd, &data, 3);

	if (err < 0)
		return err;

	host->card->scr[0] = __be32_to_cpu(scr[0]);
	host->card->scr[1] = __be32_to_cpu(scr[1]);

	switch ((host->card->scr[0] >> 24) & 0xf) {
	case 0:
		host->version = SD_VERSION_1_0;
		break;
	case 1:
		host->version = SD_VERSION_1_10;
		break;
	case 2:
		host->version = SD_VERSION_2;
		if ((host->card->scr[0] >> 15) & 0x1)
			host->version = SD_VERSION_3;
		break;
	default:
		host->version = SD_VERSION_1_0;
		break;
	}
    debug("[MMC] card caps mmc version %X\n", host->version);

	if (host->card->scr[0] & SD_DATA_4BIT)
		host->card->caps |= MMC_MODE_4BIT;

	/* Version 1.0 doesn't support switching */
	if (host->version == SD_VERSION_1_0)
		return 0;

	timeout = 4;
	while (timeout--) {
		err = sd_switch(host, SD_SWITCH_CHECK, 0, 1,
				(uint8_t *)switch_status);

		if (err < 0)
			return err;

		/* The high-speed function is busy.  Try again */
		if (!(__be32_to_cpu(switch_status[7]) & SD_HIGHSPEED_BUSY))
			break;
	}

	/* If high-speed isn't supported, we return */
	if (__be32_to_cpu(switch_status[3]) & SD_HIGHSPEED_SUPPORTED)
		host->card->caps |= MMC_CAP(SD_HS);
/*
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
	// Version before 3.0 don't support UHS modes 
	if (mmc->version < SD_VERSION_3)
		return 0;

	sd3_bus_mode = __be32_to_cpu(switch_status[3]) >> 16 & 0x1f;
	if (sd3_bus_mode & SD_MODE_UHS_SDR104)
		mmc->card_caps |= MMC_CAP(UHS_SDR104);
	if (sd3_bus_mode & SD_MODE_UHS_SDR50)
		mmc->card_caps |= MMC_CAP(UHS_SDR50);
	if (sd3_bus_mode & SD_MODE_UHS_SDR25)
		mmc->card_caps |= MMC_CAP(UHS_SDR25);
	if (sd3_bus_mode & SD_MODE_UHS_SDR12)
		mmc->card_caps |= MMC_CAP(UHS_SDR12);
	if (sd3_bus_mode & SD_MODE_UHS_DDR50)
		mmc->card_caps |= MMC_CAP(UHS_DDR50);
#endif
*/
	return 0;
}

static int 
sd_set_card_speed(struct mmc_host *host, enum bus_mode mode) 
{
	int err;

	char __switch_status[128] __ALIGN(32);   // __be32[16]
    __ALIGN(32) uint32_t *switch_status = (uint32_t*)__switch_status;

	int speed;

	/* SD version 1.00 and 1.01 does not support CMD 6 */
	if (host->version == SD_VERSION_1_0)
		return SUCCESS;

	switch (mode) {
	case MMC_LEGACY:
		speed = UHS_SDR12_BUS_SPEED;
		break;
	case SD_HS:
		speed = HIGH_SPEED_BUS_SPEED;
		break;
/*       
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
	case UHS_SDR12:
		speed = UHS_SDR12_BUS_SPEED;
		break;
	case UHS_SDR25:
		speed = UHS_SDR25_BUS_SPEED;
		break;
	case UHS_SDR50:
		speed = UHS_SDR50_BUS_SPEED;
		break;
	case UHS_DDR50:
		speed = UHS_DDR50_BUS_SPEED;
		break;
	case UHS_SDR104:
		speed = UHS_SDR104_BUS_SPEED;
		break;
#endif
*/ 
	default:
		return -EINVAL;
	}

	err = sd_switch(host, SD_SWITCH_SWITCH, 0, speed, (uint8_t *)switch_status);
	if (err < 0)
		return err;

	if (((__be32_to_cpu(switch_status[4]) >> 24) & 0xF) != speed)
		return -ENOSUPPORT;

	return SUCCESS;
}

static int 
sd_select_bus_width(struct mmc_host *host, int w) 
{
	int err;
	struct mmc_cmd cmd;

	if ((w != 4) && (w != 1))
		return -EINVAL;

	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = host->card->rca << 16;

	err = mmc_send_cmd(host, &cmd, NULL);
	if (err < 0)
		return err;

	cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;
	cmd.resp_type = MMC_RSP_R1;
	if (w == 4)
		cmd.cmdarg = 2;
	else if (w == 1)
		cmd.cmdarg = 0;
	err = mmc_send_cmd(host, &cmd, NULL);
	if (err < 0)
		return err;

	return SUCCESS;
}

static int 
mmc_read_blocks(struct mmc_host *host, void *dst, uint64_t start, uint64_t blkcnt) 
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	if (blkcnt > 1)
		cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;

	if (host->card->high_capacity)
		cmd.cmdarg = start;
	else
		cmd.cmdarg = start * host->card->read_bl_len;

	cmd.resp_type = MMC_RSP_R1;

	data.dest = dst;
	data.blocks = blkcnt;
	data.blocksize = host->card->read_bl_len;
	data.flags = MMC_DATA_READ;
    int err = mmc_send_cmd(host, &cmd, &data);
	if (err < 0) {
        debug("[MMC] %s.%d read err %d\n", __func__,__LINE__, err);
        return 0;
    }
		

	if (blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		if (mmc_send_cmd(host, &cmd, NULL) < 0) {
			debug("mmc fail to send stop cmd\n");
			return 0;
		}
	}
    
	return blkcnt;
}

static int mmc_set_ios(struct mmc_host *host) 
{
	int ret = 0;

	if (host->ops->set_ios)
		ret = host->ops->set_ios(host);

	return ret;
}

static int mmc_select_mode(struct mmc_host *host, enum bus_mode mode) 
{
	host->card->selected_mode = mode;
	host->card->tran_speed = mmc_mode2freq(host, mode);
	host->card->ddr_mode = mmc_is_mode_ddr(mode);
	debug("[MMC] selecting mode %s (freq : %d MHz)\n", mmc_mode_name(mode),
		 host->card->tran_speed / 1000000);
	return 0;
}

int 
mmc_set_clock(struct mmc_host *host, uint32_t clock, bool disable) {
	if (!disable) {
		if (clock > host->cfg->f_max)
			clock = host->cfg->f_max;

		if (clock < host->cfg->f_min)
			clock = host->cfg->f_min;
	}

	host->clock = clock;
	host->clk_disable = disable;

	debug("[MMC] clock is %s (%dHz)\n", disable ? "disabled" : "enabled", host->clock);

	return mmc_set_ios(host);
}

static int mmc_set_bus_width(struct mmc_host *host, uint32_t width) 
{
	host->buswidth = width;
	return mmc_set_ios(host);
}

struct mode_width_tuning {
	enum bus_mode mode;
	uint32_t widths;
#ifdef MMC_SUPPORTS_TUNING
	uint tuning;
#endif
};

static const struct mode_width_tuning sd_modes_by_pref[] = {
/*   
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
#ifdef MMC_SUPPORTS_TUNING
	{
		.mode = UHS_SDR104,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
		.tuning = MMC_CMD_SEND_TUNING_BLOCK
	},
#endif
	{
		.mode = UHS_SDR50,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	},
	{
		.mode = UHS_DDR50,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	},
	{
		.mode = UHS_SDR25,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	},
#endif
*/ 
	{
		.mode = SD_HS,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	},
/*
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
	{
		.mode = UHS_SDR12,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	},
#endif
*/
	{
		.mode = MMC_LEGACY,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	}
};


#define for_each_sd_mode_by_pref(caps, mwt)     \
	for (mwt = sd_modes_by_pref;        \
	     mwt < sd_modes_by_pref + ARRAY_SIZE(sd_modes_by_pref);     \
	     mwt++)         \
		if (caps & MMC_CAP(mwt->mode))



static int
sd_select_mode_and_width(struct mmc_host *host, uint32_t card_caps) 
{
	kerrno_t err;
	uint32_t widths[] = {MMC_MODE_4BIT, MMC_MODE_1BIT};
	const struct mode_width_tuning *mwt;
   
//#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
//	bool uhs_en = (mmc->ocr & OCR_S18R) ? true : false;
//#else
//	bool uhs_en = false;
//#endif
	uint32_t caps;

//#ifdef DEBUG
	mmc_dump_capabilities("sd card", card_caps);
	mmc_dump_capabilities("host", host->caps); 
    mmc_dump_capabilities("host cfg", host->cfg->host_caps); 
//#endif

// 	if (mmc_host_is_spi(mmc)) {
// 		mmc_set_bus_width(mmc, 1);
// 		mmc_select_mode(mmc, MMC_LEGACY);
// 		mmc_set_clock(mmc, mmc->tran_speed, MMC_CLK_ENABLE);
// /*        
// #if CONFIG_IS_ENABLED(MMC_WRITE)
// 		err = sd_read_ssr(mmc);
// 		if (err)
// 			pr_warn("unable to read ssr\n");
// #endif
// */
// 		return SUCCESS;
// 	}

	/* Restrict card's capabilities by what the host can do */
	caps = card_caps & host->caps; // host->cfg->host_caps

//	if (!uhs_en)
//		caps &= ~UHS_CAPS;

	for_each_sd_mode_by_pref(caps, mwt) {
		uint32_t *w;

		for (w = widths; w < widths + ARRAY_SIZE(widths); w++) {
			if (*w & caps & mwt->widths) {
				debug("[MMC] trying mode %s width %d (at %d MHz)\n",
					 mmc_mode_name(mwt->mode),
					 bus_width(*w),
					 mmc_mode2freq(host, mwt->mode) / 1000000);

				/* configure the bus width (card + host) */
				err = sd_select_bus_width(host, bus_width(*w));
				if (err < 0)
					goto error;
				mmc_set_bus_width(host, bus_width(*w));

				/* configure the bus mode (card) */
				err = sd_set_card_speed(host, mwt->mode);
				if (err < 0)
					goto error;

				/* configure the bus mode (host) */
				mmc_select_mode(host, mwt->mode);
				mmc_set_clock(host, host->card->tran_speed,
						MMC_CLK_ENABLE);

#ifdef MMC_SUPPORTS_TUNING
				/* execute tuning if needed */
				if (mwt->tuning && !mmc_host_is_spi(mmc)) {
					err = mmc_execute_tuning(mmc,
								 mwt->tuning);
					if (err) {
						pr_debug("tuning failed\n");
						goto error;
					}
				}
#endif
/*
#if CONFIG_IS_ENABLED(MMC_WRITE)
				err = sd_read_ssr(mmc);
				if (err)
					pr_warn("unable to read ssr\n");
#endif
*/
				if (!err)
					return SUCCESS;

error:
				/* revert to a safer bus speed */
				mmc_select_mode(host, MMC_LEGACY);
				mmc_set_clock(host, host->card->tran_speed,
						MMC_CLK_ENABLE);
			}
		}
	}

	debug("[MMC] unable to select a mode\n");
	return -ENOSUPPORT;
}


static int mmc_go_idle(struct mmc_host *host)
{
	struct mmc_cmd cmd;
	int err;

	udelay(1000);

	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;

	err = mmc_send_cmd(host, &cmd, NULL);

	udelay(2000);

	return err;
}

/*
 * put the host in the initial state:
 * - turn on Vdd (card power supply)
 * - configure the bus width and clock to minimal values
 */
static void mmc_set_initial_state(struct mmc_host *host) 
{
//	int err;

	/* First try to set 3.3V. If it fails set to 1.8V */
//	err = mmc_set_signal_voltage(mmc, MMC_SIGNAL_VOLTAGE_330);
//	if (err != 0)
//		err = mmc_set_signal_voltage(mmc, MMC_SIGNAL_VOLTAGE_180);
//	if (err != 0)
//		pr_warn("mmc: failed to set signal voltage\n");

	mmc_select_mode(host, MMC_LEGACY);
	mmc_set_bus_width(host, 1);
	mmc_set_clock(host, 0, MMC_CLK_ENABLE);
}

static int 
mmc_power_init(struct mmc_host *host) 
{
    // It should power_up controller
    // I have it on after U-Boot
    // skip for now

	return 0;
}

static kerrno_t mmc_startup(struct mmc_host *host) 
{
	int err/*, i*/;
	uint32_t mult, freq;
	uint64_t cmult, csize;
	struct mmc_cmd cmd;
//	struct blk_desc *bdesc;

// #ifdef CONFIG_MMC_SPI_CRC_ON
// 	if (mmc_host_is_spi(mmc)) { /* enable CRC check for spi */
// 		cmd.cmdidx = MMC_CMD_SPI_CRC_ON_OFF;
// 		cmd.resp_type = MMC_RSP_R1;
// 		cmd.cmdarg = 1;
// 		err = mmc_send_cmd(mmc, &cmd, NULL);
// 		if (err)
// 			return err;
// 	}
// #endif

	/* Put the Card in Identify Mode */

	// cmd.cmdidx = mmc_host_is_spi(mmc) ? MMC_CMD_SEND_CID :
	// 	MMC_CMD_ALL_SEND_CID; /* cmd not supported in spi */
    cmd.cmdidx = MMC_CMD_ALL_SEND_CID;

	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;

	err = mmc_send_cmd_quirks(host, &cmd, NULL, MMC_QUIRK_RETRY_SEND_CID, 4);
	if (err < 0)
		return err;

	memcpy(host->card->cid, cmd.response, 16);

	/*
	 * For MMC cards, set the Relative Address.
	 * For SD cards, get the Relatvie Address.
	 * This also puts the cards into Standby State
	 */

//	if (!mmc_host_is_spi(mmc)) { /* cmd not supported in spi */
		cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
		cmd.cmdarg = host->card->rca << 16;
		cmd.resp_type = MMC_RSP_R6;

		err = mmc_send_cmd(host, &cmd, NULL);

		if (err < 0)
			return err;

		if (IS_SD(host))
			host->card->rca = (cmd.response[0] >> 16) & 0xffff;
//	}

	/* Get the Card-Specific Data */
	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = host->card->rca << 16;

	err = mmc_send_cmd(host, &cmd, NULL);

	if (err < 0)
		return err;

	host->card->csd[0] = cmd.response[0];
	host->card->csd[1] = cmd.response[1];
	host->card->csd[2] = cmd.response[2];
	host->card->csd[3] = cmd.response[3];

	if (host->version == MMC_VERSION_UNKNOWN) {
		int version = (cmd.response[0] >> 26) & 0xf;

		switch (version) {
		case 0:
			host->version = MMC_VERSION_1_2;
			break;
		case 1:
			host->version = MMC_VERSION_1_4;
			break;
		case 2:
			host->version = MMC_VERSION_2_2;
			break;
		case 3:
			host->version = MMC_VERSION_3;
			break;
		case 4:
			host->version = MMC_VERSION_4;
			break;
		default:
			host->version = MMC_VERSION_1_2;
			break;
		}
	}

    debug("[MMC] mmc version %X\n", host->version);

	/* divide frequency by 10, since the mults are 10x bigger */
	freq = fbase[(cmd.response[0] & 0x7)];
	mult = multipliers[((cmd.response[0] >> 3) & 0xf)];

	host->card->legacy_speed = freq * mult;
    debug("[MMC] legacy speed %X\n", host->card->legacy_speed);
	mmc_select_mode(host, MMC_LEGACY);

	host->card->dsr_imp = ((cmd.response[1] >> 12) & 0x1);
	host->card->read_bl_len = 1 << ((cmd.response[1] >> 16) & 0xf);

    debug("[MMC] dsr_imp %X\n", host->card->dsr_imp);
    debug("[MMC] read_bl_len %X\n", host->card->read_bl_len);
/*    
#if CONFIG_IS_ENABLED(MMC_WRITE)

	if (IS_SD(mmc))
		mmc->write_bl_len = mmc->read_bl_len;
	else
		mmc->write_bl_len = 1 << ((cmd.response[3] >> 22) & 0xf);
#endif
*/
	if (host->card->high_capacity) {
		csize = (host->card->csd[1] & 0x3f) << 16
			| (host->card->csd[2] & 0xffff0000) >> 16;
		cmult = 8;
	} else {
		csize = (host->card->csd[1] & 0x3ff) << 2
			| (host->card->csd[2] & 0xc0000000) >> 30;
		cmult = (host->card->csd[2] & 0x00038000) >> 15;
	}

	host->card->capacity_user = (csize + 1) << (cmult + 2);
	host->card->capacity_user *= host->card->read_bl_len;
	// mmc->capacity_boot = 0;
	// mmc->capacity_rpmb = 0;
	// for (i = 0; i < 4; i++)
	// 	mmc->capacity_gp[i] = 0;
    debug("[MMC] Card capacity 0x%lX\n", host->card->capacity_user);
	if (host->card->read_bl_len > MMC_MAX_BLOCK_LEN)
		host->card->read_bl_len = MMC_MAX_BLOCK_LEN;
/*
#if CONFIG_IS_ENABLED(MMC_WRITE)
	if (mmc->write_bl_len > MMC_MAX_BLOCK_LEN)
		mmc->write_bl_len = MMC_MAX_BLOCK_LEN;
#endif
*/
	if ((host->card->dsr_imp) && (0xffffffff != host->card->dsr)) {
		cmd.cmdidx = MMC_CMD_SET_DSR;
		cmd.cmdarg = (host->card->dsr & 0xffff) << 16;
		cmd.resp_type = MMC_RSP_NONE;
		if (mmc_send_cmd(host, &cmd, NULL) < 0)
			debug("[MMC] SET_DSR failed\n");
	}

	/* Select the card, and put it into Transfer Mode */
//	if (!mmc_host_is_spi(mmc)) { /* cmd not supported in spi */
		cmd.cmdidx = MMC_CMD_SELECT_CARD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = host->card->rca << 16;
		err = mmc_send_cmd(host, &cmd, NULL);

		if (err < 0)
			return err;
//	}

	/*
	 * For SD, its erase group is always one sector
	 */
/*   
#if CONFIG_IS_ENABLED(MMC_WRITE)
	mmc->erase_grp_size = 1;
#endif
*/ 
	//  mmc->part_config = MMCPART_NOAVAILABLE;

//	err = mmc_startup_v4(mmc);
//	if (err)
//		return err;
        
/*
	err = mmc_set_capacity(mmc, mmc_get_blk_desc(mmc)->hwpart);
	if (err)
		return err;
*/
#if 0 //CONFIG_IS_ENABLED(MMC_TINY)
	mmc_set_clock(host, host->card->legacy_speed, false);
	mmc_select_mode(host, MMC_LEGACY);
	mmc_set_bus_width(host, 1);
#else
	if (IS_SD(host)) {
		err = sd_get_capabilities(host);
		if (err < 0)
			return err;

		err = sd_select_mode_and_width(host, host->card->caps);
	}/* else {
		err = mmc_get_capabilities(mmc);
		if (err)
			return err;
		err = mmc_select_mode_and_width(mmc, mmc->card_caps);
	}*/
#endif
	if (err < 0)
		return err;

	host->card->best_mode = host->card->selected_mode;
    debug("[MMC] best mode %d\n", host->card->best_mode);
	/* Fix the block length for DDR mode */
	// if (mmc->ddr_mode) {
	// 	mmc->read_bl_len = MMC_MAX_BLOCK_LEN;
/*        
#if CONFIG_IS_ENABLED(MMC_WRITE)
		mmc->write_bl_len = MMC_MAX_BLOCK_LEN;
#endif
*/
	//}

	/* fill in device description */
/*    
	bdesc = mmc_get_blk_desc(mmc);
	bdesc->lun = 0;
	bdesc->hwpart = 0;
	bdesc->type = 0;
	bdesc->blksz = mmc->read_bl_len;
	bdesc->log2blksz = LOG2(bdesc->blksz);
	bdesc->lba = lldiv(mmc->capacity, mmc->read_bl_len);
*/
/*   
#if !defined(CONFIG_SPL_BUILD) || \
		(defined(CONFIG_SPL_LIBCOMMON_SUPPORT) && \
		!CONFIG_IS_ENABLED(USE_TINY_PRINTF))
	sprintf(bdesc->vendor, "Man %06x Snr %04x%04x",
		mmc->cid[0] >> 24, (mmc->cid[2] & 0xffff),
		(mmc->cid[3] >> 16) & 0xffff);
	sprintf(bdesc->product, "%c%c%c%c%c%c", mmc->cid[0] & 0xff,
		(mmc->cid[1] >> 24), (mmc->cid[1] >> 16) & 0xff,
		(mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff,
		(mmc->cid[2] >> 24) & 0xff);
	sprintf(bdesc->revision, "%d.%d", (mmc->cid[2] >> 20) & 0xf,
		(mmc->cid[2] >> 16) & 0xf);
#else
	bdesc->vendor[0] = 0;
	bdesc->product[0] = 0;
	bdesc->revision[0] = 0;
#endif
*/ 
/*
#if !defined(CONFIG_DM_MMC) && (!defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_LIBDISK_SUPPORT))
	part_init(bdesc);
#endif
*/
	return SUCCESS;
}


static kerrno_t sd_send_op_cond(struct mmc_host *host, bool uhs_en) 
{
	int timeout = 1000;
	int err;
	struct mmc_cmd cmd;

	while (1) {
		cmd.cmdidx = MMC_CMD_APP_CMD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;

		err = mmc_send_cmd(host, &cmd, NULL);

		if (err < 0) return err;

		cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;

		/*
		 * Most cards do not answer if some reserved bits
		 * in the ocr are set. However, Some controller
		 * can set bit 7 (reserved for low voltages), but
		 * how to manage low voltages SD card is not yet
		 * specified.
		 */
		cmd.cmdarg = (host->cfg->voltages & 0xff8000);

		if (host->version == SD_VERSION_2)
			cmd.cmdarg |= OCR_HCS;

		if (uhs_en)
			cmd.cmdarg |= OCR_S18R;

		err = mmc_send_cmd(host, &cmd, NULL);

		if (err < 0) return err;

		if (cmd.response[0] & OCR_BUSY)
			break;

		if (timeout-- <= 0)
			return -ETIMEOUT;

		udelay(1000);
	}

	if (host->version != SD_VERSION_2)
		host->version = SD_VERSION_1_0;

	// if (mmc_host_is_spi(mmc)) { /* read OCR for spi */
	// 	cmd.cmdidx = MMC_CMD_SPI_READ_OCR;
	// 	cmd.resp_type = MMC_RSP_R3;
	// 	cmd.cmdarg = 0;

	// 	err = mmc_send_cmd(host, &cmd, NULL);

	// 	if (err)
	// 		return err;
	// }

	host->card->ocr = cmd.response[0];

#if 0 // CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
	if (uhs_en && (cmd.response[0] & 0x41000000)
	    == 0x41000000) {
		err = mmc_switch_voltage(host, MMC_SIGNAL_VOLTAGE_180);
		if (err < 0)
			return err;
	}
#endif

	host->card->high_capacity = ((host->card->ocr & OCR_HCS) == OCR_HCS);
	host->card->rca = 0;

    debug("[MMC] %s.%d ocr 0x%X\n", __func__, __LINE__, host->card->ocr);

	return SUCCESS;
}

static kerrno_t mmc_send_if_cond(struct mmc_host *host) 
{
	struct mmc_cmd cmd;
	int err;

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	/* We set the bit if the host supports voltages between 2.7 and 3.6 V */
	cmd.cmdarg = ((host->cfg->voltages & 0xff8000) != 0) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;

	err = mmc_send_cmd(host, &cmd, NULL);

	if (err < 0)
		return err;

	if ((cmd.response[0] & 0xff) != 0xaa)
		return -ENOSUPPORT;
	else
		host->version = SD_VERSION_2;

	return SUCCESS;
}


static kerrno_t mmc_send_op_cond_iter(struct mmc_host *host, int use_arg) 
{
	struct mmc_cmd cmd;
	int err;

	cmd.cmdidx = MMC_CMD_SEND_OP_COND;
	cmd.resp_type = MMC_RSP_R3;
	cmd.cmdarg = 0;
	if (use_arg)
		cmd.cmdarg = OCR_HCS |
			(host->cfg->voltages &
			(host->card->ocr & OCR_VOLTAGE_MASK)) |
			(host->card->ocr & OCR_ACCESS_MODE);

	err = mmc_send_cmd(host, &cmd, NULL);
	if (err < 0)
		return err;
	host->card->ocr = cmd.response[0];
	return SUCCESS;
}

static kerrno_t mmc_send_op_cond(struct mmc_host *host) 
{
	int err, i;
	int timeout = 1000;
	uint64_t start;

	/* Some cards seem to need this */
	mmc_go_idle(host);

	start = get_timer(0);
 	/* Asking to the card its capabilities */
	for (i = 0; ; i++) {
		err = mmc_send_op_cond_iter(host, i != 0);
		if (err < 0)
			return err;

		/* exit if not busy (flag seems to be inverted) */
		if (host->card->ocr & OCR_BUSY)
			break;

		if (get_timer(start)  > timeout)
			return -ETIMEOUT;
		udelay(100);
	}
	host->card->op_cond_pending = 1;
	return 0;
}

static kerrno_t mmc_complete_op_cond(struct mmc_host *host) 
{
//	struct mmc_cmd cmd;
	int timeout = 1000;
	uint64_t start;
	int err;

	host->card->op_cond_pending = 0;
	if (!(host->card->ocr & OCR_BUSY)) {
		/* Some cards seem to need this */
		mmc_go_idle(host);

		start = get_timer(0);
		while (1) {
			err = mmc_send_op_cond_iter(host, 1);
			if (err < 0)
				return err;
			if (host->card->ocr & OCR_BUSY)
				break;
			if (get_timer(start) > timeout)
				return -ETIMEOUT;
			udelay(100);
		}
	}

	// if (mmc_host_is_spi(mmc)) { /* read OCR for spi */
	// 	cmd.cmdidx = MMC_CMD_SPI_READ_OCR;
	// 	cmd.resp_type = MMC_RSP_R3;
	// 	cmd.cmdarg = 0;

	// 	err = mmc_send_cmd(mmc, &cmd, NULL);

	// 	if (err)
	// 		return err;

	// 	mmc->ocr = cmd.response[0];
	// }

	host->version = MMC_VERSION_UNKNOWN;

	host->card->high_capacity = ((host->card->ocr & OCR_HCS) == OCR_HCS);
	host->card->rca = 1;

	return 0;
}

kerrno_t mmc_get_op_cond(struct mmc_host *host, bool quiet) 
{

    bool uhs_en = supports_uhs(host->cfg->host_caps);
	kerrno_t err;
    
    if (host->card->has_init)
		return SUCCESS;
    
    err = mmc_power_init(host);
	if (err)
		return err;

	if(host->ops->init)
		err = host->ops->init(host);
    

	if (err)
		return err;

	host->card->ddr_mode = 0;

retry:
 	mmc_set_initial_state(host);
    
 	/* Reset the Card */
 	err = mmc_go_idle(host);
 	if (err < 0) return err;

// 	/* The internal partition reset to user partition(0) at every CMD0 */
// //	mmc_get_blk_desc(mmc)->hwpart = 0;

	/* Test for SD version 2 */
	err = mmc_send_if_cond(host);

	/* Now try to get the SD card's operating condition */
	err = sd_send_op_cond(host, uhs_en);
	if ((err < 0) && uhs_en) {
		uhs_en = false;
// 		mmc_power_cycle(mmc);
		goto retry;
	}

	/* If the command timed out, we check for an MMC card */
	if (err == -ETIMEOUT) {
		err = mmc_send_op_cond(host);

 		if (err < 0) {
// #if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_LIBCOMMON_SUPPORT)
// 			if (!quiet)
// 				printf("[MMC] Card did not respond to voltage select! : %d\n", err);
// #endif
 			return -ENOSUPPORT;
 		}
 	}

	return err;
}

int mmc_bread(void* dst, uint32_t src_lba, size_t size) 
{
    return mmc_read_blocks(&sd_mmc, dst, src_lba, size);
}

int mmc_init()
{
    int err;
    bool no_card;
//    board_mmc_init(&sd_mmc);
    
    no_card = 0; //sd_mmc.ops->getcd(&sd_mmc);
    if (no_card) {
		sd_mmc.card->has_init = 0;
		debug("[MMC] no card present\n");
		return -ENOENT; // ENOMEDIUM
	}

    err = mmc_get_op_cond(&sd_mmc, false);


    // if (!err)
	// 	mmc->init_in_progress = 1;

    // 	mmc->init_in_progress = 0;
	if (sd_mmc.card->op_cond_pending)
	 	err = mmc_complete_op_cond(&sd_mmc);

	if (!err)
		err = mmc_startup(&sd_mmc);

	if (err < 0)
		sd_mmc.card->has_init = 0;
	else
		sd_mmc.card->has_init = 1;

//    debug("[MMC] %s.%d MMC init err %d\n", __func__,__LINE__, err);
	// mmc->bread = mmc_bread;

    return err;
}

uint64_t get_card_capacity()
{
    return sd_mmc.card->capacity_user;
}