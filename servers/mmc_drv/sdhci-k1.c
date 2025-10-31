#include <string.h>
#include <mosstd.h>
#include <libsys/timer.h>
#include "mmc.h"
#include "sdhci.h"
#include "mmio.h"

#include "spacemit-k1.h"

// Init SDH0 port for sd card


kerrno_t board_init_host(struct mmc_host *host)
{
    kerrno_t err;

    void *base = mmap(NULL, 4096, MAP_MEMIO | MAP_READ | MAP_WRITE, (void*)SDIO1_BASE);
    if(base == 0)
        panic("[SD/MMC] init error");

    host->ioaddr = base;

    // bus frequency of mmc
    host->bus_hz = 208000000;

    host->quirks = SDHCI_QUIRK_WAIT_SEND_CMD;
    host->buswidth	= 4;
    host->max_clk = 50000000;
    host->caps = MMC_CAP(MMC_LEGACY) | MMC_MODE_1BIT  | MMC_CAP_CD_ACTIVE_HIGH | MMC_CAP(SD_HS) ;
//  | MMC_MODE_4BIT        ;

    /* Setup dsr related values */
//	host->mmc->dsr_imp = 0;
//	host->mmc->dsr = 0xffffffff;


    err = spacemit_sdhci_init(host);
    if(err <0) return err;

    return SUCCESS;
}

kerrno_t board_mmc_init(struct mmc_host *host)
{
    kerrno_t err;
    err = spacemit_sdhci_init(host);
    if(err < 0) return err;
    return SUCCESS;
}

void sdhci_do_enable_v4_mode(struct mmc_host *host) 
{
	
	uint16_t ctrl2;
	ctrl2 = readw(host, SDHCI_HOST_CONTROL2);
	ctrl2 |= SDHCI_CTRL_ADMA2_LEN_MODE
			| SDHCI_CTRL_CMD23_ENABLE
			| SDHCI_CTRL_HOST_VERSION_4_ENABLE
			| SDHCI_CTRL_ADDRESSING
			| SDHCI_CTRL_ASYNC_INT_ENABLE;

	writew(host, ctrl2, SDHCI_HOST_CONTROL2);
}

kerrno_t spacemit_sdhci_init(struct mmc_host *host)
{
    kerrno_t err;
	struct mmc_config *cfg;

    // struct mmc *mmc = host->mmc;

	 cfg = host->cfg;

    err = sdhci_setup_cfg(cfg, host, host->max_clk, SPACEMIT_SDHC_MIN_FREQ);
	if (err < 0) return err;

    debug("[SDHCI] Host configured\n");
	err = sdhci_init(host);
	if (err < 0) return err;

	/*
	enable v4 should execute after sdhci_init(prob), because sdhci reset would
	clear the SDHCI_HOST_CONTROL2 register.
	*/

    sdhci_do_enable_v4_mode(host);

    debug("SDHC_LEGACY_CTRL_REG REGISTER = 0x%X\n", readl(host, 0x10c));
    return SUCCESS;
}
