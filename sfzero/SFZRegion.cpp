/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SFZRegion.h"
#include "SFZSample.h"

void sfzero::EGParameters::clear()
{
  *this = EGParameters{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 100.0f, 0.0f};
}

void sfzero::EGParameters::clearMod()
{
  *this = EGParameters{};
}

void sfzero::Region::clear()
{
  *this = Region{};
}

void sfzero::Region::clearForSF2()
{
  clear();
  pitch_keycenter = -1;
  loop_mode = no_loop;

  // SF2 defaults in timecents.
  ampeg.delay = -12000.0;
  ampeg.attack = -12000.0;
  ampeg.hold = -12000.0;
  ampeg.decay = -12000.0;
  ampeg.sustain = 0.0;
  ampeg.release = -12000.0;

  // Mod-env SF2 defaults in timecents (same convention as ampeg).
  modeg.delay = -12000.0f;
  modeg.attack = -12000.0f;
  modeg.hold = -12000.0f;
  modeg.decay = -12000.0f;
  modeg.sustain = 0.0f;
  modeg.release = -12000.0f;
}

void sfzero::Region::clearForRelativeSF2()
{
  clear();
  pitch_keytrack = 0;
  amp_veltrack = 0.0;
  ampeg.sustain = 0.0;

  // Phase C - relative (preset-zone) values are additive offsets, so they must
  // start at zero rather than the absolute SF2 defaults set by clearForSF2().
  initialFilterFc = 0.0f;
  delayVibLFO = 0.0f;
  modeg.delay = 0.0f;
  modeg.attack = 0.0f;
  modeg.hold = 0.0f;
  modeg.decay = 0.0f;
  modeg.sustain = 0.0f;
  modeg.release = 0.0f;
}

void sfzero::Region::addForSF2(sfzero::Region *other)
{
  offset += other->offset;
  end += other->end;
  loop_start += other->loop_start;
  loop_end += other->loop_end;
  transpose += other->transpose;
  tune += other->tune;
  pitch_keytrack += other->pitch_keytrack;
  volume += other->volume;
  pan += other->pan;

  ampeg.delay += other->ampeg.delay;
  ampeg.attack += other->ampeg.attack;
  ampeg.hold += other->ampeg.hold;
  ampeg.decay += other->ampeg.decay;
  ampeg.sustain += other->ampeg.sustain;
  ampeg.release += other->ampeg.release;

  // Phase C - sum filter / mod-env / vibrato-LFO fields just like ampeg.
  initialFilterFc += other->initialFilterFc;
  initialFilterQ += other->initialFilterQ;
  modEnvToFilterFc += other->modEnvToFilterFc;
  modEnvToPitch += other->modEnvToPitch;
  modeg.delay += other->modeg.delay;
  modeg.attack += other->modeg.attack;
  modeg.hold += other->modeg.hold;
  modeg.decay += other->modeg.decay;
  modeg.sustain += other->modeg.sustain;
  modeg.release += other->modeg.release;
  delayVibLFO += other->delayVibLFO;
  freqVibLFO += other->freqVibLFO;
  vibLfoToPitch += other->vibLfoToPitch;
}

void sfzero::Region::sf2ToSFZ()
{
  // EG times need to be converted from timecents to seconds.
  ampeg.delay = timecents2Secs(static_cast<int>(ampeg.delay));
  ampeg.attack = timecents2Secs(static_cast<int>(ampeg.attack));
  ampeg.hold = timecents2Secs(static_cast<int>(ampeg.hold));
  ampeg.decay = timecents2Secs(static_cast<int>(ampeg.decay));
  if (ampeg.sustain < 0.0f)
  {
    ampeg.sustain = 0.0f;
  }
  ampeg.sustain = 100.0f * juce::Decibels::decibelsToGain(-ampeg.sustain / 10.0f);
  ampeg.release = timecents2Secs(static_cast<int>(ampeg.release));

  // Pin very short EG segments.  Timecents don't get to zero, and our EG is
  // happier with zero values.
  if (ampeg.delay < 0.01f)
  {
    ampeg.delay = 0.0f;
  }
  if (ampeg.attack < 0.01f)
  {
    ampeg.attack = 0.0f;
  }
  if (ampeg.hold < 0.01f)
  {
    ampeg.hold = 0.0f;
  }
  if (ampeg.decay < 0.01f)
  {
    ampeg.decay = 0.0f;
  }
  if (ampeg.release < 0.01f)
  {
    ampeg.release = 0.0f;
  }

  // Pin values to their ranges.
  if (pan < -100.0f)
  {
    pan = -100.0f;
  }
  else if (pan > 100.0f)
  {
    pan = 100.0f;
  }

  // Phase C - mod envelope: same timecents->seconds conversion as ampeg.
  modeg.delay = timecents2Secs(static_cast<int>(modeg.delay));
  modeg.attack = timecents2Secs(static_cast<int>(modeg.attack));
  modeg.hold = timecents2Secs(static_cast<int>(modeg.hold));
  modeg.decay = timecents2Secs(static_cast<int>(modeg.decay));
  // SF2 sustainModEnv is in 0.1% units (0 = peak, 1000 = zero). The shared EG
  // class consumes sustain as a 0-100 percentage (100 = peak), so convert to
  // match: peak (0) -> 100, zero (1000) -> 0.
  if (modeg.sustain < 0.0f) modeg.sustain = 0.0f;
  if (modeg.sustain > 1000.0f) modeg.sustain = 1000.0f;
  modeg.sustain = 100.0f - (modeg.sustain / 10.0f);
  modeg.release = timecents2Secs(static_cast<int>(modeg.release));

  if (modeg.delay < 0.01f) modeg.delay = 0.0f;
  if (modeg.attack < 0.01f) modeg.attack = 0.0f;
  if (modeg.hold < 0.01f) modeg.hold = 0.0f;
  if (modeg.decay < 0.01f) modeg.decay = 0.0f;
  if (modeg.release < 0.01f) modeg.release = 0.0f;
}

juce::String sfzero::Region::dump()
{
  juce::String info = juce::String::formatted("%d - %d, vel %d - %d", lokey, hikey, lovel, hivel);
  if (sample)
  {
    info << sample->getShortName();
  }
  info << "\n";
  return info;
}

float sfzero::Region::timecents2Secs(int timecents) { return static_cast<float>(pow(2.0, timecents / 1200.0)); }

float sfzero::Region::absoluteCentsToHz(float cents)
{
  // SF2 spec: f = 8.176 * 2^(cents/1200). 0 cents -> 8.176 Hz, 13500 -> ~20 kHz.
  return 8.176f * static_cast<float>(pow(2.0, cents / 1200.0));
}

float sfzero::Region::centibelsToLinear(float cb)
{
  // 200 centibels = 1x gain ratio change in linear amplitude (per SF2 spec,
  // initialFilterQ uses centibels of resonance gain).
  return static_cast<float>(pow(10.0, cb / 200.0));
}
