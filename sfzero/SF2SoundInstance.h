/*************************************************************************************
 * SF2SoundInstance - Lightweight wrapper for multi-timbral SF2 playback
 *
 * OWNERSHIP MODEL (RAII):
 * - This class does NOT own its parent SF2Sound or any Region objects
 * - The parent SF2Sound must outlive all SF2SoundInstance objects that reference it
 * - Regions are borrowed pointers from the parent's presets
 * - Use juce::ReferenceCountedObjectPtr<SF2SoundInstance> for proper lifetime management
 *
 * This allows multiple synths to share a single SF2Sound's sample data
 * while maintaining independent preset (subsound) selection per instance.
 *
 * Memory usage: ~1x sample data + 16 small region pointer arrays
 * vs. 16x sample data when using separate SF2Sound per channel
 *************************************************************************************/
#ifndef SF2SOUNDINSTANCE_H_INCLUDED
#define SF2SOUNDINSTANCE_H_INCLUDED

#include "SF2Sound.h"

namespace sfzero
{

class SF2SoundInstance : public juce::SynthesiserSound
{
public:
  /** Creates an SF2SoundInstance that shares sample data with the given parent.
      @param parent  The SF2Sound that owns the sample data. Must not be null and
                     must outlive this instance. This class does NOT take ownership. */
  explicit SF2SoundInstance(SF2Sound* parent);

  ~SF2SoundInstance() override;

  using Ptr = juce::ReferenceCountedObjectPtr<SF2SoundInstance>;

  // SynthesiserSound interface
  bool appliesToNote(int midiNoteNumber) override;
  bool appliesToChannel(int midiChannel) override;

  // Subsound/preset selection (independent per instance)
  int numSubsounds() const;
  juce::String subsoundName(int whichSubsound) const;
  void useSubsound(int whichSubsound);
  int selectedSubsound() const noexcept { return selectedPreset_; }

  // Region access for voices (borrowed pointers - do not delete)
  Region* getRegionFor(int note, int velocity, Region::Trigger trigger = Region::attack);
  int getNumRegions() const noexcept { return regions_.size(); }
  Region* regionAt(int index) noexcept { return regions_[index]; }

  // Parent access (for sample data - borrowed pointer)
  SF2Sound* getParent() { return parent_; }

private:
  SF2Sound* parent_;                    ///< Borrowed pointer to parent (not owned)
  juce::Array<Region*> regions_;        ///< Borrowed region pointers (not owned)
  int selectedPreset_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SF2SoundInstance)
};

} // namespace sfzero

#endif // SF2SOUNDINSTANCE_H_INCLUDED
