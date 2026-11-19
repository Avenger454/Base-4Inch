#include "../ui.h"
#include <math.h>

/* Reference artwork is an unmodified RGB565 conversion of the supplied mockup.
 * The original sample digits are covered by an opaque LVGL panel. */
extern const uint8_t face_start[] asm("_binary_tach_face_rgb565_start");
LV_FONT_DECLARE(tach_digits);
static const lv_img_dsc_t face = {
    .header = {.cf = LV_IMG_CF_TRUE_COLOR, .w = 512, .h = 512},
    .data_size = 512 * 512 * 2, .data = face_start
};
lv_obj_t *ui_Screen1, *ui_rpm_bg, *ui_rpm_arc;
lv_obj_t *ui_label_mph_value, *ui_label_mph, *ui_label_gear;
lv_obj_t *ui_label_gear_value, *ui_label_odometer_value;
static lv_obj_t *rpm_label;
static int last_rpm = -1;
/* LVGL keeps this array by reference. */
static lv_point_t pointer_points[2];

void ui_Screen1_screen_init(void)
{
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_black(), 0);
    lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    ui_rpm_bg = lv_img_create(ui_Screen1);
    lv_img_set_src(ui_rpm_bg, &face);
    lv_obj_center(ui_rpm_bg);
    /* Fill the 720px circle with the face inside the decorative bezel.
     * The silver ring is outside the screen at this zoom. */
    lv_img_set_zoom(ui_rpm_bg, 425);

    lv_obj_t *panel = lv_obj_create(ui_Screen1);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, 184, 319);
    lv_obj_set_size(panel, 350, 96);
    lv_obj_set_style_bg_color(panel, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    rpm_label = lv_label_create(panel);
    lv_obj_set_style_text_font(rpm_label, &tach_digits, 0);
    lv_obj_set_style_text_color(rpm_label, lv_color_white(), 0);
    lv_label_set_text(rpm_label, "0");
    lv_obj_center(rpm_label);

    /* Retain the exported name; the pointer is now a narrow radial line.
     * Artwork at zoom 425: inner arch ~224px, outer arch ~352px. */
    ui_rpm_arc = lv_line_create(ui_Screen1);
    lv_obj_remove_style_all(ui_rpm_arc);
    lv_obj_set_pos(ui_rpm_arc, 0, 0);
    lv_obj_set_style_line_width(ui_rpm_arc, 8, 0);
    lv_obj_set_style_line_color(ui_rpm_arc, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_line_opa(ui_rpm_arc, LV_OPA_COVER, 0);
    lv_obj_set_style_line_rounded(ui_rpm_arc, false, 0);
    lv_obj_clear_flag(ui_rpm_arc, LV_OBJ_FLAG_CLICKABLE);
    /* Existing speed/gear/odo callbacks retain valid, hidden targets. */
    lv_obj_t **legacy[] = {&ui_label_mph_value, &ui_label_mph, &ui_label_gear,
                          &ui_label_gear_value, &ui_label_odometer_value};
    for(unsigned i = 0; i < sizeof(legacy) / sizeof(legacy[0]); ++i) {
        *legacy[i] = lv_label_create(ui_Screen1);
        lv_obj_add_flag(*legacy[i], LV_OBJ_FLAG_HIDDEN);
    }
    last_rpm = -1;
    ui_tach_set_rpm(0);
}

void ui_tach_set_rpm(int rpm)
{
    if(!rpm_label || !ui_rpm_arc) return;
    if(rpm < 0) rpm = 0;
    /* Preserve the actual numeric RPM above full scale; clamp only the marker. */
    if(rpm == last_rpm) return;
    last_rpm = rpm;
    lv_label_set_text_fmt(rpm_label, "%d", rpm);
    lv_obj_center(rpm_label);
    int capped = rpm > 6500 ? 6500 : rpm;
    /* Artwork ticks are not uniformly spaced. Calibrate to their positions. */
    static const int rpm_ticks[] = {0, 1000, 2000, 3000, 4000, 5000, 6000, 6500};
    static const int angle_ticks[] = {180, 198, 221, 246, 276, 304, 334, 360};
    unsigned segment = 0;
    while(segment < 6 && capped > rpm_ticks[segment + 1]) ++segment;
    float angle = angle_ticks[segment] +
        (float)(capped - rpm_ticks[segment]) *
        (angle_ticks[segment + 1] - angle_ticks[segment]) /
        (rpm_ticks[segment + 1] - rpm_ticks[segment]);
    float radians = angle * (3.14159265358979323846f / 180.0f);
    float x = cosf(radians), y = sinf(radians);
    pointer_points[0].x = (lv_coord_t)lroundf(360.0f + 224.0f * x);
    pointer_points[0].y = (lv_coord_t)lroundf(353.0f + 224.0f * y);
    pointer_points[1].x = (lv_coord_t)lroundf(360.0f + 350.0f * x);
    pointer_points[1].y = (lv_coord_t)lroundf(353.0f + 350.0f * y);
    lv_line_set_points(ui_rpm_arc, pointer_points, 2);
}

void ui_Screen1_screen_destroy(void)
{
    if(ui_Screen1) lv_obj_del(ui_Screen1);
    ui_Screen1 = ui_rpm_bg = ui_rpm_arc = rpm_label = NULL;
    ui_label_mph_value = ui_label_mph = ui_label_gear = NULL;
    ui_label_gear_value = ui_label_odometer_value = NULL;
    last_rpm = -1;
}
