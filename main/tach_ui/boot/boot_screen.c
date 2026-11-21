#include "boot_screen.h"
#include "esp_log.h"

#if !LV_USE_GIF
#error "Enable CONFIG_LV_USE_GIF for the Violent Poetry boot test"
#endif

extern const uint8_t boot_gif_start[] asm("_binary_violent_poetry_gif_start");
extern const uint8_t boot_gif_end[] asm("_binary_violent_poetry_gif_end");

lv_obj_t *boot_screen;
static lv_obj_t *boot_logo;
static lv_obj_t *next_screen;
static lv_timer_t *exit_timer;
static lv_img_dsc_t boot_asset;
static bool finish_pending;

/* Runs after the decoder callback returns, so deleting the GIF is safe. */
static void finish_cb(lv_timer_t *timer)
{
    (void)timer;
    exit_timer = NULL;
    boot_finish(next_screen);
}

static void ready_cb(lv_event_t *event)
{
    (void)event;
    if (finish_pending) return;
    finish_pending = true;
    if (exit_timer) lv_timer_del(exit_timer);
    exit_timer = lv_timer_create(finish_cb, 1, NULL);
    lv_timer_set_repeat_count(exit_timer, 1);
}

void boot_screen_create(void)
{
    boot_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(boot_screen);
    lv_obj_set_size(boot_screen, 720, 720);
    lv_obj_set_style_bg_color(boot_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(boot_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(boot_screen, LV_OBJ_FLAG_SCROLLABLE);
    boot_logo = lv_gif_create(boot_screen);
    boot_asset = (lv_img_dsc_t) {
        .header = {.cf = LV_IMG_CF_RAW, .w = 360, .h = 360},
        .data_size = (uint32_t)(boot_gif_end - boot_gif_start),
        .data = boot_gif_start,
    };
    lv_gif_set_src(boot_logo, &boot_asset);
    lv_img_set_zoom(boot_logo, 512); /* 360px asset fills the 720px display. */
    lv_img_set_antialias(boot_logo, true);
    lv_obj_center(boot_logo);
    lv_obj_add_event_cb(boot_logo, ready_cb, LV_EVENT_READY, NULL);
}

void boot_start(void)
{
    finish_pending = false;
    lv_scr_load(boot_screen);
    /* Decoder completion normally exits after 8.4s. This bounds a failed decode. */
    exit_timer = lv_timer_create(finish_cb, 15000, NULL);
    lv_timer_set_repeat_count(exit_timer, 1);
    ESP_LOGI("boot", "Violent Poetry boot animation started (8.4s, 12.5fps)");
}

void boot_finish(lv_obj_t *screen)
{
    if (exit_timer) {
        lv_timer_del(exit_timer);
        exit_timer = NULL;
    }
    if (!screen) return;
    lv_scr_load(screen);
    if (boot_screen) {
        lv_obj_del(boot_screen); /* Also frees GIF decoder buffers and timer. */
        boot_screen = NULL;
        boot_logo = NULL;
    }
    ESP_LOGI("boot", "Boot animation finished; normal gauge active");
}

void boot_screen_show(lv_obj_t *screen)
{
    next_screen = screen;
    boot_screen_create();
    boot_start();
}
