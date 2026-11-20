#include "esp_idf_version.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_log.h"

esp_err_t __real_esp_lcd_new_dsi_bus(const esp_lcd_dsi_bus_config_t *config,
                                   esp_lcd_dsi_bus_handle_t *ret_bus);

/* Waveshare BSP 0.0.1 uses MIPI_DSI_PHY_CLK_SRC_DEFAULT, an alias for
 * PLL_F20M even on P4 rev 3.x. IDF 5.5.6 rejects that clock on rev 3.x.
 * Zero lets the IDF driver choose its build-revision-appropriate default.
 * Keep the caller's configuration untouched and preserve explicit clocks.
 */
esp_err_t __wrap_esp_lcd_new_dsi_bus(const esp_lcd_dsi_bus_config_t *config,
                                   esp_lcd_dsi_bus_handle_t *ret_bus)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 6)
    if (config && config->phy_clk_src == MIPI_DSI_PHY_CLK_SRC_DEFAULT) {
        esp_lcd_dsi_bus_config_t corrected = *config;
        corrected.phy_clk_src = 0;
        ESP_LOGI("DISPLAY_COMPAT", "Using IDF default MIPI DSI PHY reference clock");
        return __real_esp_lcd_new_dsi_bus(&corrected, ret_bus);
    }
#endif
    return __real_esp_lcd_new_dsi_bus(config, ret_bus);
}
