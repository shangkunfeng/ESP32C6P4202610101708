#include "driver_lcd.h"

#include "board_display.h"
#include "display_config.h"
#include "esp_lcd_jd9165.h"

#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"

static const char *TAG = "driver_lcd";

static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_dsi_bus_handle_t s_dsi_bus;

extern int driver_draw_init(void);

static esp_err_t driver_enable_dsi_phy_power(void)
{
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;

    const esp_ldo_channel_config_t ldo_cfg = 
    {
        .chan_id = MIPI_DSI_PHY_LDO_CHAN,
        .voltage_mv = MIPI_DSI_PHY_LDO_MV,
    };

    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &ldo_mipi_phy), TAG, "LDO acquire");
    ESP_LOGI(TAG, "MIPI D-PHY LDO ch%d %dmV", MIPI_DSI_PHY_LDO_CHAN, MIPI_DSI_PHY_LDO_MV);
    return ESP_OK;
}

int driver_lcd_init(void)
{
    ESP_LOGI(TAG, "init JD9165 MIPI %dx%d RGB565", LCD_H_RES, LCD_V_RES);

    ESP_RETURN_ON_ERROR(driver_enable_dsi_phy_power(), TAG, "phy power");

    const esp_lcd_dsi_bus_config_t bus_config = JD9165_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &s_dsi_bus), TAG, "dsi bus");

    const esp_lcd_dbi_io_config_t dbi_config = JD9165_PANEL_IO_DBI_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(s_dsi_bus, &dbi_config, &s_io), TAG, "panel io");

    esp_lcd_dpi_panel_config_t dpi_config =
        JD9165_1024_600_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_FMT_RGB565);

    jd9165_vendor_config_t vendor_config = 
    {
        .mipi_config = 
        {
            .dsi_bus = s_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };

    const esp_lcd_panel_dev_config_t panel_config = 
    {
        .reset_gpio_num = LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_jd9165(s_io, &panel_config, &s_panel), TAG, "new jd9165");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init");
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_enable_dma2d(s_panel), TAG, "dpi dma2d");

    ESP_RETURN_ON_ERROR(driver_draw_init(), TAG, "draw init");

    /* 首帧也走 back→present→VSync，再开背光，避免未同步时的花屏/闪一下 */
    ESP_RETURN_ON_ERROR(driver_lcd_fill_screen(0x0000), TAG, "prime black frame");

    ESP_RETURN_ON_ERROR(board_set_backlight(100), TAG, "backlight on");
    ESP_LOGI(TAG, "driver_lcd_init done");
    return ESP_OK;
}

void *driver_lcd_get_panel(void)
{
    return s_panel;
}
