from pathlib import Path

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
library_dir = project_dir / ".pio" / "libdeps" / env.subst("$PIOENV") / "TFT_eSPI"
setup_source = project_dir / "include" / "User_Setup.h"
setup_target = library_dir / "User_Setup.h"

if setup_target.parent.exists():
    setup_target.write_text(setup_source.read_text(encoding="ascii"), encoding="ascii")

# TFT_eSPI 2.5.43 bug: the DMA end callback passes the host enum (SPI2_HOST = 1) to
# SPI_DMA_CONF_REG(), which wants the peripheral number (2), so it writes to address 0x30 and
# crashes on the first DMA transfer
dma_source = library_dir / "Processors" / "TFT_eSPI_ESP32_S3.c"
broken_line = "WRITE_PERI_REG(SPI_DMA_CONF_REG(spi_host), 0);"
fixed_line = "WRITE_PERI_REG(SPI_DMA_CONF_REG(spi_host + 1), 0);"
if dma_source.exists():
    text = dma_source.read_text(encoding="utf-8")
    if broken_line in text:
        dma_source.write_text(text.replace(broken_line, fixed_line), encoding="utf-8")
