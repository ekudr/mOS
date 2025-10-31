
#define SDIO1_BASE 0xD4280000UL



#define SPACEMIT_SDHC_MIN_FREQ    (400000)
#define SDHCI_CTRL_ADMA2_LEN_MODE BIT(10)
#define SDHCI_CTRL_CMD23_ENABLE BIT(11)
#define SDHCI_CTRL_HOST_VERSION_4_ENABLE BIT(12)
#define SDHCI_CTRL_ADDRESSING BIT(13)
#define SDHCI_CTRL_ASYNC_INT_ENABLE BIT(14)

#define SDHCI_CLOCK_PLL_EN BIT(3)

void sdhci_do_enable_v4_mode(struct mmc_host *host);
kerrno_t spacemit_sdhci_init(struct mmc_host *host);
void sdhci_do_enable_v4_mode(struct mmc_host *host);
