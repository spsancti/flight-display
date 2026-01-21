#include "CO5300.h"

#ifndef CO5300_REQUIRE_2X2_UPDATES
#define CO5300_REQUIRE_2X2_UPDATES 1
#endif
#ifndef CO5300_THIN_STROKES
#define CO5300_THIN_STROKES 1
#endif
#ifndef CO5300_STROKE_BG_FOLLOW_FILL
#define CO5300_STROKE_BG_FOLLOW_FILL 1
#endif

namespace {

static void alignRect2x2(int16_t &x, int16_t &y, int16_t &w, int16_t &h, int16_t maxW, int16_t maxH) {
    if (w <= 0 || h <= 0 || maxW < 2 || maxH < 2) return;

    int16_t x0 = x;
    int16_t y0 = y;
    int16_t x1 = x + w;
    int16_t y1 = y + h;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;

    if (x0 & 1) x0 -= 1;
    if (y0 & 1) y0 -= 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;

    if (x1 > maxW) x1 = maxW;
    if (y1 > maxH) y1 = maxH;

    if (x1 & 1) {
        if (x1 < maxW) x1 += 1;
        else x1 -= 1;
    }
    if (y1 & 1) {
        if (y1 < maxH) y1 += 1;
        else y1 -= 1;
    }

    int16_t newW = x1 - x0;
    int16_t newH = y1 - y0;
    if (newW < 2) {
        newW = 2;
        if (x0 + newW > maxW) x0 = maxW - newW;
        if (x0 < 0) x0 = 0;
    }
    if (newH < 2) {
        newH = 2;
        if (y0 + newH > maxH) y0 = maxH - newH;
        if (y0 < 0) y0 = 0;
    }

    x = x0;
    y = y0;
    w = newW;
    h = newH;
}

}  // namespace

CO5300::CO5300(Arduino_DataBus *bus, int8_t rst, uint8_t r, bool ips, int16_t w, int16_t h, uint8_t col_offset1,
               uint8_t row_offset1, uint8_t col_offset2, uint8_t row_offset2, uint8_t color_order)
    : Arduino_CO5300(bus, rst, r, ips, w, h, col_offset1, row_offset1, col_offset2, row_offset2),
      _color_order(color_order),
      _stroke_bg_color(BLACK) {}

void CO5300::setRotation(uint8_t r) {
    Arduino_TFT::setRotation(r);
    switch (_rotation) {
    case 1:
        r = _color_order | 0x60;
        break;
    case 2:
        r = _color_order | 0xC0;
        break;
    case 3:
        r = _color_order | 0xA0;
        break;
    default: // case 0:
        r = _color_order;
        break;
    }
    _bus->beginWrite();
    _bus->writeC8D8(CO5300_W_MADCTL, r);
    _bus->endWrite();
}
