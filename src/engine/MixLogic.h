#pragma once

// Solo/mute resolution — pure logic, unit-tested.
// If any channel is soloed, everything not soloed is effectively muted;
// an explicit mute always wins.
inline void computeEffectiveMutes (const bool* mute, const bool* solo, int n, bool* out)
{
    bool anySolo = false;
    for (int i = 0; i < n; ++i)
        anySolo = anySolo || solo[i];

    for (int i = 0; i < n; ++i)
        out[i] = anySolo ? (! solo[i] || mute[i]) : mute[i];
}
