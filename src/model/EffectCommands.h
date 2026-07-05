#pragma once
#include <cstdint>

// Effect-column commands — native MIDI by design (no internal DSP effects).
// Cell::effect stores a small enum; letters are a display/input concern only.
//
//   Axx..Hxx  send the CC mapped to that per-track slot (Song::ccSlots), value 00..7F
//   Pxx       pitch bend, 40 = centre (7-bit coarse -> 14-bit MSB)
//   Nxx       note delay: the cell triggers x ticks late (FT2 EDx)
//   Kxx       note cut/kill: the playing note stops at tick x (FT2 ECx)
//
// N (delay) and K (kill) deliberately avoid the letters C and D, which are
// CC slot names.
namespace FxCmd
{
    enum : uint8_t
    {
        kNone      = 0,
        kSlotA     = 1,    // ..kSlotA+7 = slot H
        kSlotH     = 8,
        kPitchBend = 9,
        kNoteDelay = 10,
        kNoteCut   = 11,
        kNumCommands
    };

    constexpr int kNumSlots = 8;   // CC slots A..H per track

    constexpr bool isSlot (uint8_t fx)    { return fx >= kSlotA && fx <= kSlotH; }
    constexpr int  slotIndex (uint8_t fx) { return isSlot (fx) ? fx - kSlotA : -1; }

    // sanitise an effect byte read from untrusted storage
    constexpr uint8_t sanitize (uint8_t fx) { return fx < kNumCommands ? fx : (uint8_t) kNone; }

    // display letter, 0 for none
    constexpr char letter (uint8_t fx)
    {
        if (isSlot (fx))          return (char) ('A' + fx - kSlotA);
        if (fx == kPitchBend)     return 'P';
        if (fx == kNoteDelay)     return 'N';
        if (fx == kNoteCut)       return 'K';
        return 0;
    }

    // typed character -> command ('.' or '0' clears), -1 = not a command key
    constexpr int fromChar (char c)
    {
        if (c >= 'a' && c <= 'z') c = (char) (c - 'a' + 'A');
        if (c >= 'A' && c <= 'H') return kSlotA + (c - 'A');
        if (c == 'P')             return kPitchBend;
        if (c == 'N')             return kNoteDelay;
        if (c == 'K')             return kNoteCut;
        if (c == '.' || c == '0') return kNone;
        return -1;
    }

    // human name for the common MIDI controllers offered as slot defaults
    constexpr const char* ccName (int cc)
    {
        switch (cc)
        {
            case 1:   return "Mod Wheel";
            case 7:   return "Volume";
            case 10:  return "Pan";
            case 11:  return "Expression";
            case 64:  return "Sustain";
            case 71:  return "Resonance";
            case 74:  return "Cutoff";
            case 91:  return "Reverb";
            case 93:  return "Chorus";
            default:  return nullptr;
        }
    }
}
