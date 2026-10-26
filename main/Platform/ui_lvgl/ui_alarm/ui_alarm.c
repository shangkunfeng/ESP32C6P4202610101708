#include "ui_alarm.h"

#include "ui_alarm_events.h"

#include "ui_param.h"

#include "display_config.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "ui_alarm";

LV_FONT_DECLARE(ui_font_home_22);

#define COL_CYAN       0x5EDFFF
#define COL_WHITE      0xF0F8FF
#define COL_PANEL      0x0C1E38
#define COL_BORDER     0x2A8FD4
#define COL_TABLE_BG   0xFFFFFF
#define COL_TABLE_LINE 0x000000
#define COL_TABLE_TEXT 0x1A1A1A
#define COL_TABLE_HEAD 0x0A3D6E

#define ALARM_PAD_X          0
#define ALARM_TEXT_PAD_X     12
#define ALARM_TITLE_TOP      22
#define ALARM_LIST_TOP       64
#define ALARM_LIST_BOTTOM    120
#define ALARM_ROWS_PAGE      5
#define ALARM_COL_COUNT      10
#define ALARM_HEADER_LIST_GAP 0

#define ALARM_LVL_COMM  2
#define ALARM_LVL_NORMAL 1

static const ui_alarm_record_t s_demo_records[] = {
    { 2026, 10, 24, 9, 5, "E0000", "风机体温高", ALARM_LVL_NORMAL, 0 },
    { 2026, 10, 24, 8, 40, "E0001", "风机体震动异常", ALARM_LVL_NORMAL, 0 },
    { 2026, 10, 24, 8, 12, "E0002", "风机体通讯异常", ALARM_LVL_COMM, 0 },
    { 2026, 10, 23, 22, 15, "E1000", "加热器超温", ALARM_LVL_NORMAL, 0 },
    { 2026, 10, 23, 18, 0, "E1001", "加热器升温不足", ALARM_LVL_NORMAL, 0 },
    { 2026, 10, 23, 12, 30, "E1002", "加热器通讯异常", ALARM_LVL_COMM, 0 },
    { 2026, 10, 23, 6, 45, "E2000", "加湿器缺水故障", ALARM_LVL_NORMAL, 0 },
    { 2026, 10, 22, 20, 10, "E2001", "加湿器水位过高", ALARM_LVL_NORMAL, 0 },
    { 2026, 10, 22, 15, 55, "E2002", "加湿器通讯异常", ALARM_LVL_COMM, 0 },
    { 2026, 10, 22, 9, 0, "E3000", "排气阀堵转异常", ALARM_LVL_NORMAL, 0 },
    { 2026, 10, 21, 14, 20, "E3001", "排气阀指令异常", ALARM_LVL_NORMAL, 0 },
    { 2026, 10, 21, 8, 5, "E3002", "排气阀通讯异常", ALARM_LVL_COMM, 0 },
};

#define ALARM_DEMO_COUNT ((int)(sizeof(s_demo_records) / sizeof(s_demo_records[0])))

static int s_visible_count = ALARM_DEMO_COUNT;

/** 10 列按权重平分整表宽度（行宽 = LCD_H_RES 1024px） */
static const int32_t s_alarm_col_dsc[] = {
    LV_GRID_FR(4),  LV_GRID_FR(5),  LV_GRID_FR(3), LV_GRID_FR(3), LV_GRID_FR(3), LV_GRID_FR(3),
    LV_GRID_FR(9),  LV_GRID_FR(8),  LV_GRID_FR(8), LV_GRID_FR(26),
    LV_GRID_TEMPLATE_LAST,
};

static const int32_t s_alarm_row_dsc[] = {
    LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST,
};

static const char *const s_col_titles[ALARM_COL_COUNT] = {
    "序号", "年", "月", "日", "时", "分", "警告代码", "告警等级", "发生次数", "警告信息",
};

/** 表头、数据行、底栏按钮：用 22px 并配合行高占满控件 */
static const lv_font_t *font_table(void)
{
    return &ui_font_home_22;
}

static const lv_font_t *font_title(void)
{
    return &ui_font_home_22;
}

static int32_t alarm_row_height(void)
{
    const int32_t list_h = LCD_V_RES - ALARM_LIST_TOP - ALARM_LIST_BOTTOM;
    return (list_h - ALARM_HEADER_LIST_GAP) / (ALARM_ROWS_PAGE + 1);
}

static int32_t alarm_table_outer_w(void)
{
    return LCD_H_RES - ALARM_PAD_X * 2;
}

/** 表头/数据网格固定占满屏宽 */
static int32_t alarm_table_grid_w(void)
{
    return LCD_H_RES;
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

static void style_table_surface(lv_obj_t *obj, lv_border_side_t outer_side)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(COL_TABLE_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, lv_color_hex(COL_TABLE_LINE), LV_PART_MAIN);
    lv_obj_set_style_border_side(obj, outer_side, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
}

static void style_table_label(lv_obj_t *lbl, bool header, bool message_col)
{
    lv_obj_set_style_text_font(lbl, font_table(), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, header ? lv_color_hex(COL_TABLE_HEAD) : lv_color_hex(COL_TABLE_TEXT),
                                LV_PART_MAIN);
    lv_obj_set_style_pad_all(lbl, 2, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, message_col ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

static void style_table_cell(lv_obj_t *cell, bool header, bool message_col, int col_idx, bool header_row)
{
    style_table_label(cell, header, message_col);
    lv_obj_set_style_bg_color(cell, lv_color_hex(COL_TABLE_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(cell, lv_color_hex(COL_TABLE_LINE), LV_PART_MAIN);
    lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);

    lv_border_side_t side = (lv_border_side_t)(LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM);
    if (col_idx == 0) {
        side = (lv_border_side_t)(side | LV_BORDER_SIDE_LEFT);
    }
    if (header_row) {
        side = (lv_border_side_t)(side | LV_BORDER_SIDE_TOP);
    }
    lv_obj_set_style_border_side(cell, side, LV_PART_MAIN);
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
    lv_obj_set_height(btn, 50);
    lv_obj_set_style_min_width(btn, 96, LV_PART_MAIN);
    style_action_btn(btn, primary);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font_table(), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_WHITE), LV_PART_MAIN);
    lv_obj_center(lbl);
    return btn;
}

static lv_obj_t *create_grid_row(lv_obj_t *parent, int32_t width, int32_t height, bool header)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, width);
    lv_obj_set_height(row, height);
    lv_obj_set_layout(row, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(row, s_alarm_col_dsc, s_alarm_row_dsc);
    lv_obj_set_style_bg_color(row, lv_color_hex(COL_TABLE_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(row, false);

    for (int c = 0; c < ALARM_COL_COUNT; c++) {
        const bool message_col = (c == ALARM_COL_COUNT - 1);
        lv_obj_t *lbl = lv_label_create(row);
        if (header) {
            lv_label_set_text(lbl, s_col_titles[c]);
        } else {
            lv_label_set_text(lbl, "");
            if (message_col) {
                lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
            }
        }
        style_table_cell(lbl, header, message_col, c, header);
        lv_obj_set_width(lbl, lv_pct(100));
        lv_obj_set_height(lbl, lv_pct(100));
        lv_obj_set_grid_cell(lbl, LV_GRID_ALIGN_STRETCH, c, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }

    return row;
}

static void fill_data_row(lv_obj_t *row, int seq, const ui_alarm_record_t *rec)
{
    if (!row || !rec) {
        return;
    }

    uint32_t ci = 0;
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++) {
        lv_obj_t *lbl = lv_obj_get_child(row, i);
        if (!lv_obj_check_type(lbl, &lv_label_class)) {
            continue;
        }
        char buf[32];
        switch (ci) {
        case 0:
            snprintf(buf, sizeof(buf), "%d", seq);
            break;
        case 1:
            snprintf(buf, sizeof(buf), "%u", (unsigned)rec->year);
            break;
        case 2:
            snprintf(buf, sizeof(buf), "%u", (unsigned)rec->month);
            break;
        case 3:
            snprintf(buf, sizeof(buf), "%u", (unsigned)rec->day);
            break;
        case 4:
            snprintf(buf, sizeof(buf), "%u", (unsigned)rec->hour);
            break;
        case 5:
            snprintf(buf, sizeof(buf), "%u", (unsigned)rec->minute);
            break;
        case 6:
            lv_label_set_text(lbl, rec->code ? rec->code : "");
            ci++;
            continue;
        case 7:
            snprintf(buf, sizeof(buf), "%u级", (unsigned)rec->level);
            break;
        case 8:
            if (rec->count > 0) {
                snprintf(buf, sizeof(buf), "%u", (unsigned)rec->count);
            } else {
                snprintf(buf, sizeof(buf), "--");
            }
            break;
        case 9:
            lv_label_set_text(lbl, rec->message ? rec->message : "");
            ci++;
            continue;
        default:
            buf[0] = '\0';
            break;
        }
        lv_label_set_text(lbl, buf);
        ci++;
    }
}

void ui_alarm_clear_all(void)
{
    s_visible_count = 0;
}

static void ui_alarm_recalc_pages(ui_alarm_t *ui)
{
    if (!ui) {
        return;
    }
    const int total = s_visible_count;
    ui->page_count = (total + ALARM_ROWS_PAGE - 1) / ALARM_ROWS_PAGE;
    if (ui->page_count < 1) {
        ui->page_count = 1;
    }
    if (ui->current_page >= ui->page_count) {
        ui->current_page = ui->page_count - 1;
    }
    if (ui->current_page < 0) {
        ui->current_page = 0;
    }
}

void ui_alarm_refresh_list(ui_alarm_t *ui)
{
    if (!ui || !ui->list) {
        return;
    }

    ui_alarm_recalc_pages(ui);

    lv_obj_clean(ui->list);

    const int32_t grid_w = alarm_table_grid_w();
    const int32_t row_h = alarm_row_height();
    const int start = ui->current_page * ALARM_ROWS_PAGE;

    for (int r = 0; r < ALARM_ROWS_PAGE; r++) {
        const int idx = start + r;
        lv_obj_t *item = lv_obj_create(ui->list);
        lv_obj_set_width(item, grid_w);
        lv_obj_set_height(item, row_h);
        lv_obj_set_style_bg_color(item, lv_color_hex(COL_TABLE_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_set_scrollable(item, false);
        lv_obj_set_clickable(item, false);

        lv_obj_t *row = create_grid_row(item, grid_w, row_h, false);
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 0);

        if (idx < s_visible_count) {
            fill_data_row(row, idx + 1, &s_demo_records[idx]);
        }
    }

    if (ui->lbl_page) {
        lv_label_set_text_fmt(ui->lbl_page, "第 %d / %d 页", ui->current_page + 1, ui->page_count);
    }
}

static void style_alarm_rows_panel(lv_obj_t *panel)
{
    style_table_surface(panel, (lv_border_side_t)(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM));
    lv_obj_set_style_text_font(panel, font_table(), LV_PART_MAIN);
    lv_obj_set_style_text_color(panel, lv_color_hex(COL_TABLE_TEXT), LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
}

void ui_alarm_create(ui_alarm_t *ui, const lv_image_dsc_t *bg)
{
    if (!ui || !bg) {
        return;
    }

    ui->current_page = 0;
    ui->page_count = 1;
    ui->panel_jump = NULL;
    ui->keyboard = NULL;
    ui->header_row = NULL;
    ui->list = NULL;

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
    lv_obj_set_style_bg_opa(ui->content_root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->content_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->content_root, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(ui->content_root, false);

    lv_obj_t *root = ui->content_root;

    ui->lbl_title = lv_label_create(root);
    lv_label_set_text(ui->lbl_title, "报警记录");
    lv_obj_set_style_text_font(ui->lbl_title, font_title(), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->lbl_title, lv_color_hex(COL_WHITE), LV_PART_MAIN);
    lv_obj_align(ui->lbl_title, LV_ALIGN_TOP_LEFT, ALARM_TEXT_PAD_X, ALARM_TITLE_TOP);

    const int32_t list_w = alarm_table_outer_w();
    const int32_t row_h = alarm_row_height();

    ui->header_row = lv_obj_create(root);
    lv_obj_set_size(ui->header_row, list_w, row_h);
    lv_obj_align(ui->header_row, LV_ALIGN_TOP_LEFT, ALARM_PAD_X, ALARM_LIST_TOP);
    style_table_surface(ui->header_row,
                        (lv_border_side_t)(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_TOP));
    lv_obj_set_scrollable(ui->header_row, false);

    const int32_t grid_w = alarm_table_grid_w();
    lv_obj_t *hdr_grid = create_grid_row(ui->header_row, grid_w, row_h, true);
    lv_obj_align(hdr_grid, LV_ALIGN_TOP_LEFT, 0, 0);

    ui->list = lv_obj_create(root);
    lv_obj_set_size(ui->list, list_w, row_h * ALARM_ROWS_PAGE);
    lv_obj_align(ui->list, LV_ALIGN_TOP_LEFT, ALARM_PAD_X, ALARM_LIST_TOP + row_h + ALARM_HEADER_LIST_GAP);
    style_alarm_rows_panel(ui->list);
    lv_obj_set_scrollable(ui->list, false);
    lv_obj_set_scrollbar_mode(ui->list, LV_SCROLLBAR_MODE_OFF);

    ui->lbl_page = lv_label_create(root);
    lv_obj_set_style_text_font(ui->lbl_page, font_table(), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->lbl_page, lv_color_hex(COL_CYAN), LV_PART_MAIN);
    lv_obj_align(ui->lbl_page, LV_ALIGN_BOTTOM_LEFT, ALARM_TEXT_PAD_X, -72);

    lv_obj_t *btn_row = lv_obj_create(root);
    lv_obj_set_size(btn_row, list_w, 52);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_LEFT, ALARM_PAD_X, -14);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(btn_row, false);

    ui->btn_prev = create_action_btn(btn_row, "上一页", false);
    ui->btn_next = create_action_btn(btn_row, "下一页", false);
    ui->btn_jump = create_action_btn(btn_row, "跳转", false);
    ui->btn_clear = create_action_btn(btn_row, "清除", false);
    ui->btn_back = create_action_btn(btn_row, "返回", true);

    ui->panel_jump = lv_obj_create(root);
    lv_obj_set_size(ui->panel_jump, 320, 140);
    lv_obj_align(ui->panel_jump, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_bg_color(ui->panel_jump, lv_color_hex(COL_PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->panel_jump, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui->panel_jump, lv_color_hex(COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->panel_jump, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(ui->panel_jump, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->panel_jump, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_row(ui->panel_jump, 12, LV_PART_MAIN);
    lv_obj_set_flex_flow(ui->panel_jump, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui->panel_jump, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(ui->panel_jump, false);
    lv_obj_set_hidden(ui->panel_jump, true);

    lv_obj_t *lj = lv_label_create(ui->panel_jump);
    lv_label_set_text(lj, "跳转到页码");
    lv_obj_set_width(lj, lv_pct(100));
    lv_obj_set_style_text_font(lj, font_table(), LV_PART_MAIN);
    lv_obj_set_style_text_color(lj, lv_color_hex(COL_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_align(lj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t *jump_input_row = lv_obj_create(ui->panel_jump);
    lv_obj_set_size(jump_input_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(jump_input_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(jump_input_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(jump_input_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(jump_input_row, 12, LV_PART_MAIN);
    lv_obj_set_flex_flow(jump_input_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(jump_input_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(jump_input_row, false);

    ui->ta_jump_page = lv_textarea_create(jump_input_row);
    lv_obj_set_width(ui->ta_jump_page, 120);
    lv_obj_set_height(ui->ta_jump_page, 46);
    lv_obj_set_style_text_font(ui->ta_jump_page, font_table(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui->ta_jump_page, lv_color_hex(COL_TABLE_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->ta_jump_page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->ta_jump_page, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui->ta_jump_page, lv_color_hex(COL_TABLE_LINE), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->ta_jump_page, lv_color_hex(COL_TABLE_TEXT), LV_PART_MAIN);
    lv_textarea_set_one_line(ui->ta_jump_page, true);
    lv_textarea_set_max_length(ui->ta_jump_page, 4);
    lv_textarea_set_accepted_chars(ui->ta_jump_page, "0123456789");
    lv_obj_set_scrollbar_mode(ui->ta_jump_page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_clickable(ui->ta_jump_page, true);
    lv_obj_set_click_focusable(ui->ta_jump_page, true);

    ui->btn_jump_ok = create_action_btn(jump_input_row, "确定", true);
    lv_obj_set_size(ui->btn_jump_ok, 88, 46);

    ui->keyboard = lv_keyboard_create(ui->screen);
    lv_obj_set_floating(ui->keyboard, true);
    ui_alarm_keyboard_setup(ui->keyboard);
    ui_param_keyboard_apply_style(ui->keyboard);
    lv_obj_set_align(ui->keyboard, LV_ALIGN_TOP_LEFT);
    lv_obj_set_hidden(ui->keyboard, true);
    lv_obj_set_style_bg_color(ui->keyboard, lv_color_hex(0x142840), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->keyboard, LV_OPA_COVER, LV_PART_MAIN);

    ui_alarm_recalc_pages(ui);
    ui_alarm_refresh_list(ui);
    ui_alarm_events_register(ui);

    ESP_LOGI(TAG, "ui_alarm list %d records (ui_home.bin bg)", ALARM_DEMO_COUNT);
}

void ui_alarm_show(const ui_alarm_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }
    lv_screen_load(ui->screen);
}

void ui_alarm_destroy(ui_alarm_t *ui)
{
    if (!ui) {
        return;
    }
    if (ui->screen) {
        lv_obj_delete(ui->screen);
        ui->screen = NULL;
    }
}
