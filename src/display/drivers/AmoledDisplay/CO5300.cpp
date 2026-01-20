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

void CO5300::writePixelPreclipped(int16_t x, int16_t y, uint16_t color) {
#if CO5300_THIN_STROKES
    int16_t x0 = x & ~1;
    int16_t y0 = y & ~1;
    uint16_t buf[4] = {_stroke_bg_color, _stroke_bg_color, _stroke_bg_color, _stroke_bg_color};
    uint8_t idx = (uint8_t)((y & 1) ? 2 : 0) + (uint8_t)((x & 1) ? 1 : 0);
    buf[idx] = color;
    writeAddrWindow(x0, y0, 2, 2);
    _bus->writePixels(buf, 4);
#elif CO5300_REQUIRE_2X2_UPDATES
    int16_t w = 2;
    int16_t h = 2;
    writeFillRectPreclipped(x, y, w, h, color);
#else
    Arduino_TFT::writePixelPreclipped(x, y, color);
#endif
}

void CO5300::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
#if CO5300_THIN_STROKES
    for (int16_t i = x; i < x + w; ++i) {
        writePixel(i, y, color);
    }
#elif CO5300_REQUIRE_2X2_UPDATES
    writeFillRectPreclipped(x, y, w, 2, color);
#else
    Arduino_TFT::writeFillRectPreclipped(x, y, w, 1, color);
#endif
}

void CO5300::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
#if CO5300_THIN_STROKES
    for (int16_t i = y; i < y + h; ++i) {
        writePixel(x, i, color);
    }
#elif CO5300_REQUIRE_2X2_UPDATES
    writeFillRectPreclipped(x, y, 2, h, color);
#else
    Arduino_TFT::writeFillRectPreclipped(x, y, 1, h, color);
#endif
}

void CO5300::writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
#if CO5300_REQUIRE_2X2_UPDATES
    // This panel requires 2x2-aligned updates to render reliably.
    alignRect2x2(x, y, w, h, _width, _height);
#if CO5300_THIN_STROKES && CO5300_STROKE_BG_FOLLOW_FILL
    _stroke_bg_color = color;
#endif
    Arduino_TFT::writeFillRectPreclipped(x, y, w, h, color);
#else
    Arduino_TFT::writeFillRectPreclipped(x, y, w, h, color);
#endif
}

void CO5300::setStrokeBackground(uint16_t color) {
    _stroke_bg_color = color;
}
