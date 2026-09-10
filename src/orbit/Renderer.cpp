#include "Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

#include <cairo/cairo.h>

namespace Orbit {

namespace {

constexpr double PI_VALUE = 3.14159265358979323846;

bool validImage(const RGBAImage& image) {
    return image.width > 0 &&
           image.height > 0 &&
           image.pixels.size() ==
               static_cast<size_t>(image.width) *
               static_cast<size_t>(image.height) *
               4;
}

} // namespace

bool loadPNG(const std::string& path, RGBAImage& out) {

    cairo_surface_t* surface =
        cairo_image_surface_create_from_png(path.c_str());

    if (!surface)
        return false;

    const cairo_status_t status =
        cairo_surface_status(surface);

    if (status != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return false;
    }

    cairo_surface_flush(surface);

    const int width =
        cairo_image_surface_get_width(surface);

    const int height =
        cairo_image_surface_get_height(surface);

    const int stride =
        cairo_image_surface_get_stride(surface);

    const unsigned char* data =
        cairo_image_surface_get_data(surface);

    if (!data || width <= 0 || height <= 0) {
        cairo_surface_destroy(surface);
        return false;
    }

    out.width = width;
    out.height = height;

    out.pixels.resize(
        static_cast<size_t>(width) *
        static_cast<size_t>(height) *
        4
    );

    /*
     * Cairo's ARGB32 is native-endian.
     * On our x86_64 machine this is BGRA in memory.
     *
     * Orbit internally uses RGBA.
     */
    for (int y = 0; y < height; ++y) {

        const unsigned char* src =
            data + y * stride;

        unsigned char* dst =
            out.pixels.data() +
            static_cast<size_t>(y) *
            static_cast<size_t>(width) *
            4;

        for (int x = 0; x < width; ++x) {

            const unsigned char b = src[x * 4 + 0];
            const unsigned char g = src[x * 4 + 1];
            const unsigned char r = src[x * 4 + 2];
            const unsigned char a = src[x * 4 + 3];

            dst[x * 4 + 0] = r;
            dst[x * 4 + 1] = g;
            dst[x * 4 + 2] = b;
            dst[x * 4 + 3] = a;
        }
    }

    cairo_surface_destroy(surface);

    return true;
}


bool makeCircularCard(
    const RGBAImage& source,
    int diameter,
    Card& out
) {

    if (!validImage(source))
        return false;

    if (diameter <= 0)
        return false;

    /*
     * Create a Cairo surface containing the circular crop.
     */
    cairo_surface_t* surface =
        cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32,
            diameter,
            diameter
        );

    if (!surface)
        return false;

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return false;
    }

    cairo_t* cr =
        cairo_create(surface);

    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return false;
    }

    /*
     * Transparent background.
     */
    cairo_set_operator(
        cr,
        CAIRO_OPERATOR_CLEAR
    );

    cairo_paint(cr);

    cairo_set_operator(
        cr,
        CAIRO_OPERATOR_OVER
    );

    /*
     * Circular clipping region.
     */
    const double radius =
        static_cast<double>(diameter) / 2.0;

    cairo_arc(
        cr,
        radius,
        radius,
        radius,
        0.0,
        2.0 * PI_VALUE
    );

    cairo_clip(cr);

    /*
     * Preserve the screenshot aspect ratio.

     * We use a "cover" crop:
     *
     *   screenshot
     *       ↓
     *   scale until card is filled
     *       ↓
     *   crop excess
     */
    const double sx =
        static_cast<double>(diameter) /
        static_cast<double>(source.width);

    const double sy =
        static_cast<double>(diameter) /
        static_cast<double>(source.height);

    const double scale =
        std::max(sx, sy);

    const double scaledWidth =
        static_cast<double>(source.width) * scale;

    const double scaledHeight =
        static_cast<double>(source.height) * scale;

    const double offsetX =
        (diameter - scaledWidth) / 2.0;

    const double offsetY =
        (diameter - scaledHeight) / 2.0;

    /*
     * Build a temporary Cairo surface
     * containing the screenshot.
     */
    cairo_surface_t* screenshot =
        cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32,
            source.width,
            source.height
        );

    if (!screenshot) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return false;
    }

    unsigned char* screenshotData =
        cairo_image_surface_get_data(screenshot);

    const int screenshotStride =
        cairo_image_surface_get_stride(screenshot);

    /*
     * Convert RGBA -> Cairo native ARGB32/BGRA.
     */
    for (int y = 0; y < source.height; ++y) {

        unsigned char* dst =
            screenshotData +
            y * screenshotStride;

        const unsigned char* src =
            source.pixels.data() +
            static_cast<size_t>(y) *
            static_cast<size_t>(source.width) *
            4;

        for (int x = 0; x < source.width; ++x) {

            const unsigned char r = src[x * 4 + 0];
            const unsigned char g = src[x * 4 + 1];
            const unsigned char b = src[x * 4 + 2];
            const unsigned char a = src[x * 4 + 3];

            dst[x * 4 + 0] = b;
            dst[x * 4 + 1] = g;
            dst[x * 4 + 2] = r;
            dst[x * 4 + 3] = a;
        }
    }

    cairo_surface_mark_dirty(screenshot);

    /*
     * Draw screenshot into the circular clip.
     */
    cairo_save(cr);

    cairo_translate(
        cr,
        offsetX,
        offsetY
    );

    cairo_scale(
        cr,
        scale,
        scale
    );

    cairo_set_source_surface(
        cr,
        screenshot,
        0.0,
        0.0
    );

    cairo_paint(cr);

    cairo_restore(cr);

    /*
     * Add a subtle outer ring.
     *
     * This is intentionally part of the card itself.
     * Later the actual Orbit renderer can add selection
     * effects without touching the screenshot.
     */
    cairo_new_path(cr);

    cairo_arc(
        cr,
        radius,
        radius,
        radius - 2.0,
        0.0,
        2.0 * PI_VALUE
    );

    cairo_set_source_rgba(
        cr,
        1.0,
        1.0,
        1.0,
        0.18
    );

    cairo_set_line_width(
        cr,
        4.0
    );

    cairo_stroke(cr);

    cairo_destroy(cr);

    cairo_surface_flush(surface);

    /*
     * Copy result into Orbit's RGBA buffer.
     */
    const unsigned char* result =
        cairo_image_surface_get_data(surface);

    const int resultStride =
        cairo_image_surface_get_stride(surface);

    out.image.width = diameter;
    out.image.height = diameter;

    out.image.pixels.resize(
        static_cast<size_t>(diameter) *
        static_cast<size_t>(diameter) *
        4
    );

    for (int y = 0; y < diameter; ++y) {

        const unsigned char* src =
            result + y * resultStride;

        unsigned char* dst =
            out.image.pixels.data() +
            static_cast<size_t>(y) *
            static_cast<size_t>(diameter) *
            4;

        for (int x = 0; x < diameter; ++x) {

            const unsigned char b = src[x * 4 + 0];
            const unsigned char g = src[x * 4 + 1];
            const unsigned char r = src[x * 4 + 2];
            const unsigned char a = src[x * 4 + 3];

            dst[x * 4 + 0] = r;
            dst[x * 4 + 1] = g;
            dst[x * 4 + 2] = b;
            dst[x * 4 + 3] = a;
        }
    }

    out.radius =
        static_cast<float>(diameter) / 2.0f;

    cairo_surface_destroy(screenshot);
    cairo_surface_destroy(surface);

    return true;
}


bool savePNG(
    const RGBAImage& image,
    const std::string& path
) {

    if (!validImage(image))
        return false;

    cairo_surface_t* surface =
        cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32,
            image.width,
            image.height
        );

    if (!surface)
        return false;

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return false;
    }

    unsigned char* dst =
        cairo_image_surface_get_data(surface);

    const int stride =
        cairo_image_surface_get_stride(surface);

    for (int y = 0; y < image.height; ++y) {

        const unsigned char* src =
            image.pixels.data() +
            static_cast<size_t>(y) *
            static_cast<size_t>(image.width) *
            4;

        unsigned char* row =
            dst + y * stride;

        for (int x = 0; x < image.width; ++x) {

            const unsigned char r = src[x * 4 + 0];
            const unsigned char g = src[x * 4 + 1];
            const unsigned char b = src[x * 4 + 2];
            const unsigned char a = src[x * 4 + 3];

            row[x * 4 + 0] = b;
            row[x * 4 + 1] = g;
            row[x * 4 + 2] = r;
            row[x * 4 + 3] = a;
        }
    }

    cairo_surface_mark_dirty(surface);

    const cairo_status_t status =
        cairo_surface_write_to_png(
            surface,
            path.c_str()
        );

    cairo_surface_destroy(surface);

    return status == CAIRO_STATUS_SUCCESS;
}

bool renderOverlay(
    const std::vector<Card>& cards,
    const OverlayConfig& config,
    RGBAImage& out
) {

    if (config.screenWidth <= 0 || config.screenHeight <= 0)
        return false;

    const int w = config.screenWidth;
    const int h = config.screenHeight;

    cairo_surface_t* surface =
        cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32,
            w,
            h
        );

    if (!surface)
        return false;

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return false;
    }

    cairo_t* cr = cairo_create(surface);

    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return false;
    }

    /*
     * Transparent background.
     */
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /*
     * Semi-transparent dark background.
     */
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.55);
    cairo_fill(cr);

    /*
     * Center point.
     */
    const double cx =
        config.centerX > 0.0f
            ? static_cast<double>(config.centerX)
            : static_cast<double>(w) / 2.0;

    const double cy =
        config.centerY > 0.0f
            ? static_cast<double>(config.centerY)
            : static_cast<double>(h) / 2.0;

    const double orbitR =
        static_cast<double>(config.orbitRadius);

    const double cardR =
        static_cast<double>(config.cardRadius);

    const std::size_t count = cards.size();

    if (count == 0) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return false;
    }

    /*
     * Draw connecting ring.
     */
    if (count > 1) {
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, orbitR, 0.0, 2.0 * PI_VALUE);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
    }

    /*
     * Draw each workspace card.
     */
    constexpr double START_ANGLE = -PI_VALUE / 2.0;

    const double angleStep =
        (2.0 * PI_VALUE) / static_cast<double>(count);

    for (std::size_t i = 0; i < count; ++i) {

        const auto& card = cards[i];

        const double angle =
            START_ANGLE + angleStep * static_cast<double>(i);

        const bool isActive =
            (static_cast<int>(i) == config.activeIndex);

        const double drawRadius =
            isActive ? cardR * 1.15 : cardR * 0.85;

        const double drawX =
            cx + std::cos(angle) * orbitR - drawRadius;

        const double drawY =
            cy + std::sin(angle) * orbitR - drawRadius;

        const double drawSize = drawRadius * 2.0;

        /*
         * Build a temporary Cairo surface from the card image.
         */
        if (card.image.width > 0 && card.image.height > 0 &&
            !card.image.pixels.empty()) {

            cairo_surface_t* cardSurface =
                cairo_image_surface_create(
                    CAIRO_FORMAT_ARGB32,
                    card.image.width,
                    card.image.height
                );

            if (!cardSurface)
                continue;

            if (cairo_surface_status(cardSurface) !=
                CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy(cardSurface);
                continue;
            }

            unsigned char* dst =
                cairo_image_surface_get_data(cardSurface);

            const int dstStride =
                cairo_image_surface_get_stride(cardSurface);

            /*
             * Convert RGBA -> Cairo BGRA.
             */
            for (int y = 0; y < card.image.height; ++y) {

                const unsigned char* src =
                    card.image.pixels.data() +
                    static_cast<size_t>(y) *
                    static_cast<size_t>(card.image.width) * 4;

                unsigned char* row = dst + y * dstStride;

                for (int x = 0; x < card.image.width; ++x) {
                    row[x * 4 + 0] = src[x * 4 + 2];
                    row[x * 4 + 1] = src[x * 4 + 1];
                    row[x * 4 + 2] = src[x * 4 + 0];
                    row[x * 4 + 3] = src[x * 4 + 3];
                }
            }

            cairo_surface_mark_dirty(cardSurface);

            /*
             * Clip to circle.
             */
            cairo_save(cr);

            cairo_new_path(cr);
            cairo_arc(
                cr,
                drawX + drawRadius,
                drawY + drawRadius,
                drawRadius,
                0.0,
                2.0 * PI_VALUE
            );
            cairo_clip(cr);

            /*
             * Draw the card image scaled to fit.
             */
            cairo_set_source_surface(
                cr,
                cardSurface,
                drawX,
                drawY
            );

            cairo_pattern_set_filter(
                cairo_get_source(cr),
                CAIRO_FILTER_BILINEAR
            );

            cairo_rectangle(
                cr,
                drawX,
                drawY,
                drawSize,
                drawSize
            );
            cairo_fill(cr);

            cairo_restore(cr);

            cairo_surface_destroy(cardSurface);
        }

        /*
         * Active card glow ring.
         */
        if (isActive) {
            cairo_new_path(cr);
            cairo_arc(
                cr,
                drawX + drawRadius,
                drawY + drawRadius,
                drawRadius + 3.0,
                0.0,
                2.0 * PI_VALUE
            );
            cairo_set_source_rgba(
                cr,
                0.4,
                0.7,
                1.0,
                0.8
            );
            cairo_set_line_width(cr, 3.0);
            cairo_stroke(cr);
        }

        /*
         * Outer ring for all cards.
         */
        cairo_new_path(cr);
        cairo_arc(
            cr,
            drawX + drawRadius,
            drawY + drawRadius,
            drawRadius - 1.0,
            0.0,
            2.0 * PI_VALUE
        );
        cairo_set_source_rgba(
            cr,
            1.0,
            1.0,
            1.0,
            isActive ? 0.35 : 0.15
        );
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
    }

    /*
     * Center label.
     */
    if (config.activeIndex >= 0 &&
        static_cast<std::size_t>(config.activeIndex) < count) {

        /*
         * Draw workspace number in center.
         */
        cairo_select_font_face(
            cr,
            "monospace",
            CAIRO_FONT_SLANT_NORMAL,
            CAIRO_FONT_WEIGHT_BOLD
        );

        cairo_set_font_size(cr, 28.0);

        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);

        char buf[64];
        snprintf(
            buf,
            sizeof(buf),
            "WS %d",
            config.activeIndex + 1
        );

        cairo_text_extents_t extents;
        cairo_text_extents(cr, buf, &extents);

        cairo_move_to(
            cr,
            cx - extents.width / 2.0,
            cy + extents.height / 2.0
        );

        cairo_show_text(cr, buf);
    }

    /*
     * Copy result to RGBA buffer.
     */
    cairo_surface_flush(surface);

    const unsigned char* result =
        cairo_image_surface_get_data(surface);

    const int resultStride =
        cairo_image_surface_get_stride(surface);

    out.width = w;
    out.height = h;

    out.pixels.resize(
        static_cast<size_t>(w) *
        static_cast<size_t>(h) * 4
    );

    for (int y = 0; y < h; ++y) {

        const unsigned char* src =
            result + y * resultStride;

        unsigned char* dst =
            out.pixels.data() +
            static_cast<size_t>(y) *
            static_cast<size_t>(w) * 4;

        for (int x = 0; x < w; ++x) {
            dst[x * 4 + 0] = src[x * 4 + 2];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = src[x * 4 + 3];
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return true;
}

} // namespace Orbit
