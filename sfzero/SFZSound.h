/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *
 * OWNERSHIP MODEL:
 * - samplesStorage_ is the single owner of all Sample objects (juce::OwnedArray).
 * - samplesByPath_ is a lookup map of borrowed Sample* (do not delete via map).
 * - regions_ is a non-owning view of currently-active Region pointers; ownership
 *   lives in sfzOwnedRegions_ (SFZ path) or in SF2Sound::Preset::regions (SF2 path).
 * - Samples and Regions are exposed via borrowed pointers; callers must not delete.
 *************************************************************************************/
#ifndef SFZSOUND_H_INCLUDED
#define SFZSOUND_H_INCLUDED

#include "SFZRegion.h"
#include <memory>

namespace sfzero
{

class Sample;

class Sound : public juce::SynthesiserSound
{
public:
  explicit Sound(const juce::File &file);
  virtual ~Sound() override;

  typedef juce::ReferenceCountedObjectPtr<Sound> Ptr;

  bool appliesToNote(int midiNoteNumber) override;
  bool appliesToChannel(int midiChannel) override;

  void addRegion(std::unique_ptr<Region> region); // Takes ownership of the region.
  Sample *addSample(juce::String path, juce::String defaultPath = {});
  void addError(const juce::String &message);
  void addUnsupportedOpcode(const juce::String &opcode);
  void addUnsupportedOpcode(const juce::String &opcode, int amount);

  virtual void loadRegions();
  virtual void loadSamples(juce::AudioFormatManager *formatManager, double *progressVar = nullptr,
                           juce::Thread *thread = nullptr);

  Region *getRegionFor(int note, int velocity, Region::Trigger trigger = Region::attack);
  int getNumRegions() const noexcept { return regions_.size(); }
  Region *regionAt(int index) const noexcept { return regions_[index]; }

  const juce::StringArray &getErrors() const noexcept { return errors_; }
  const juce::StringArray &getWarnings() const noexcept { return warnings_; }
  juce::StringArray getUnsupportedOpcodes();
  juce::StringArray getUnsupportedOpcodeReport();

  virtual int numSubsounds();
  virtual juce::String subsoundName(int whichSubsound);
  virtual void useSubsound(int whichSubsound);
  virtual int selectedSubsound();

  juce::String dump();
  juce::Array<Region *> &getRegions() noexcept { return regions_; }
  const juce::Array<Region *> &getRegions() const noexcept { return regions_; }
  juce::File &getFile() noexcept { return file_; }

private:
  juce::File file_;
  juce::Array<Region *> regions_;                       ///< Borrowed view; ownership lives in sfzOwnedRegions_ or Preset::regions.
  juce::OwnedArray<Region> sfzOwnedRegions_;            ///< Owns Regions added via addRegion() (SFZ path; empty for SF2).
  juce::OwnedArray<Sample> samplesStorage_;             ///< Sole owner of Sample objects.
  juce::HashMap<juce::String, Sample *> samplesByPath_; ///< Borrowed lookup keyed by full path.
  juce::StringArray errors_;
  juce::StringArray warnings_;

  struct OpcodeStats
  {
    int total = 0;
    int nonZero = 0;
    int minAmount = 0;
    int maxAmount = 0;
    bool hasAmount = false;
  };
  juce::HashMap<juce::String, OpcodeStats> unsupportedOpcodes_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sound)
};
}

#endif // SFZSOUND_H_INCLUDED
