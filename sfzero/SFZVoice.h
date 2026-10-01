/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#ifndef SFZVOICE_H_INCLUDED
#define SFZVOICE_H_INCLUDED

#include "SFZEG.h"
#include <cstdint>
#include <memory>

namespace sfzero
{
struct Region;
class SampleBuffer;

class Voice : public juce::SynthesiserVoice
{
public:
  Voice() noexcept;
  virtual ~Voice() override;

  // Rule of five: copy ops deleted by JUCE_DECLARE_NON_COPYABLE below; delete
  // move ops too (C26432) without re-declaring copy.
  Voice(Voice &&) = delete;
  Voice &operator=(Voice &&) = delete;

  bool canPlaySound(juce::SynthesiserSound *sound) override;
  void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound *sound, int currentPitchWheelPosition) override;
  void stopNote(float velocity, bool allowTailOff) override;
  void stopNoteForGroup();
  void stopNoteQuick();
  void pitchWheelMoved(int newValue) override;
  void controllerMoved(int controllerNumber, int newValue) override;
  void renderNextBlock(juce::AudioSampleBuffer &outputBuffer, int startSample, int numSamples) override;
  bool isPlayingNoteDown() const noexcept;
  bool isPlayingOneShot() const noexcept;

  int getGroup() const noexcept;
  juce::uint64 getOffBy() const noexcept;

  // Set the region to be used by the next startNote().
  void setRegion(Region *nextRegion) noexcept;

  /** True while a hard stop (killNote: voice steal, all-sound-off, pool
      reclaim) is still emitting its short declick fade. The voice is
      otherwise idle (getCurrentlyPlayingNote() < 0). */
  bool hasPendingDeclick() const noexcept { return declickSamples_ > 0; }

  juce::String infoString();

private:
  Region *region_;
  int trigger_;
  int curMidiNote_, curPitchWheel_;
  double pitchRatio_;
  float noteGainLeft_, noteGainRight_;
  double sourceSamplePosition_;
  EG ampeg_;
  juce::int64 sampleStart_, sampleEnd_;
  juce::int64 loopStart_, loopEnd_;
  std::shared_ptr<SampleBuffer> bufferKeepAlive_;
  const std::int16_t *inL_;
  const std::int16_t *inR_;
  int bufferNumSamples_;

  // Phase C - per-voice low-pass biquad. Bypassed when the region requests no
  // filtering (initialFilterFc near max and no mod-env contribution).
  juce::IIRFilter filterL_, filterR_;
  float currentCutoffHz_;
  float currentQ_;
  bool bypassFilter_;

  // Phase C - second envelope routed to filter cutoff and pitch.
  EG modeg_;
  bool modegInUse_;          // any contribution at all (filter or pitch)
  bool modegFilterActive_;   // contributes to filter cutoff specifically
  bool modegPitchActive_;    // contributes to pitch ratio specifically

  // Phase C - vibrato LFO (sine, with onset delay). Phase advances per audio
  // sample; output is sampled at control rate inside the render loop.
  bool vibInUse_;
  float vibPhase_;
  float vibPhaseInc_;
  int vibDelaySamples_;

  // Declick on hard stop. killNote() captures the last output sample and the
  // next renderNextBlock() calls fade it out over kDeclickSamples instead of
  // letting the waveform step to zero (the click heard on voice steals).
  // Stored floats only, so the tail is safe to render after the region /
  // sample buffer have been released.
  float lastOutL_, lastOutR_;
  float declickL_, declickR_;
  int declickSamples_;

  // Info only.
  int numLoops_;
  int curVelocity_;

  void calcPitchRatio();
  void killNote();
  void renderDeclickTail(juce::AudioSampleBuffer &outputBuffer, int startSample, int numSamples) noexcept;
  double fractionalMidiNoteInHz(double note, double freqOfA = 440.0) noexcept;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Voice)
};
}

#endif // SFZVOICE_H_INCLUDED
