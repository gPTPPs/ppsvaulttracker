#pragma once

// Shared horizontal metrics: the pattern grid and the mixer strips align
// column-for-column (same stride, same left offset for the row numbers).
namespace GridMetrics
{
    inline constexpr int kRowNumW = 42;
    inline constexpr int kChanW   = 134;
    inline constexpr int kChanGap = 10;
    inline constexpr int kStride  = kChanW + kChanGap;

    // mixer strip box: narrower than kChanW on purpose. The grid column's
    // glyphs end around x=128 with no border, so a full-width bordered box
    // below reads as "wider than the column" — this width matches the text
    // block optically and gives strips grid-like breathing room.
    inline constexpr int kStripBoxW = 126;
}
