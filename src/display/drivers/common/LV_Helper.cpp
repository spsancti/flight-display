/**
 * @file      LV_Helper.cpp
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @date      2024-01-22
 *
 */
#include "LV_Helper.h"
#include "log.h"

#if LV_VERSION_CHECK(9, 0, 0)
#error "LVGL 9.x not supported"
#endif

static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;
static lv_color_t *buf = nullptr;
static lv_color_t *buf1 = nullptr;

static void rounder_cb(lv_disp_drv_t *disp_drv, lv_area_t *area) {
  if (area->x1 % 2 != 0) area->x1 += 1;
  if (area->y1 % 2 != 0) area->y1 += 1;

  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  if (w % 2 != 0) area->x2 -= 1;
  if (h % 2 != 0) area->y2 -= 1;
}

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  uint16_t w = area->x2 - area->x1 + 1;
  uint16_t h = area->y2 - area->y1 + 1;
  static_cast<Display *>(disp_drv->user_data)->pushColors(area->x1, area->y1, w, h,
                                                         reinterpret_cast<uint16_t *>(color_p));
  lv_disp_flush_ready(disp_drv);
}

static void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  static int16_t x, y;
  uint8_t touched = static_cast<Display *>(indev_driver->user_data)->getPoint(&x, &y, 1);
  if (touched) {
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PR;
    return;
  }
  data->state = LV_INDEV_STATE_REL;
}

#if LV_USE_LOG
static void lv_log_print_g_cb(const char *buf) {
  LOG_INFO("%s", buf);
}
#endif

String lvgl_helper_get_fs_filename(String filename) {
  static String path;
  path = String("A") + ":" + (filename);
  return path;
}

const char *lvgl_helper_get_fs_filename(const char *filename) {
  static String path;
  path = String("A") + ":" + String(filename);
  return path.c_str();
}

void beginLvglHelper(Display &board, bool debug) {
  lv_init();

#if LV_USE_LOG
  if (debug) {
    lv_log_register_print_cb(lv_log_print_g_cb);
  }
#endif

  size_t lv_buffer_size = board.width() * board.height() * sizeof(lv_color_t);
  buf = reinterpret_cast<lv_color_t *>(ps_malloc(lv_buffer_size));
  buf1 = reinterpret_cast<lv_color_t *>(ps_malloc(lv_buffer_size));
  if (!buf || !buf1) {
    LOG_ERROR("LVGL buffer allocation failed");
    return;
  }

  lv_disp_draw_buf_init(&draw_buf, buf, buf1, board.width() * board.height());

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = board.width();
  disp_drv.ver_res = board.height();
  disp_drv.flush_cb = disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.full_refresh = 0;
  disp_drv.direct_mode = 1;
  disp_drv.rounder_cb = rounder_cb;
  disp_drv.user_data = &board;
  lv_disp_drv_register(&disp_drv);

  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  indev_drv.user_data = &board;
  lv_indev_drv_register(&indev_drv);
}
