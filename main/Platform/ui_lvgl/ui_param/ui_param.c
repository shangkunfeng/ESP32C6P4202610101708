#include "ui_param.h"

#include "ui_param_events.h"

#include "board_display.h"
#include "display_config.h"
#include "driver_co2.h"
#include "driver_hum.h"
#include "driver_rpm.h"
#include "driver_temp.h"
#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "ui_param";

LV_FONT_DECLARE(ui_font_home_16);
LV_FONT_DECLARE(ui_font_home_22);
LV_FONT_DECLARE(lv_font_source_han_sans_sc_16_cjk);

#define COL_CYAN       0x5EDFFF
#define COL_NAV_SEL    0x7ECFFF
#define COL_WHITE      0xF0F8FF
#define COL_PANEL      0x0C1E38
#define COL_BORDER     0x2A8FD4
#define COL_VALUE_BOX  0xF5F8FC
#define COL_VALUE_TEXT 0x1A2A3A

#define LAYOUT_PAD_X       24
#define LAYOUT_TITLE_TOP   22
#define LAYOUT_BODY_TOP    64
#define LAYOUT_BODY_BOTTOM 68
#define LAYOUT_NAV_W       200
#define LAYOUT_GAP         12

#define PARAM_GRID_COL_LBL   124
#define PARAM_GRID_COL_UNIT  60
#define PARAM_TA_H           48
#define PARAM_GRID_ROW_CNT   11

typedef struct {
    const char *name;
    const char *value;
    const char *unit;
} ui_param_field_row_t;

static const lv_font_t *font_body(void)
{
    return &ui_font_home_22;
}

static const lv_font_t *font_title(void)
{
    return &ui_font_home_22;
}

/** 右侧参数区字号（大于左侧列表） */
static const lv_font_t *font_panel(void)
{
    return &ui_font_home_22;
}

/** 扩展汉字（屏幕参数等），ui_font_home_22 未收录时用内置 CJK */
static const lv_font_t *font_cjk(void)
{
    return &lv_font_source_han_sans_sc_16_cjk;
}

static void setup_digits_only_keyboard(lv_obj_t *kb)
{
    static const char *const map[] = {
        "1", "2", "3", "\n",
        "4", "5", "6", "\n",
        "7", "8", "9", "\n",
        "清除", "0", ".", "\n",
        "删除", "确认", "",
    };
    static const lv_buttonmatrix_ctrl_t ctrl[] = {
        LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3,
        LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3,
        LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3,
        LV_BUTTONMATRIX_CTRL_WIDTH_6, LV_BUTTONMATRIX_CTRL_WIDTH_2, LV_BUTTONMATRIX_CTRL_WIDTH_2,
        LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_5,
    };

    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, map, ctrl);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
}

#define PARAM_KB_W        300
#define PARAM_KB_H        148
#define PARAM_KB_GAP      6
#define PARAM_KB_MARGIN   10

void ui_param_keyboard_apply_style(lv_obj_t *keyboard)
{
    if (!keyboard) {
        return;
    }

    const lv_font_t *f = font_body();
    lv_obj_set_style_text_font(keyboard, f, LV_PART_MAIN);
    lv_obj_set_style_text_font(keyboard, f, LV_PART_ITEMS);
    lv_obj_set_style_text_font(keyboard, f, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_font(keyboard, f, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_align(keyboard, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);

    lv_obj_set_style_pad_all(keyboard, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(keyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_column(keyboard, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(keyboard, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(keyboard, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(keyboard, 0, LV_PART_ITEMS);
}

void ui_param_keyboard_place_near_field(lv_obj_t *keyboard, lv_obj_t *field_ta)
{
    if (!keyboard || !field_ta) {
        return;
    }

    lv_area_t ta_a;
    lv_obj_get_coords(field_ta, &ta_a);

    lv_obj_set_size(keyboard, PARAM_KB_W, PARAM_KB_H);

    const int32_t ta_w = lv_area_get_width(&ta_a);
    int32_t x = ta_a.x1 + (ta_w - PARAM_KB_W) / 2;
    int32_t y = ta_a.y2 + PARAM_KB_GAP;

    if (y + PARAM_KB_H > (int32_t)LCD_V_RES - PARAM_KB_MARGIN) {
        y = ta_a.y1 - PARAM_KB_H - PARAM_KB_GAP;
    }
    if (y < PARAM_KB_MARGIN) {
        y = PARAM_KB_MARGIN;
    }

    if (x + PARAM_KB_W > (int32_t)LCD_H_RES - PARAM_KB_MARGIN) {
        x = (int32_t)LCD_H_RES - PARAM_KB_W - PARAM_KB_MARGIN;
    }
    if (x < PARAM_KB_MARGIN) {
        x = PARAM_KB_MARGIN;
    }

    lv_obj_set_align(keyboard, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(keyboard, x, y);
}

static void style_screen(lv_obj_t *screen)
{
    lv_obj_set_scrollable(screen, false);
    lv_obj_set_size(screen, LCD_H_RES, LCD_V_RES);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, LV_PART_MAIN);
}

static void style_nav_list(lv_obj_t *list)
{
    lv_obj_set_style_bg_color(list, lv_color_hex(COL_PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, lv_color_hex(COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_radius(list, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(list, 4, LV_PART_MAIN);
    lv_obj_set_style_text_font(list, font_body(), LV_PART_MAIN);
    lv_obj_set_style_text_color(list, lv_color_hex(COL_WHITE), LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
}

static void style_nav_list_button(lv_obj_t *btn)
{
    lv_obj_set_height(btn, 50);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_font(btn, font_body(), LV_PART_MAIN);
    lv_obj_set_style_text_color(btn, lv_color_hex(COL_WHITE), LV_PART_MAIN);

    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_NAV_SEL), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(btn, lv_color_hex(0x0A1A28), LV_PART_MAIN | LV_STATE_CHECKED);
}

static void style_action_btn(lv_obj_t *btn, bool primary)
{
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(COL_BORDER), LV_PART_MAIN);
    if (primary) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E6FD9), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(btn, lv_color_hex(0x0E4FA8), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A3A50), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    }
}

static lv_obj_t *create_action_btn(lv_obj_t *parent, const char *text, bool primary)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 140, 50);
    style_action_btn(btn, primary);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font_body(), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_WHITE), LV_PART_MAIN);
    lv_obj_center(lbl);
    return btn;
}

static lv_obj_t *create_value_ta(lv_obj_t *parent, const char *value, ui_param_group_t group, int field_idx)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_height(ta, PARAM_TA_H);
    lv_obj_set_width(ta, lv_pct(100));
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_text(ta, value);
    lv_textarea_set_max_length(ta, 16);
    lv_textarea_set_accepted_chars(ta, "0123456789.");
    lv_obj_set_style_radius(ta, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ta, lv_color_hex(COL_VALUE_BOX), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ta, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(ta, lv_color_hex(0xC5D0DC), LV_PART_MAIN);
    lv_obj_set_style_text_font(ta, font_panel(), LV_PART_MAIN);
    lv_obj_set_style_text_color(ta, lv_color_hex(COL_VALUE_TEXT), LV_PART_MAIN);
    lv_obj_set_style_pad_left(ta, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(ta, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_top(ta, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(ta, 6, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_clickable(ta, true);
    lv_obj_set_click_focusable(ta, true);
    ui_param_events_attach_value(ta);
    lv_obj_set_user_data(ta, (void *)(intptr_t)((((unsigned)group) << 8) | (unsigned)field_idx));
    return ta;
}

static void style_grid_label(lv_obj_t *lbl)
{
    lv_obj_set_style_text_font(lbl, font_panel(), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_WHITE), LV_PART_MAIN);
}

static void style_grid_unit(lv_obj_t *lbl)
{
    lv_obj_set_style_text_font(lbl, font_panel(), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_CYAN), LV_PART_MAIN);
}

static void grid_add_field_row(lv_obj_t *grid, int32_t row, const ui_param_field_row_t *field,
                               ui_param_group_t group, int field_idx)
{
    if (!grid || !field) {
        return;
    }

    lv_obj_t *ln = lv_label_create(grid);
    lv_label_set_text(ln, field->name);
    style_grid_label(ln);
    lv_obj_set_grid_cell(ln, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, row, 1);

    lv_obj_t *ta = create_value_ta(grid, field->value, group, field_idx);
    lv_obj_set_grid_cell(ta, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, row, 1);

    lv_obj_t *lu = lv_label_create(grid);
    lv_label_set_text(lu, field->unit ? field->unit : "");
    style_grid_unit(lu);
    lv_obj_set_grid_cell(lu, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, row, 1);
}

static void grid_add_section_title(lv_obj_t *grid, int32_t row, const char *title)
{
    lv_obj_t *ln = lv_label_create(grid);
    lv_label_set_text(ln, title);
    lv_obj_set_style_text_font(ln, font_title(), LV_PART_MAIN);
    lv_obj_set_style_text_color(ln, lv_color_hex(COL_CYAN), LV_PART_MAIN);
    lv_obj_set_grid_cell(ln, LV_GRID_ALIGN_START, 0, 3, LV_GRID_ALIGN_CENTER, row, 1);
}

/** LVGL 只保存指针，描述符须为 static/全局，不可使用栈上数组 */
static const int32_t s_param_grid_col_dsc[] = {
    PARAM_GRID_COL_LBL,
    LV_GRID_FR(1),
    PARAM_GRID_COL_UNIT,
    LV_GRID_TEMPLATE_LAST,
};

static const int32_t s_param_grid_row_dsc[] = {
    LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
    LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_TEMPLATE_LAST,
};

static lv_obj_t *create_module_panel(lv_obj_t *parent, int32_t w,
                                     const ui_param_field_row_t *rows, size_t row_cnt,
                                     const char *pid_title, ui_param_group_t group)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_width(panel, w);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COL_PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_column(panel, 12, LV_PART_MAIN);
    lv_obj_set_scrollable(panel, false);
    lv_obj_set_style_min_height(panel, 520, LV_PART_MAIN);
    lv_obj_set_layout(panel, LV_LAYOUT_GRID);

    lv_obj_set_grid_dsc_array(panel, s_param_grid_col_dsc, s_param_grid_row_dsc);

    int32_t grid_row = 0;
    for (size_t i = 0; i < row_cnt && grid_row < PARAM_GRID_ROW_CNT; i++) {
        if (pid_title && i == 6) {
            grid_add_section_title(panel, grid_row, pid_title);
            grid_row++;
        }
        grid_add_field_row(panel, grid_row, &rows[i], group, (int)i);
        grid_row++;
    }

    return panel;
}

static const ui_param_field_row_t s_fields_co2[] = {
    { "设定值", "800", "ppm" },
    { "运行小时", "2", "小时" },
    { "运行分钟", "0", "分钟" },
    { "回差", "5", "ppm" },
    { "高报警上限", "1200", "ppm" },
    { "低报警上限", "400", "ppm" },
    { "P", "1.0", "" },
    { "I", "0.5", "" },
    { "D", "0.1", "" },
    { "积分上限", "100", "" },
};

static const ui_param_field_row_t s_fields_temp[] = {
    { "设定值", "25.0", "℃" },
    { "运行小时", "1", "小时" },
    { "运行分钟", "0", "分钟" },
    { "回差", "0.5", "℃" },
    { "高报警上限", "35.0", "℃" },
    { "低报警上限", "15.0", "℃" },
    { "P", "2.0", "" },
    { "I", "0.8", "" },
    { "D", "0.2", "" },
    { "积分上限", "50", "" },
};

static const ui_param_field_row_t s_fields_hum[] = {
    { "设定值", "60.0", "%" },
    { "运行小时", "1", "小时" },
    { "运行分钟", "30", "分钟" },
    { "回差", "2.0", "%" },
    { "高报警上限", "80.0", "%" },
    { "低报警上限", "40.0", "%" },
    { "P", "1.5", "" },
    { "I", "0.6", "" },
    { "D", "0.0", "" },
    { "积分上限", "80", "" },
};

static const ui_param_field_row_t s_fields_rpm[] = {
    { "设定值", "1500", "rpm" },
    { "运行小时", "0", "小时" },
    { "运行分钟", "30", "分钟" },
    { "回差", "50", "rpm" },
    { "高报警上限", "3000", "rpm" },
    { "低报警上限", "500", "rpm" },
    { "P", "0.8", "" },
    { "I", "0.3", "" },
    { "D", "0.05", "" },
    { "积分上限", "200", "" },
};

static const char *const s_nav_labels[UI_PARAM_GROUP_COUNT] = {
    "CO2", "温度", "湿度", "转速", "屏幕参数设置",
};

static void style_screen_brightness_slider(lv_obj_t *slider)
{
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x1A3050), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(slider, 14, LV_PART_MAIN);

    lv_obj_set_style_bg_color(slider, lv_color_hex(0x1E6FD9), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 8, LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(slider, lv_color_hex(COL_CYAN), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 10, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
}

static lv_obj_t *create_screen_panel(lv_obj_t *parent, int32_t w, ui_param_t *ui)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_width(panel, w);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COL_PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 24, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollable(panel, false);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "屏幕亮度");
    lv_obj_set_style_text_font(title, font_cjk(), LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(COL_CYAN), LV_PART_MAIN);

    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text(hint, "拖动滑条调节背光（30–100%）");
    lv_obj_set_style_text_font(hint, font_cjk(), LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(COL_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_opa(hint, LV_OPA_80, LV_PART_MAIN);

    ui->slider_brightness = lv_slider_create(panel);
    lv_obj_set_width(ui->slider_brightness, w - 48);
    lv_obj_set_height(ui->slider_brightness, 36);
    lv_slider_set_range(ui->slider_brightness, LCD_BL_MIN_PERCENT, 100);
    lv_slider_set_mode(ui->slider_brightness, LV_SLIDER_MODE_NORMAL);
    style_screen_brightness_slider(ui->slider_brightness);

    ui->lbl_brightness_value = lv_label_create(panel);
    lv_label_set_text(ui->lbl_brightness_value, "100%");
    lv_obj_set_style_text_font(ui->lbl_brightness_value, font_panel(), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->lbl_brightness_value, lv_color_hex(COL_WHITE), LV_PART_MAIN);

    return panel;
}

void ui_param_sync_screen_brightness(ui_param_t *ui, bool to_hardware)
{
    if (!ui || !ui->slider_brightness) {
        return;
    }

    if (to_hardware) {
        int32_t v = lv_slider_get_value(ui->slider_brightness);
        if (v < LCD_BL_MIN_PERCENT) {
            v = LCD_BL_MIN_PERCENT;
            lv_slider_set_value(ui->slider_brightness, v, LV_ANIM_OFF);
        }
        board_set_backlight((uint32_t)v);
    } else {
        uint32_t v = board_get_backlight();
        if (v < LCD_BL_MIN_PERCENT) {
            v = LCD_BL_MIN_PERCENT;
            board_set_backlight(v);
        }
        lv_slider_set_value(ui->slider_brightness, (int32_t)v, LV_ANIM_OFF);
    }

    if (ui->lbl_brightness_value) {
        const int32_t shown = lv_slider_get_value(ui->slider_brightness);
        char buf[16];
        snprintf(buf, sizeof(buf), "%ld%%", (long)shown);
        lv_label_set_text(ui->lbl_brightness_value, buf);
    }
}

void ui_param_show_page(ui_param_t *ui, ui_param_page_t page)
{
    if (!ui || page >= UI_PARAM_GROUP_COUNT) {
        return;
    }

    ui->active_page = (ui_param_group_t)page;

    for (int i = 0; i < UI_PARAM_GROUP_COUNT; i++) {
        if (ui->group_panels[i]) {
            if (i == (int)page) {
                lv_obj_set_hidden(ui->group_panels[i], false);
            } else {
                lv_obj_set_hidden(ui->group_panels[i], true);
            }
        }
        if (ui->nav_btns[i]) {
            if (i == (int)page) {
                lv_obj_add_state(ui->nav_btns[i], LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(ui->nav_btns[i], LV_STATE_CHECKED);
            }
        }
    }
}

void ui_param_create(ui_param_t *ui, const lv_image_dsc_t *bg)
{
    if (!ui || !bg) {
        return;
    }

    ui->active_page = UI_PARAM_GROUP_CO2;
    ui->slider_brightness = NULL;
    ui->lbl_brightness_value = NULL;
    ui->nav_list = NULL;
    ui->panel_host = NULL;
    for (int i = 0; i < UI_PARAM_GROUP_COUNT; i++) {
        ui->group_panels[i] = NULL;
        ui->nav_btns[i] = NULL;
    }

    ui->screen = lv_obj_create(NULL);
    if (!ui->screen) {
        ESP_LOGE(TAG, "screen create failed");
        return;
    }
    style_screen(ui->screen);

    ui->bg_img = lv_image_create(ui->screen);
    lv_image_set_src(ui->bg_img, bg);
    lv_obj_set_size(ui->bg_img, LCD_H_RES, LCD_V_RES);
    lv_obj_align(ui->bg_img, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_clickable(ui->bg_img, false);
    lv_obj_set_scrollable(ui->bg_img, false);

    ui->content_root = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->content_root, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(ui->content_root, 0, 0);
    lv_obj_set_style_pad_all(ui->content_root, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->content_root, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->content_root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_scrollable(ui->content_root, false);

    lv_obj_t *root = ui->content_root;

    ui->lbl_title = lv_label_create(root);
    lv_label_set_text(ui->lbl_title, "传感器设置");
    lv_obj_set_style_text_font(ui->lbl_title, font_title(), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->lbl_title, lv_color_hex(COL_WHITE), LV_PART_MAIN);
    lv_obj_align(ui->lbl_title, LV_ALIGN_TOP_LEFT, LAYOUT_PAD_X, LAYOUT_TITLE_TOP);

    const int32_t body_h = LCD_V_RES - LAYOUT_BODY_TOP - LAYOUT_BODY_BOTTOM;
    const int32_t nav_x = LAYOUT_PAD_X;
    const int32_t nav_y = LAYOUT_BODY_TOP;
    const int32_t panel_x = nav_x + LAYOUT_NAV_W + LAYOUT_GAP;
    const int32_t panel_w = LCD_H_RES - panel_x - LAYOUT_PAD_X;

    ui->nav_list = lv_obj_create(root);
    lv_obj_set_pos(ui->nav_list, nav_x, nav_y);
    lv_obj_set_size(ui->nav_list, LAYOUT_NAV_W, body_h);
    style_nav_list(ui->nav_list);
    lv_obj_set_scrollable(ui->nav_list, true);
    lv_obj_set_scrollbar_mode(ui->nav_list, LV_SCROLLBAR_MODE_AUTO);

    for (int i = 0; i < UI_PARAM_GROUP_COUNT; i++) {
        lv_obj_t *btn = lv_button_create(ui->nav_list);
        lv_obj_set_width(btn, lv_pct(100));
        ui->nav_btns[i] = btn;
        lv_obj_set_checkable(btn, true);
        style_nav_list_button(btn);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, s_nav_labels[i]);
        lv_obj_set_style_text_font(lbl, font_body(), LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_WHITE), LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x0A1A28), LV_PART_MAIN | LV_STATE_CHECKED);
        if (i == (int)UI_PARAM_GROUP_SCREEN) {
            lv_obj_set_height(btn, LV_SIZE_CONTENT);
            lv_obj_set_style_pad_ver(btn, 8, LV_PART_MAIN);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(lbl, LAYOUT_NAV_W - 24);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, font_cjk(), LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, font_cjk(), LV_PART_MAIN | LV_STATE_CHECKED);
            lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
        } else {
            lv_obj_center(lbl);
        }
        ui_param_events_attach_nav_button(btn, (ui_param_group_t)i);
    }

    ui->panel_host = lv_obj_create(root);
    lv_obj_set_pos(ui->panel_host, panel_x, nav_y);
    lv_obj_set_size(ui->panel_host, panel_w, body_h);
    lv_obj_set_style_bg_opa(ui->panel_host, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->panel_host, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->panel_host, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(ui->panel_host, 16, LV_PART_MAIN);
    lv_obj_set_scrollable(ui->panel_host, true);
    lv_obj_set_scrollbar_mode(ui->panel_host, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(ui->panel_host, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(ui->panel_host, LV_SCROLL_SNAP_NONE);

    ui->group_panels[UI_PARAM_GROUP_CO2] =
        create_module_panel(ui->panel_host, panel_w - 8, s_fields_co2,
                            sizeof(s_fields_co2) / sizeof(s_fields_co2[0]), "PID控制参数",
                            UI_PARAM_GROUP_CO2);
    ui->group_panels[UI_PARAM_GROUP_TEMP] =
        create_module_panel(ui->panel_host, panel_w - 8, s_fields_temp,
                            sizeof(s_fields_temp) / sizeof(s_fields_temp[0]), "PID控制参数",
                            UI_PARAM_GROUP_TEMP);
    ui->group_panels[UI_PARAM_GROUP_HUM] =
        create_module_panel(ui->panel_host, panel_w - 8, s_fields_hum,
                            sizeof(s_fields_hum) / sizeof(s_fields_hum[0]), "PID控制参数",
                            UI_PARAM_GROUP_HUM);
    ui->group_panels[UI_PARAM_GROUP_RPM] =
        create_module_panel(ui->panel_host, panel_w - 8, s_fields_rpm,
                            sizeof(s_fields_rpm) / sizeof(s_fields_rpm[0]), "PID控制参数",
                            UI_PARAM_GROUP_RPM);
    ui->group_panels[UI_PARAM_GROUP_SCREEN] =
        create_screen_panel(ui->panel_host, panel_w - 8, ui);

    for (int i = 0; i < UI_PARAM_GROUP_COUNT; i++) {
        if (ui->group_panels[i]) {
            lv_obj_align(ui->group_panels[i], LV_ALIGN_TOP_LEFT, 0, 0);
        }
    }

    ui->btn_reset = create_action_btn(root, "重置默认", false);
    lv_obj_align(ui->btn_reset, LV_ALIGN_BOTTOM_LEFT, LAYOUT_PAD_X, -14);

    ui->btn_back = create_action_btn(root, "返回", false);
    lv_obj_set_size(ui->btn_back, 96, 44);
    lv_obj_align(ui->btn_back, LV_ALIGN_BOTTOM_RIGHT, -LAYOUT_PAD_X, -14);

    ui->btn_confirm = create_action_btn(root, "确认", true);
    lv_obj_set_size(ui->btn_confirm, 120, 44);
    lv_obj_align(ui->btn_confirm, LV_ALIGN_BOTTOM_RIGHT, -(LAYOUT_PAD_X + 96 + 12), -14);

    ui->keyboard = lv_keyboard_create(ui->screen);
    lv_obj_set_floating(ui->keyboard, true);
    setup_digits_only_keyboard(ui->keyboard);
    ui_param_keyboard_apply_style(ui->keyboard);
    lv_obj_set_align(ui->keyboard, LV_ALIGN_TOP_LEFT);
    lv_obj_set_hidden(ui->keyboard, true);
    lv_obj_set_style_bg_color(ui->keyboard, lv_color_hex(0x142840), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->keyboard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_move_foreground(ui->keyboard);

    ui_param_events_register(ui);
    ui_param_sync_screen_brightness(ui, false);
    ui_param_show_page(ui, UI_PARAM_GROUP_CO2);

    ESP_LOGI(TAG, "ui_param CO2/温度/湿度/转速/屏幕");
}

#define UI_PARAM_FIELD_COUNT  10
#define UI_PARAM_PID_SCALE    10.0f
#define UI_PARAM_U8_MAX       255.0f
#define UI_PARAM_PID_DISP_MAX 25.5f
#define UI_PARAM_RUN_HOUR_MAX 999.0f
#define UI_PARAM_RUN_MIN_MAX  59.0f
#define UI_CO2_SET_MAX        5000.0f
#define UI_TEMP_SET_MIN       (-40.0f)
#define UI_TEMP_SET_MAX       100.0f
#define UI_HUM_SET_MAX        100.0f
#define UI_RPM_SET_MAX        10000.0f

static float clamp_float(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static const char *ta_text(lv_obj_t *ta)
{
    const char *t = lv_textarea_get_text(ta);
    return t ? t : "";
}

static void panel_collect_textareas(lv_obj_t *panel, lv_obj_t **tas, size_t *count)
{
    *count = 0;
    if (!panel || !tas) {
        return;
    }

    const uint32_t n = lv_obj_get_child_count(panel);
    for (uint32_t i = 0; i < n && *count < UI_PARAM_FIELD_COUNT; i++) {
        lv_obj_t *ch = lv_obj_get_child(panel, i);
        if (lv_obj_check_type(ch, &lv_textarea_class)) {
            tas[(*count)++] = ch;
        }
    }
}

static void field_limit_range(ui_param_group_t group, int field_idx, float *lo, float *hi, bool *one_decimal)
{
    *lo = 0.0f;
    *hi = UI_PARAM_U8_MAX;
    *one_decimal = false;

    if (field_idx == 1) {
        *hi = UI_PARAM_RUN_HOUR_MAX;
        return;
    }
    if (field_idx == 2) {
        *hi = UI_PARAM_RUN_MIN_MAX;
        return;
    }
    if (field_idx >= 6 && field_idx <= 8) {
        *hi = UI_PARAM_PID_DISP_MAX;
        *one_decimal = true;
        return;
    }
    if (field_idx == 9) {
        return;
    }

    if (field_idx == 0) {
        switch (group) {
        case UI_PARAM_GROUP_CO2:
            *hi = UI_CO2_SET_MAX;
            break;
        case UI_PARAM_GROUP_TEMP:
            *lo = UI_TEMP_SET_MIN;
            *hi = UI_TEMP_SET_MAX;
            *one_decimal = true;
            break;
        case UI_PARAM_GROUP_HUM:
            *hi = UI_HUM_SET_MAX;
            *one_decimal = true;
            break;
        case UI_PARAM_GROUP_RPM:
            *hi = UI_RPM_SET_MAX;
            break;
        default:
            break;
        }
        return;
    }

    if (field_idx == 3 && (group == UI_PARAM_GROUP_TEMP || group == UI_PARAM_GROUP_HUM)) {
        *hi = UI_PARAM_PID_DISP_MAX;
        *one_decimal = true;
        return;
    }
    if ((field_idx == 4 || field_idx == 5) &&
        (group == UI_PARAM_GROUP_TEMP || group == UI_PARAM_GROUP_HUM)) {
        *hi = UI_HUM_SET_MAX;
        *one_decimal = true;
        return;
    }
}

void ui_param_clamp_textarea(lv_obj_t *ta)
{
    if (!ta) {
        return;
    }

    const intptr_t ud = (intptr_t)lv_obj_get_user_data(ta);
    const int field_idx = (int)(ud & 0xff);
    const ui_param_group_t group = (ui_param_group_t)((ud >> 8) & 0xff);
    if (group >= UI_PARAM_GROUP_COUNT || field_idx < 0 || field_idx >= UI_PARAM_FIELD_COUNT) {
        return;
    }

    float lo = 0.0f;
    float hi = UI_PARAM_U8_MAX;
    bool one_decimal = false;
    field_limit_range(group, field_idx, &lo, &hi, &one_decimal);

    float v = (float)atof(ta_text(ta));
    v = clamp_float(v, lo, hi);

    char buf[24];
    if (one_decimal) {
        snprintf(buf, sizeof(buf), "%.1f", (double)v);
    } else {
        snprintf(buf, sizeof(buf), "%.0f", (double)v);
    }
    lv_textarea_set_text(ta, buf);
}

void ui_param_clamp_all_fields(ui_param_t *ui)
{
    if (!ui) {
        return;
    }

    for (int g = 0; g < UI_PARAM_GROUP_COUNT; g++) {
        if (g == (int)UI_PARAM_GROUP_SCREEN) {
            continue;
        }
        lv_obj_t *tas[UI_PARAM_FIELD_COUNT];
        size_t n = 0;
        panel_collect_textareas(ui->group_panels[g], tas, &n);
        for (size_t i = 0; i < n; i++) {
            ui_param_clamp_textarea(tas[i]);
        }
    }
}

static uint8_t clamp_u8_from_float(float v)
{
    if (v < 0.0f) {
        return 0;
    }
    if (v > 255.0f) {
        return 255;
    }
    return (uint8_t)v;
}

static void sync_co2_panel(lv_obj_t *panel, bool to_driver)
{
    lv_obj_t *tas[UI_PARAM_FIELD_COUNT];
    size_t n = 0;
    panel_collect_textareas(panel, tas, &n);
    if (n < UI_PARAM_FIELD_COUNT) {
        return;
    }

    if (to_driver) {
        co2_struct.CO2_Set_Value = (float)atof(ta_text(tas[0]));
        co2_struct.Run_Time_Hour = (uint16_t)atoi(ta_text(tas[1]));
        co2_struct.Run_Time_Minute = (uint16_t)atoi(ta_text(tas[2]));
        co2_struct.Hysteresis = clamp_u8_from_float((float)atof(ta_text(tas[3])));
        co2_struct.Alarm_High = clamp_u8_from_float((float)atof(ta_text(tas[4])));
        co2_struct.Alarm_Low = clamp_u8_from_float((float)atof(ta_text(tas[5])));
        co2_struct.PID_P_Value = clamp_u8_from_float((float)atof(ta_text(tas[6])) * UI_PARAM_PID_SCALE);
        co2_struct.PID_I_Value = clamp_u8_from_float((float)atof(ta_text(tas[7])) * UI_PARAM_PID_SCALE);
        co2_struct.PID_D_Value = clamp_u8_from_float((float)atof(ta_text(tas[8])) * UI_PARAM_PID_SCALE);
        co2_struct.PID_Max_Value = clamp_u8_from_float((float)atof(ta_text(tas[9])));
        return;
    }

    char buf[24];
    snprintf(buf, sizeof(buf), "%.0f", (double)co2_struct.CO2_Set_Value);
    lv_textarea_set_text(tas[0], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)co2_struct.Run_Time_Hour);
    lv_textarea_set_text(tas[1], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)co2_struct.Run_Time_Minute);
    lv_textarea_set_text(tas[2], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)co2_struct.Hysteresis);
    lv_textarea_set_text(tas[3], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)co2_struct.Alarm_High);
    lv_textarea_set_text(tas[4], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)co2_struct.Alarm_Low);
    lv_textarea_set_text(tas[5], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)co2_struct.PID_P_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[6], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)co2_struct.PID_I_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[7], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)co2_struct.PID_D_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[8], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)co2_struct.PID_Max_Value);
    lv_textarea_set_text(tas[9], buf);
}

static void sync_temp_panel(lv_obj_t *panel, bool to_driver)
{
    lv_obj_t *tas[UI_PARAM_FIELD_COUNT];
    size_t n = 0;
    panel_collect_textareas(panel, tas, &n);
    if (n < UI_PARAM_FIELD_COUNT) {
        return;
    }

    if (to_driver) {
        temp_struct.Temp_Set_Value = (float)atof(ta_text(tas[0]));
        temp_struct.Run_Time_Hour = (uint16_t)atoi(ta_text(tas[1]));
        temp_struct.Run_Time_Minute = (uint16_t)atoi(ta_text(tas[2]));
        temp_struct.Hysteresis = clamp_u8_from_float((float)atof(ta_text(tas[3])) * UI_PARAM_PID_SCALE);
        temp_struct.Alarm_High = clamp_u8_from_float((float)atof(ta_text(tas[4])));
        temp_struct.Alarm_Low = clamp_u8_from_float((float)atof(ta_text(tas[5])));
        temp_struct.PID_P_Value = clamp_u8_from_float((float)atof(ta_text(tas[6])) * UI_PARAM_PID_SCALE);
        temp_struct.PID_I_Value = clamp_u8_from_float((float)atof(ta_text(tas[7])) * UI_PARAM_PID_SCALE);
        temp_struct.PID_D_Value = clamp_u8_from_float((float)atof(ta_text(tas[8])) * UI_PARAM_PID_SCALE);
        temp_struct.PID_Max_Value = clamp_u8_from_float((float)atof(ta_text(tas[9])));
        return;
    }

    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f", (double)temp_struct.Temp_Set_Value);
    lv_textarea_set_text(tas[0], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)temp_struct.Run_Time_Hour);
    lv_textarea_set_text(tas[1], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)temp_struct.Run_Time_Minute);
    lv_textarea_set_text(tas[2], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)temp_struct.Hysteresis / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[3], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)temp_struct.Alarm_High);
    lv_textarea_set_text(tas[4], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)temp_struct.Alarm_Low);
    lv_textarea_set_text(tas[5], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)temp_struct.PID_P_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[6], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)temp_struct.PID_I_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[7], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)temp_struct.PID_D_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[8], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)temp_struct.PID_Max_Value);
    lv_textarea_set_text(tas[9], buf);
}

static void sync_hum_panel(lv_obj_t *panel, bool to_driver)
{
    lv_obj_t *tas[UI_PARAM_FIELD_COUNT];
    size_t n = 0;
    panel_collect_textareas(panel, tas, &n);
    if (n < UI_PARAM_FIELD_COUNT) {
        return;
    }

    if (to_driver) {
        hum_struct.Hum_Set_Value = (float)atof(ta_text(tas[0]));
        hum_struct.Run_Time_Hour = (uint16_t)atoi(ta_text(tas[1]));
        hum_struct.Run_Time_Minute = (uint16_t)atoi(ta_text(tas[2]));
        hum_struct.Hysteresis = clamp_u8_from_float((float)atof(ta_text(tas[3])) * UI_PARAM_PID_SCALE);
        hum_struct.Alarm_High = clamp_u8_from_float((float)atof(ta_text(tas[4])));
        hum_struct.Alarm_Low = clamp_u8_from_float((float)atof(ta_text(tas[5])));
        hum_struct.PID_P_Value = clamp_u8_from_float((float)atof(ta_text(tas[6])) * UI_PARAM_PID_SCALE);
        hum_struct.PID_I_Value = clamp_u8_from_float((float)atof(ta_text(tas[7])) * UI_PARAM_PID_SCALE);
        hum_struct.PID_D_Value = clamp_u8_from_float((float)atof(ta_text(tas[8])) * UI_PARAM_PID_SCALE);
        hum_struct.PID_Max_Value = clamp_u8_from_float((float)atof(ta_text(tas[9])));
        return;
    }

    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f", (double)hum_struct.Hum_Set_Value);
    lv_textarea_set_text(tas[0], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)hum_struct.Run_Time_Hour);
    lv_textarea_set_text(tas[1], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)hum_struct.Run_Time_Minute);
    lv_textarea_set_text(tas[2], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)hum_struct.Hysteresis / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[3], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)hum_struct.Alarm_High);
    lv_textarea_set_text(tas[4], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)hum_struct.Alarm_Low);
    lv_textarea_set_text(tas[5], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)hum_struct.PID_P_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[6], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)hum_struct.PID_I_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[7], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)hum_struct.PID_D_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[8], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)hum_struct.PID_Max_Value);
    lv_textarea_set_text(tas[9], buf);
}

static void sync_rpm_panel(lv_obj_t *panel, bool to_driver)
{
    lv_obj_t *tas[UI_PARAM_FIELD_COUNT];
    size_t n = 0;
    panel_collect_textareas(panel, tas, &n);
    if (n < UI_PARAM_FIELD_COUNT) {
        return;
    }

    if (to_driver) {
        rpm_struct.RPM_Set_Value = (float)atof(ta_text(tas[0]));
        rpm_struct.Run_Time_Hour = (uint16_t)atoi(ta_text(tas[1]));
        rpm_struct.Run_Time_Minute = (uint16_t)atoi(ta_text(tas[2]));
        rpm_struct.Hysteresis = clamp_u8_from_float((float)atof(ta_text(tas[3])));
        rpm_struct.Alarm_High = clamp_u8_from_float((float)atof(ta_text(tas[4])));
        rpm_struct.Alarm_Low = clamp_u8_from_float((float)atof(ta_text(tas[5])));
        rpm_struct.PID_P_Value = clamp_u8_from_float((float)atof(ta_text(tas[6])) * UI_PARAM_PID_SCALE);
        rpm_struct.PID_I_Value = clamp_u8_from_float((float)atof(ta_text(tas[7])) * UI_PARAM_PID_SCALE);
        rpm_struct.PID_D_Value = clamp_u8_from_float((float)atof(ta_text(tas[8])) * UI_PARAM_PID_SCALE);
        rpm_struct.PID_Max_Value = clamp_u8_from_float((float)atof(ta_text(tas[9])));
        return;
    }

    char buf[24];
    snprintf(buf, sizeof(buf), "%.0f", (double)rpm_struct.RPM_Set_Value);
    lv_textarea_set_text(tas[0], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)rpm_struct.Run_Time_Hour);
    lv_textarea_set_text(tas[1], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)rpm_struct.Run_Time_Minute);
    lv_textarea_set_text(tas[2], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)rpm_struct.Hysteresis);
    lv_textarea_set_text(tas[3], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)rpm_struct.Alarm_High);
    lv_textarea_set_text(tas[4], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)rpm_struct.Alarm_Low);
    lv_textarea_set_text(tas[5], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)rpm_struct.PID_P_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[6], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)rpm_struct.PID_I_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[7], buf);
    snprintf(buf, sizeof(buf), "%.1f", (double)rpm_struct.PID_D_Value / UI_PARAM_PID_SCALE);
    lv_textarea_set_text(tas[8], buf);
    snprintf(buf, sizeof(buf), "%u", (unsigned)rpm_struct.PID_Max_Value);
    lv_textarea_set_text(tas[9], buf);
}

void ui_param_sync_from_drivers(ui_param_t *ui)
{
    if (!ui) {
        return;
    }
    sync_co2_panel(ui->group_panels[UI_PARAM_GROUP_CO2], false);
    sync_temp_panel(ui->group_panels[UI_PARAM_GROUP_TEMP], false);
    sync_hum_panel(ui->group_panels[UI_PARAM_GROUP_HUM], false);
    sync_rpm_panel(ui->group_panels[UI_PARAM_GROUP_RPM], false);
    ui_param_sync_screen_brightness(ui, false);
}

void ui_param_sync_to_drivers(ui_param_t *ui)
{
    if (!ui) {
        return;
    }
    sync_co2_panel(ui->group_panels[UI_PARAM_GROUP_CO2], true);
    sync_temp_panel(ui->group_panels[UI_PARAM_GROUP_TEMP], true);
    sync_hum_panel(ui->group_panels[UI_PARAM_GROUP_HUM], true);
    sync_rpm_panel(ui->group_panels[UI_PARAM_GROUP_RPM], true);
    ui_param_sync_screen_brightness(ui, true);
}

void ui_param_show(const ui_param_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }
    lv_screen_load(ui->screen);
}

void ui_param_destroy(ui_param_t *ui)
{
    if (!ui) {
        return;
    }
    if (ui->screen) {
        lv_obj_delete(ui->screen);
        ui->screen = NULL;
        ui->bg_img = NULL;
        ui->content_root = NULL;
        ui->nav_list = NULL;
        ui->panel_host = NULL;
        for (int i = 0; i < UI_PARAM_GROUP_COUNT; i++) {
            ui->group_panels[i] = NULL;
            ui->nav_btns[i] = NULL;
        }
    }
}
