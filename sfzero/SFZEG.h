/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#ifndef SFZEG_H_INCLUDED
#define SFZEG_H_INCLUDED

#include "SFZRegion.h"

namespace sfzero
{
class EG
{
public:
  EG();
  virtual ~EG() {}

  void setExponentialDecay(bool newExponentialDecay);
  void startNote(const EGParameters *parameters, float floatVelocity, double sampleRate, const EGParameters *velMod = nullptr);
  void nextSegment();
  void noteOff();
  void fastRelease();
  bool isDone() const noexcept { return (segment_ == Done); }
  bool isReleasing() const noexcept { return (segment_ == Release); }
  int segmentIndex() const noexcept { return static_cast<int>(segment_); }
  float getLevel() const noexcept { return level_; }
  void setLevel(float v) noexcept { level_ = v; }
  float getSlope() const noexcept { return slope_; }
  void setSlope(float v) noexcept { slope_ = v; }
  int getSamplesUntilNextSegment() const noexcept { return samplesUntilNextSegment_; }
  void setSamplesUntilNextSegment(int v) noexcept { samplesUntilNextSegment_ = v; }
  bool getSegmentIsExponential() const noexcept { return segmentIsExponential_; }
  void setSegmentIsExponential(bool v) noexcept { segmentIsExponential_ = v; }

  /** Returns a stable, lowercase string for the current envelope segment.
      Used by infoString() for debug rendering. */
  const char *segmentName() const noexcept;

private:
  enum Segment
  {
    Delay,
    Attack,
    Hold,
    Decay,
    Sustain,
    Release,
    Done
  };

  void startDelay();
  void startAttack();
  void startHold();
  void startDecay();
  void startSustain();
  void startRelease();

  Segment segment_;
  EGParameters parameters_;
  double sampleRate_;
  bool exponentialDecay_;
  float level_;
  float slope_;
  int samplesUntilNextSegment_;
  bool segmentIsExponential_;
  static const float BottomLevel;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EG)
};
}

#endif // SFZEG_H_INCLUDED
