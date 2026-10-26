#include "ui_login.h"

#include "ui_login_events.h"

#include "display_config.h"
#include "esp_log.h"

static const char *TAG = "ui_login";

/** 用户名/密码：仅数字与英文字母 */
static const char s_login_input_chars[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

LV_FONT_DECLARE(ui_font_login_16);

static const lv_font_t *login_font(void)
{
    return &ui_font_login_16;
}

static void style_screen(lv_obj_t *screen)
{
    lv_obj_set_scrollable(screen, false);
    lv_obj_set_size(screen, LCD_H_RES, LCD_V_RES);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(screen, 0, LV_PART_MAIN);
    /* 不透明底色：键盘收起/弹窗时避免透明区域露出默认浅色 */
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x081828), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
}

static void apply_label_white(lv_obj_t *label, const lv_font_t *font)
{
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
}

static lv_obj_t *create_login_panel(lv_obj_t *screen, ui_login_t *ui, const lv_font_t *font)
{
    ui->panel = lv_obj_create(screen);
    lv_obj_set_size(ui->panel, 420, 380);
    lv_obj_align(ui->panel, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_scrollable(ui->panel, false);
    lv_obj_set_style_bg_color(ui->panel, lv_color_hex(0x0B2A4A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->panel, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_radius(ui->panel, 16, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->panel, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui->panel, lv_color_hex(0x3BCFFF), LV_PART_MAIN);
    lv_obj_set_style_border_side(ui->panel, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ui->panel, 24, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(ui->panel, lv_color_hex(0x0088CC), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(ui->panel, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->panel, 24, LV_PART_MAIN);

    ui->title = lv_label_create(ui->panel);
    lv_label_set_text(ui->title, "工控系统登录");
    apply_label_white(ui->title, font);
    lv_obj_set_style_text_align(ui->title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(ui->title, LV_PCT(100));
    lv_obj_align(ui->title, LV_ALIGN_TOP_MID, 0, 8);

    ui->ta_user = lv_textarea_create(ui->panel);
    lv_obj_set_size(ui->ta_user, LV_PCT(100), 52);
    lv_obj_align(ui->ta_user, LV_ALIGN_TOP_MID, 0, 72);
    lv_textarea_set_one_line(ui->ta_user, true);
    lv_textarea_set_accepted_chars(ui->ta_user, s_login_input_chars);
    lv_textarea_set_max_length(ui->ta_user, 32);
    lv_textarea_set_placeholder_text(ui->ta_user, "用户名");
    lv_obj_set_style_radius(ui->ta_user, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui->ta_user, lv_color_hex(0xF5F8FC), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->ta_user, lv_color_hex(0x1A2A3A), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui->ta_user, font, LV_PART_MAIN);
    lv_obj_set_style_pad_left(ui->ta_user, 12, LV_PART_MAIN);

    ui->ta_pass = lv_textarea_create(ui->panel);
    lv_obj_set_size(ui->ta_pass, LV_PCT(100), 52);
    lv_obj_align(ui->ta_pass, LV_ALIGN_TOP_MID, 0, 140);
    lv_textarea_set_one_line(ui->ta_pass, true);
    lv_textarea_set_password_mode(ui->ta_pass, true);
    lv_textarea_set_accepted_chars(ui->ta_pass, s_login_input_chars);
    lv_textarea_set_max_length(ui->ta_pass, 32);
    lv_textarea_set_placeholder_text(ui->ta_pass, "密码");
    lv_obj_set_style_radius(ui->ta_pass, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui->ta_pass, lv_color_hex(0xF5F8FC), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->ta_pass, lv_color_hex(0x1A2A3A), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui->ta_pass, font, LV_PART_MAIN);
    lv_obj_set_style_pad_left(ui->ta_pass, 12, LV_PART_MAIN);

    ui->btn_login = lv_button_create(ui->panel);
    lv_obj_set_size(ui->btn_login, LV_PCT(100), 52);
    lv_obj_align(ui->btn_login, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_radius(ui->btn_login, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui->btn_login, lv_color_hex(0x1E6FD9), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(ui->btn_login, lv_color_hex(0x0E4FA8), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ui->btn_login, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ui->btn_login, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(ui->btn_login, lv_color_hex(0x0066CC), LV_PART_MAIN);

    lv_obj_t *btn_label = lv_label_create(ui->btn_login);
    lv_label_set_text(btn_label, "登录");
    apply_label_white(btn_label, font);
    lv_obj_center(btn_label);

    return ui->panel;
}

static lv_obj_t *create_status_icon(lv_obj_t *parent, lv_color_t color, const char *sym)
{
    lv_obj_t *icon = lv_obj_create(parent);
    lv_obj_set_size(icon, 40, 40);
    lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(icon, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(icon, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(icon, false);

    lv_obj_t *sym_lbl = lv_label_create(icon);
    lv_label_set_text(sym_lbl, sym);
    lv_obj_set_style_text_color(sym_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(sym_lbl);
    return icon;
}

static void create_status_panel(lv_obj_t *screen, ui_login_t *ui, const lv_font_t *font)
{
    ui->status_box = lv_obj_create(screen);
    lv_obj_set_size(ui->status_box, 220, 168);
    lv_obj_align(ui->status_box, LV_ALIGN_RIGHT_MID, -28, -70);
    lv_obj_set_style_bg_opa(ui->status_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->status_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->status_box, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(ui->status_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui->status_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(ui->status_box, 18, LV_PART_MAIN);
    lv_obj_set_scrollable(ui->status_box, false);

    typedef struct {
        lv_color_t color;
        const char *sym;
        const char *text;
    } row_t;

    const row_t rows[] = {
        { lv_color_hex(0x3DDC84), LV_SYMBOL_OK, "运行就绪" },
        { lv_color_hex(0x2196F3), LV_SYMBOL_WARNING, "温度 42℃" },
        { lv_color_hex(0x42A5F5), LV_SYMBOL_SETTINGS, "电压 380V" },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        lv_obj_t *row = lv_obj_create(ui->status_box);
        lv_obj_set_size(row, LV_PCT(100), 44);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
        lv_obj_set_scrollable(row, false);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 12, LV_PART_MAIN);

        create_status_icon(row, rows[i].color, rows[i].sym);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, rows[i].text);
        apply_label_white(lbl, font);
    }
}

static void create_login_keyboard(lv_obj_t *screen, ui_login_t *ui)
{
    ui->keyboard = lv_keyboard_create(screen);
    lv_obj_set_size(ui->keyboard, LCD_H_RES, 220);
    lv_obj_align(ui->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(ui->keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_hidden(ui->keyboard, true);
    lv_obj_set_style_bg_color(ui->keyboard, lv_color_hex(0x142840), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->keyboard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_move_foreground(ui->keyboard);
}

static void create_trend_chart(lv_obj_t *screen, ui_login_t *ui)
{
    const int32_t bar_values[] = { 28, 42, 36, 50, 46, 34, 40, 48 };
    const int32_t line_values[] = { 22, 38, 32, 48, 44, 30, 36, 52 };
    const uint32_t point_cnt = sizeof(bar_values) / sizeof(bar_values[0]);

    ui->chart_bar = lv_chart_create(screen);
    lv_obj_set_size(ui->chart_bar, 280, 175);
    lv_obj_align(ui->chart_bar, LV_ALIGN_BOTTOM_RIGHT, -24, -20);
    lv_chart_set_type(ui->chart_bar, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(ui->chart_bar, point_cnt);
    lv_chart_set_axis_range(ui->chart_bar, LV_CHART_AXIS_PRIMARY_Y, 0, 60);
    lv_chart_set_div_line_count(ui->chart_bar, 6, 0);
    lv_obj_set_style_bg_color(ui->chart_bar, lv_color_hex(0x0A2040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->chart_bar, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_radius(ui->chart_bar, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->chart_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui->chart_bar, lv_color_hex(0x2A6090), LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->chart_bar, 8, LV_PART_MAIN);

    ui->chart_bar_ser = lv_chart_add_series(ui->chart_bar,
                                            lv_color_hex(0x26C6DA),
                                            LV_CHART_AXIS_PRIMARY_Y);
    ui->chart_point_cnt = (uint8_t)point_cnt;
    ui->chart_demo_phase = 0;
    ui->chart_demo_timer = NULL;
    for (uint32_t i = 0; i < point_cnt; i++) {
        lv_chart_set_value_by_id(ui->chart_bar, ui->chart_bar_ser, i, bar_values[i]);
    }

    ui->chart_line = lv_chart_create(screen);
    lv_obj_set_size(ui->chart_line, 280, 175);
    lv_obj_align(ui->chart_line, LV_ALIGN_BOTTOM_RIGHT, -24, -20);
    lv_chart_set_type(ui->chart_line, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ui->chart_line, point_cnt);
    lv_chart_set_axis_range(ui->chart_line, LV_CHART_AXIS_PRIMARY_Y, 0, 60);
    lv_obj_set_style_bg_opa(ui->chart_line, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->chart_line, 0, LV_PART_MAIN);
    lv_obj_set_style_line_width(ui->chart_line, 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(ui->chart_line, 8, LV_PART_MAIN);

    ui->chart_line_ser = lv_chart_add_series(ui->chart_line,
                                             lv_color_hex(0x4FC3F7),
                                             LV_CHART_AXIS_PRIMARY_Y);
    for (uint32_t i = 0; i < point_cnt; i++) {
        lv_chart_set_value_by_id(ui->chart_line, ui->chart_line_ser, i, line_values[i]);
    }
}

void ui_login_create(ui_login_t *ui, const lv_image_dsc_t *bg)
{
    if (!ui || !bg) {
        return;
    }

    const lv_font_t *font = login_font();

    ui->screen = lv_obj_create(NULL);
    if (!ui->screen) {
        ESP_LOGE(TAG, "lv_obj_create(screen) failed");
        return;
    }
    style_screen(ui->screen);

    ui->bg_img = lv_image_create(ui->screen);
    if (!ui->bg_img) {
        ESP_LOGE(TAG, "lv_image_create failed");
        return;
    }
    lv_image_set_src(ui->bg_img, bg);
    lv_obj_set_size(ui->bg_img, LCD_H_RES, LCD_V_RES);
    lv_obj_align(ui->bg_img, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_clickable(ui->bg_img, false);
    lv_obj_set_scrollable(ui->bg_img, false);

    ui->keyboard = NULL;

    create_login_panel(ui->screen, ui, font);
    create_status_panel(ui->screen, ui, font);
    create_trend_chart(ui->screen, ui);
    create_login_keyboard(ui->screen, ui);

    ui_login_events_register(ui);

    ESP_LOGI(TAG, "screen login (ui_login.bin + widgets)");
}

void ui_login_show(const ui_login_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }
    lv_screen_load(ui->screen);
}

void ui_login_destroy(ui_login_t *ui)
{
    if (!ui) {
        return;
    }

    ui_login_events_unregister(ui);

    if (ui->screen) {
        lv_obj_delete(ui->screen);
        ui->screen = NULL;
        ui->bg_img = NULL;
        ui->panel = NULL;
        ui->title = NULL;
        ui->ta_user = NULL;
        ui->ta_pass = NULL;
        ui->btn_login = NULL;
        ui->keyboard = NULL;
        ui->status_box = NULL;
        ui->chart_bar = NULL;
        ui->chart_line = NULL;
        ui->chart_bar_ser = NULL;
        ui->chart_line_ser = NULL;
        ui->chart_point_cnt = 0;
        ui->chart_demo_phase = 0;
    }
}
