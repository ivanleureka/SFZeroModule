/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#ifndef SFZVOICE_H_INCLUDED
#define SFZVOICE_H_INCLUDED

#include "SFZEG.h"
#include <memory>

namespace sfzero
{
struct Region;

class Voice : public juce::SynthesiserVoice
{
public:
  Voice();
  virtual ~Voice() override;

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
  void setRegion(Region *nextRegion);

  juce::String infoString();

private:
  Region *region_;
  int trigger_;
  int curMidiNote_, curPitchWheel_;
  double pitchRatio_;
  float noteGainLeft_, noteGainRight_;
  double sourceSamplePosition_;
  EG ampeg_;
  juce::int64 sampleEnd_;
  juce::int64 loopStart_, loopEnd_;
  std::shared_ptr<juce::AudioSampleBuffer> bufferKeepAlive_;
  const float *inL_;
  const float *inR_;
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

  // Info only.
  int numLoops_;
  int curVelocity_;

  void calcPitchRatio();
  void killNote();
  double fractionalMidiNoteInHz(double note, double freqOfA = 440.0);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Voice)
};
}

#endif // SFZVOICE_H_INCLUDED
