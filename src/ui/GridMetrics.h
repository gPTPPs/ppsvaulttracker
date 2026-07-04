#pragma once

// Shared horizontal metrics: the pattern grid and the mixer strips align
// column-for-column (same stride, same left offset for the row numbers).
namespace GridMetrics
{
    inline constexpr int kRowNumW = 42;
    inline constexpr int kChanW   = 134;
    inline constexpr int kChanGap = 10;
    inline constexpr int kStride  = kChanW + kChanGap;
}
