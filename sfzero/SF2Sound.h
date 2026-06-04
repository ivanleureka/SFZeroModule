/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *
 * OWNERSHIP MODEL:
 * - presets_: OwnedArray of Preset objects (auto-deleted; each Preset owns its regions).
 * - samplesStorage_: OwnedArray of Sample objects (sole owner).
 * - samplesByRate_: HashMap of borrowed Sample* keyed by sample-rate (lookup only; do not delete via map).
 * - All Samples share a single AudioSampleBuffer; ownership is documented in SFZSample.h.
 * - Do NOT delete Sample or Region objects externally; the OwnedArrays manage lifetime.
 *************************************************************************************/
#ifndef SF2SOUND_H_INCLUDED
#define SF2SOUND_H_INCLUDED

#include "SFZSound.h"
#include <memory>

namespace sfzero
{

class SF2Sound : public Sound
{
public:
  explicit SF2Sound(const juce::File &file);
  SF2Sound(const void* data, size_t dataSize);
  virtual ~SF2Sound() override;

  // Rule of five: copy ops are deleted by JUCE_DECLARE_NON_COPYABLE below;
  // this type owns its presets/samples and must not be moved either.
  SF2Sound(SF2Sound &&) = delete;
  SF2Sound &operator=(SF2Sound &&) = delete;

  void loadRegions() override;
  void loadSamples(juce::AudioFormatManager *formatManager, double *progressVar = nullptr, juce::Thread *thread = nullptr) override;

  struct Preset
  {
    juce::String name;
    int bank;
    int preset;
    juce::OwnedArray<Region> regions;

    Preset(juce::String nameIn, int bankIn, int presetIn) noexcept : name(std::move(nameIn)), bank(bankIn), preset(presetIn) {}
    ~Preset() = default;

    // Rule of five: owns an OwnedArray<Region> (non-copyable); also non-movable.
    Preset(const Preset &) = delete;
    Preset &operator=(const Preset &) = delete;
    Preset(Preset &&) = delete;
    Preset &operator=(Preset &&) = delete;

    void addRegion(std::unique_ptr<Region> region) { regions.add(region.release()); }
  };
  void addPreset(std::unique_ptr<Preset> preset);

  int numSubsounds() override;
  juce::String subsoundName(int whichSubsound) override;
  void useSubsound(int whichSubsound) override;
  int selectedSubsound() override;

  Sample *sampleFor(double sampleRate);
  void setSamplesBuffer(std::shared_ptr<juce::AudioSampleBuffer> buffer);

  // Access to presets for instancing (SF2SoundInstance)
  int getNumPresets() const noexcept { return presets_.size(); }
  // juce::OwnedArray::operator[] is range-safe (returns nullptr if out of range).
#pragma warning(suppress : 26446)
  Preset* getPreset(int index) noexcept { return presets_[index]; }
#pragma warning(suppress : 26446)
  const Preset* getPreset(int index) const noexcept { return presets_[index]; }

private:
  juce::OwnedArray<Preset> presets_;
  juce::OwnedArray<Sample> samplesStorage_;     ///< Sole owner of Sample objects.
  juce::HashMap<int, Sample *> samplesByRate_;  ///< Borrowed lookup keyed by sample-rate.
  int selectedPreset_;

  // For memory-based loading (e.g., from BinaryData)
  std::unique_ptr<juce::MemoryInputStream> memoryStream_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SF2Sound)
};
}

#endif // SF2SOUND_H_INCLUDED
