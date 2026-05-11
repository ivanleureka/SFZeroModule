/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#ifndef SFZREGION_H_INCLUDED
#define SFZREGION_H_INCLUDED

#include "SFZCommon.h"

namespace sfzero
{

class Sample;

// Region is designed to be safely value-copyable.

struct EGParameters
{
  float delay = 0.0f;
  float start = 0.0f;
  float attack = 0.0f;
  float hold = 0.0f;
  float decay = 0.0f;
  float sustain = 0.0f;
  float release = 0.0f;

  void clear();
  void clearMod();
};

struct Region
{
  enum Trigger
  {
    attack,
    release,
    first,
    legato
  };

  enum LoopMode
  {
    sample_loop,
    no_loop,
    one_shot,
    loop_continuous,
    loop_sustain
  };

  enum OffMode
  {
    fast,
    normal
  };

  Region() = default;
  Region(const Region&) = default;
  Region(Region&&) noexcept = default;
  Region& operator=(const Region&) = default;
  Region& operator=(Region&&) noexcept = default;

  void clear();
  void clearForSF2();
  void clearForRelativeSF2();
  void addForSF2(Region *other);
  void sf2ToSFZ();
  juce::String dump();

  bool matches(int note, int velocity, Trigger trig) const noexcept
  {
    return (note >= lokey && note <= hikey && velocity >= lovel && velocity <= hivel &&
            (trig == this->trigger || (this->trigger == attack && (trig == first || trig == legato))));
  }

  Sample *sample = nullptr;
  int lokey = 0, hikey = 127;
  int lovel = 0, hivel = 127;
  Trigger trigger = attack;
  int group = 0;
  juce::int64 off_by = 0;
  OffMode off_mode = fast;

  juce::int64 offset = 0;
  juce::int64 end = 0;
  bool negative_end = false;
  LoopMode loop_mode = sample_loop;
  juce::int64 loop_start = 0, loop_end = 0;
  int transpose = 0;
  int tune = 0;
  int pitch_keycenter = 60, pitch_keytrack = 100;
  int bend_up = 200, bend_down = -200;

  float volume = 0.0f, pan = 0.0f;
  float amp_veltrack = 100.0f;

  EGParameters ampeg{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 100.0f, 0.0f};
  EGParameters ampeg_veltrack{};

  // Phase C - filter (initialFilterFc/Q) + mod-env routing.
  // Defaults: 13500 cents ~= 20 kHz (effectively no filter), 0 cb resonance.
  float initialFilterFc = 13500.0f;     // absolute cents
  float initialFilterQ = 0.0f;          // centibels
  float modEnvToFilterFc = 0.0f;        // cents (full-scale envelope contribution)
  float modEnvToPitch = 0.0f;           // cents

  // Mod envelope (filter sweep / pitch envelope). SF2 timecents at parse time;
  // converted to seconds in sf2ToSFZ() like ampeg.
  EGParameters modeg{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  // Vibrato LFO. SF2 timecents (delay) and absolute cents (freq) at parse time.
  float delayVibLFO = -12000.0f;        // timecents (~1 ms)
  float freqVibLFO = 0.0f;              // absolute cents (0 -> 8.176 Hz)
  float vibLfoToPitch = 0.0f;           // cents (full-scale LFO contribution)

  static float timecents2Secs(int timecents);
  static float absoluteCentsToHz(float cents);
  static float centibelsToLinear(float cb);
};
}

#endif // SFZREGION_H_INCLUDED
