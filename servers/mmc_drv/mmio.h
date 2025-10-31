#ifndef __MMIO_H__
#define __MMIO_H__

static inline uint32_t readl(struct mmc_host *host, int reg)
{
    volatile uint32_t *p = (volatile uint32_t *)(host->ioaddr+reg);
    return *p;
}
static inline void writel(struct mmc_host *host, uint32_t val, int reg)
{
    volatile uint32_t *p = (volatile uint32_t *)(host->ioaddr+reg);
    *p = val;
}

static inline uint16_t readw(struct mmc_host *host, int reg)
{
    volatile uint16_t *p = (volatile uint16_t *)(host->ioaddr+reg);
    return *p;
}
static inline void writew(struct mmc_host *host, uint16_t val, int reg)
{
    volatile uint16_t *p = (volatile uint16_t *)(host->ioaddr+reg);
    *p = val;
}

static inline uint8_t readb(struct mmc_host *host, int reg)
{
    volatile uint8_t *p = (volatile uint8_t *)(host->ioaddr+reg);
    return *p;
}
static inline void writeb(struct mmc_host *host, uint8_t val, int reg)
{
    volatile uint8_t *p = (volatile uint8_t *)(host->ioaddr+reg);
    *p = val;
}

#endif /* __MMIO_H__ */