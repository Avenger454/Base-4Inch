#include "boot_screen.h"
#include "boot_delta.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#define BOOT_WIDTH 720
#define BOOT_HEIGHT 720
#define BOOT_PIXEL_COUNT (BOOT_WIDTH * BOOT_HEIGHT)
#define BOOT_FALLBACK_MS 15000

extern void gauge_ui_start(void);
extern const uint8_t boot_vpb_start[] asm("_binary_violent_poetry_vpb_start");
extern const uint8_t boot_vpb_end[] asm("_binary_violent_poetry_vpb_end");

lv_obj_t *boot_screen;
static lv_obj_t *boot_image;
static lv_obj_t *next_screen;
static lv_timer_t *frame_timer;
static lv_timer_t *exit_timer;
static lv_img_dsc_t boot_asset;
static boot_delta_t animation;
static uint16_t *boot_pixels;
static bool finish_pending;
static bool finished;
static bool profile_active;
static bool profile_hooked;
static lv_disp_drv_t *profile_driver;
static void (*previous_monitor_cb)(lv_disp_drv_t *, uint32_t, uint32_t);
static int64_t profile_log_start_us;
static uint32_t profile_refresh_count;
static uint32_t profile_refresh_ms_sum;
static uint32_t profile_refresh_ms_max;
static uint32_t profile_refresh_px_sum;
static uint32_t profile_refresh_px_max;
static uint32_t profile_frame_count;
static uint32_t profile_bbox_px_sum;
static uint32_t profile_decode_us_sum;
static uint32_t profile_decode_us_max;
static uint32_t profile_callback_us_sum;
static uint32_t profile_callback_us_max;

static void finish_cb(lv_timer_t *timer);

static void profile_reset_window(void)
{
    profile_refresh_count = 0;
    profile_refresh_ms_sum = 0;
    profile_refresh_ms_max = 0;
    profile_refresh_px_sum = 0;
    profile_refresh_px_max = 0;
    profile_frame_count = 0;
    profile_bbox_px_sum = 0;
    profile_decode_us_sum = 0;
    profile_decode_us_max = 0;
    profile_callback_us_sum = 0;
    profile_callback_us_max = 0;
}

/* LVGL calls this after each refresh with its elapsed time and rendered area. */
static void boot_monitor_cb(lv_disp_drv_t *driver, uint32_t time_ms, uint32_t px)
{
    if (previous_monitor_cb) previous_monitor_cb(driver, time_ms, px);
    if (!profile_active) return;

    profile_refresh_count++;
    profile_refresh_ms_sum += time_ms;
    if (time_ms > profile_refresh_ms_max) profile_refresh_ms_max = time_ms;
    profile_refresh_px_sum += px;
    if (px > profile_refresh_px_max) profile_refresh_px_max = px;

    const int64_t now_us = esp_timer_get_time();
    const int64_t elapsed_us = now_us - profile_log_start_us;
    if (elapsed_us < 1000000) return;

    const uint32_t lv_fps_x10 = elapsed_us > 0
        ? (uint32_t)((uint64_t)profile_refresh_count * 10000000ULL / elapsed_us)
        : 0;
    const uint32_t frame_fps_x10 = elapsed_us > 0
        ? (uint32_t)((uint64_t)profile_frame_count * 10000000ULL / elapsed_us)
        : 0;
    const uint32_t avg_refresh_ms = profile_refresh_count
        ? profile_refresh_ms_sum / profile_refresh_count : 0;
    const uint32_t avg_refresh_px = profile_refresh_count
        ? profile_refresh_px_sum / profile_refresh_count : 0;
    const uint32_t avg_bbox_px = profile_frame_count
        ? profile_bbox_px_sum / profile_frame_count : 0;
    const uint32_t avg_decode_us = profile_frame_count
        ? profile_decode_us_sum / profile_frame_count : 0;
    const uint32_t avg_callback_us = profile_frame_count
        ? profile_callback_us_sum / profile_frame_count : 0;

    ESP_LOGI("boot_prof",
             "lvfps=%u.%u framefps=%u.%u refresh=%u/%u ms px=%u avg/%u max "
             "bbox=%u avg (%u%%) decode=%u avg/%u max us "
             "frame_cb=%u avg/%u max us",
             lv_fps_x10 / 10, lv_fps_x10 % 10,
             frame_fps_x10 / 10, frame_fps_x10 % 10,
             avg_refresh_ms, profile_refresh_ms_max,
             avg_refresh_px, profile_refresh_px_max,
             avg_bbox_px, (avg_bbox_px * 100U) / (BOOT_WIDTH * BOOT_HEIGHT),
             avg_decode_us, profile_decode_us_max,
             avg_callback_us, profile_callback_us_max);

    profile_log_start_us = now_us;
    profile_reset_window();
}

static void profile_start(void)
{
    lv_disp_t *display = lv_disp_get_default();
    if (!display || !display->driver) return;

    profile_driver = display->driver;
    previous_monitor_cb = profile_driver->monitor_cb;
    profile_driver->monitor_cb = boot_monitor_cb;
    profile_hooked = true;
    profile_active = true;
    profile_log_start_us = esp_timer_get_time();
    profile_reset_window();
}

static void profile_stop(void)
{
    profile_active = false;
    if (profile_hooked && profile_driver &&
        profile_driver->monitor_cb == boot_monitor_cb) {
        profile_driver->monitor_cb = previous_monitor_cb;
    }
    profile_driver = NULL;
    previous_monitor_cb = NULL;
    profile_hooked = false;
}

static void schedule_finish(void)
{
    if (finish_pending || finished) return;
    finish_pending = true;
    if (frame_timer) {
        lv_timer_del(frame_timer);
        frame_timer = NULL;
    }
    if (exit_timer) {
        lv_timer_del(exit_timer);
        exit_timer = NULL;
    }
    exit_timer = lv_timer_create(finish_cb, 1, NULL);
    if (exit_timer) lv_timer_set_repeat_count(exit_timer, 1);
    else boot_finish(next_screen);
}

/* Called on a later LVGL turn so no currently-rendered image is deleted. */
static void finish_cb(lv_timer_t *timer)
{
    if (timer == exit_timer) exit_timer = NULL;
    boot_finish(next_screen);
}

static void frame_cb(lv_timer_t *timer)
{
    (void)timer;
    if (animation.frame >= animation.frames) {
        schedule_finish();
        return;
    }

    boot_rect_t dirty;
    const int64_t callback_start_us = esp_timer_get_time();
    const int64_t decode_start_us = callback_start_us;
    if (!boot_delta_step(&animation, boot_pixels, &dirty)) {
        ESP_LOGE("boot", "Native animation frame decode failed");
        schedule_finish();
        return;
    }
    const uint32_t decode_us = (uint32_t)(esp_timer_get_time() - decode_start_us);
    const uint32_t bbox_px = dirty.changed
        ? (uint32_t)(dirty.x2 - dirty.x1 + 1) * (dirty.y2 - dirty.y1 + 1)
        : 0;
    if (dirty.changed && boot_image) {
        lv_area_t area = {
            .x1 = dirty.x1, .y1 = dirty.y1,
            .x2 = dirty.x2, .y2 = dirty.y2,
        };
        lv_obj_invalidate_area(boot_image, &area);
    }
    const uint32_t callback_us = (uint32_t)(esp_timer_get_time() - callback_start_us);
    profile_frame_count++;
    profile_bbox_px_sum += bbox_px;
    profile_decode_us_sum += decode_us;
    if (decode_us > profile_decode_us_max) profile_decode_us_max = decode_us;
    profile_callback_us_sum += callback_us;
    if (callback_us > profile_callback_us_max) profile_callback_us_max = callback_us;
}

void boot_screen_create(void)
{
    const size_t asset_size = (size_t)(boot_vpb_end - boot_vpb_start);
    boot_screen = lv_obj_create(NULL);
    if (!boot_screen) return;
    lv_obj_remove_style_all(boot_screen);
    lv_obj_set_size(boot_screen, BOOT_WIDTH, BOOT_HEIGHT);
    lv_obj_set_style_bg_color(boot_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(boot_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(boot_screen, LV_OBJ_FLAG_SCROLLABLE);

    boot_pixels = heap_caps_malloc(
        BOOT_PIXEL_COUNT * sizeof(*boot_pixels),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!boot_pixels) {
        ESP_LOGE("boot", "Unable to allocate RGB565 frame buffer in PSRAM");
        lv_obj_del(boot_screen);
        boot_screen = NULL;
        return;
    }

    if (!boot_delta_open(&animation, boot_vpb_start, asset_size)) {
        ESP_LOGE("boot", "Native animation header is invalid");
        goto fail;
    }

    /* Decode the first frame before showing the image. */
    boot_rect_t initial_dirty;
    if (!boot_delta_step(&animation, boot_pixels, &initial_dirty)) {
        ESP_LOGE("boot", "Unable to decode first animation frame");
        goto fail;
    }

    boot_asset = (lv_img_dsc_t) {
        .header = {
            .cf = LV_IMG_CF_TRUE_COLOR,
            .w = BOOT_WIDTH,
            .h = BOOT_HEIGHT,
        },
        .data_size = BOOT_PIXEL_COUNT * sizeof(*boot_pixels),
        .data = (const uint8_t *)boot_pixels,
    };
    boot_image = lv_img_create(boot_screen);
    if (!boot_image) goto fail;
    lv_img_set_src(boot_image, &boot_asset);
    lv_obj_center(boot_image);
    return;

fail:
    lv_obj_del(boot_screen);
    boot_screen = NULL;
    boot_image = NULL;
    heap_caps_free(boot_pixels);
    boot_pixels = NULL;
}

void boot_start(void)
{
    finish_pending = false;
    finished = false;
    if (!boot_screen || !boot_image) {
        boot_finish(next_screen);
        return;
    }

    profile_start();
    lv_scr_load(boot_screen);
    frame_timer = lv_timer_create(frame_cb, animation.period_ms, NULL);
    exit_timer = lv_timer_create(finish_cb, BOOT_FALLBACK_MS, NULL);
    if (frame_timer) lv_timer_set_repeat_count(frame_timer, animation.frames);
    if (exit_timer) lv_timer_set_repeat_count(exit_timer, 1);
    ESP_LOGI("boot", "Native RGB565 animation started (%u frames, %u ms/frame)",
             animation.frames, animation.period_ms);
}

void boot_finish(lv_obj_t *screen)
{
    if (finished) return;
    finished = true;
    finish_pending = false;
    profile_stop();
    if (frame_timer) {
        lv_timer_del(frame_timer);
        frame_timer = NULL;
    }
    if (exit_timer) {
        lv_timer_del(exit_timer);
        exit_timer = NULL;
    }
    if (screen) lv_scr_load(screen);
    if (boot_screen) {
        lv_obj_del(boot_screen);
        boot_screen = NULL;
        boot_image = NULL;
    }
    if (boot_pixels) {
        heap_caps_free(boot_pixels);
        boot_pixels = NULL;
    }
    gauge_ui_start();
    ESP_LOGI("boot", "Boot animation finished; normal gauge active");
}

void boot_screen_show(lv_obj_t *screen)
{
    next_screen = screen;
    boot_screen_create();
    boot_start();
}
