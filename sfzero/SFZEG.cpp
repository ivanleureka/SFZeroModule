/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SFZEG.h"
#include <algorithm>
#include <cmath>
#include <limits>

static const float fastReleaseTime = 0.01f;

namespace
{
// Convert (seconds * sampleRate) into a non-negative int, capped at INT_MAX/2
// so callers can decrement / divide the result without UB on pathological SF2
// timecents. Returns 0 for non-positive inputs.
inline int timeToSamples(double seconds, double sampleRate)
{
  if (!(seconds > 0.0) || !(sampleRate > 0.0))
  {
    return 0;
  }
  double samples = seconds * sampleRate;
  constexpr double kMaxSamples = static_cast<double>(std::numeric_limits<int>::max()) / 2.0;
  if (samples > kMaxSamples)
  {
    return static_cast<int>(kMaxSamples);
  }
  return static_cast<int>(samples);
}
}

sfzero::EG::EG()
    : segment_(), sampleRate_(0), exponentialDecay_(false), level_(0), slope_(0), samplesUntilNextSegment_(0), segmentIsExponential_(false)
{
}

void sfzero::EG::setExponentialDecay(bool newExponentialDecay) { exponentialDecay_ = newExponentialDecay; }

void sfzero::EG::startNote(const EGParameters *newParameters, float floatVelocity, double newSampleRate,
                           const EGParameters *velMod)
{
  parameters_ = *newParameters;
  if (velMod)
  {
    parameters_.delay += floatVelocity * velMod->delay;
    parameters_.attack += floatVelocity * velMod->attack;
    parameters_.hold += floatVelocity * velMod->hold;
    parameters_.decay += floatVelocity * velMod->decay;
    parameters_.sustain += floatVelocity * velMod->sustain;
    if (parameters_.sustain < 0.0)
    {
      parameters_.sustain = 0.0;
    }
    else if (parameters_.sustain > 100.0)
    {
      parameters_.sustain = 100.0;
    }
    parameters_.release += floatVelocity * velMod->release;
  }
  sampleRate_ = newSampleRate;

  startDelay();
}

void sfzero::EG::nextSegment()
{
  switch (segment_)
  {
  case Delay:
    startAttack();
    break;

  case Attack:
    startHold();
    break;

  case Hold:
    startDecay();
    break;

  case Decay:
    startSustain();
    break;

  case Sustain:
    // Shouldn't be called.
    break;

  case Release:
  default:
    segment_ = Done;
    break;
  }
}

void sfzero::EG::noteOff() { startRelease(); }

void sfzero::EG::fastRelease()
{
  segment_ = Release;
  samplesUntilNextSegment_ = timeToSamples(fastReleaseTime, sampleRate_);
  slope_ = -level_ / samplesUntilNextSegment_;
  segmentIsExponential_ = false;
}

void sfzero::EG::startDelay()
{
  if (parameters_.delay <= 0)
  {
    startAttack();
  }
  else
  {
    segment_ = Delay;
    level_ = 0.0;
    slope_ = 0.0;
    samplesUntilNextSegment_ = timeToSamples(parameters_.delay, sampleRate_);
    segmentIsExponential_ = false;
  }
}

void sfzero::EG::startAttack()
{
  if (parameters_.attack <= 0)
  {
    startHold();
  }
  else
  {
    segment_ = Attack;
    level_ = parameters_.start / 100.0f;
    samplesUntilNextSegment_ = timeToSamples(parameters_.attack, sampleRate_);
    slope_ = 1.0f / samplesUntilNextSegment_;
    segmentIsExponential_ = false;
  }
}

void sfzero::EG::startHold()
{
  if (parameters_.hold <= 0)
  {
    level_ = 1.0;
    startDecay();
  }
  else
  {
    segment_ = Hold;
    samplesUntilNextSegment_ = timeToSamples(parameters_.hold, sampleRate_);
    level_ = 1.0;
    slope_ = 0.0;
    segmentIsExponential_ = false;
  }
}

void sfzero::EG::startDecay()
{
  if (parameters_.decay <= 0)
  {
    startSustain();
  }
  else
  {
    segment_ = Decay;
    samplesUntilNextSegment_ = timeToSamples(parameters_.decay, sampleRate_);
    level_ = 1.0;
    if (exponentialDecay_)
    {
      // I don't truly understand this; just following what LinuxSampler does.
      float mysterySlope = -9.226f / samplesUntilNextSegment_;
      slope_ = exp(mysterySlope);
      segmentIsExponential_ = true;
      if (parameters_.sustain > 0.0)
      {
        // Again, this is following LinuxSampler's example, which is similar to
        // SF2-style decay, where "decay" specifies the time it would take to
        // get to zero, not to the sustain level.  The SFZ spec is not that
        // specific about what "decay" means, so perhaps it's really supposed
        // to specify the time to reach the sustain level.
        // Clamp inputs so log() can't produce NaN/-Inf when level_ is at or
        // near zero or sustain is pathological.
        constexpr double kEpsilon = 1.0e-6;
        double numerator = std::max(parameters_.sustain / 100.0, kEpsilon);
        double denominator = std::max(static_cast<double>(level_), kEpsilon);
        double samplesD = std::log(numerator / denominator) / mysterySlope;
        if (!std::isfinite(samplesD) || samplesD <= 0.0)
        {
          startSustain();
        }
        else
        {
          constexpr double kMaxSamples = static_cast<double>(std::numeric_limits<int>::max()) / 2.0;
          samplesUntilNextSegment_ = static_cast<int>(std::min(samplesD, kMaxSamples));
          if (samplesUntilNextSegment_ <= 0)
          {
            startSustain();
          }
        }
      }
    }
    else
    {
      slope_ = (parameters_.sustain / 100.0f - 1.0f) / samplesUntilNextSegment_;
      segmentIsExponential_ = false;
    }
  }
}

void sfzero::EG::startSustain()
{
  if (parameters_.sustain <= 0)
  {
    startRelease();
  }
  else
  {
    segment_ = Sustain;
    level_ = parameters_.sustain / 100.0f;
    slope_ = 0.0;
    samplesUntilNextSegment_ = 0x7FFFFFFF;
    segmentIsExponential_ = false;
  }
}

void sfzero::EG::startRelease()
{
  float release = parameters_.release;

  if (release <= 0)
  {
    // Enforce a short release, to prevent clicks.
    release = fastReleaseTime;
  }

  segment_ = Release;
  samplesUntilNextSegment_ = timeToSamples(release, sampleRate_);
  if (exponentialDecay_)
  {
    // I don't truly understand this; just following what LinuxSampler does.
    float mysterySlope = -9.226f / samplesUntilNextSegment_;
    slope_ = exp(mysterySlope);
    segmentIsExponential_ = true;
  }
  else
  {
    slope_ = -level_ / samplesUntilNextSegment_;
    segmentIsExponential_ = false;
  }
}

const float sfzero::EG::BottomLevel = 0.001f;

const char *sfzero::EG::segmentName() const noexcept
{
  switch (segment_)
  {
  case Delay:   return "delay";
  case Attack:  return "attack";
  case Hold:    return "hold";
  case Decay:   return "decay";
  case Sustain: return "sustain";
  case Release: return "release";
  case Done:    return "done";
  }
  return "-Invalid-";
}
