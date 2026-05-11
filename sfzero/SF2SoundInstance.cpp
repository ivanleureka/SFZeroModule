/*************************************************************************************
 * SF2SoundInstance - Lightweight wrapper for multi-timbral SF2 playback
 *************************************************************************************/
#include "SF2SoundInstance.h"

sfzero::SF2SoundInstance::SF2SoundInstance(SF2Sound* parent)
    : parent_(parent), selectedPreset_(0)
{
  jassert(parent_ != nullptr);

  // Initialize with first preset
  if (parent_->getNumPresets() > 0)
  {
    useSubsound(0);
  }
}

sfzero::SF2SoundInstance::~SF2SoundInstance()
{
  // We don't own the regions or parent - just clear our pointer array
  regions_.clear();
}

bool sfzero::SF2SoundInstance::appliesToNote(int midiNoteNumber)
{
  juce::ignoreUnused(midiNoteNumber);
  return true;
}

bool sfzero::SF2SoundInstance::appliesToChannel(int midiChannel)
{
  juce::ignoreUnused(midiChannel);
  return true;
}

int sfzero::SF2SoundInstance::numSubsounds() const
{
  jassert(parent_ != nullptr);
  return parent_ ? parent_->numSubsounds() : 0;
}

juce::String sfzero::SF2SoundInstance::subsoundName(int whichSubsound) const
{
  jassert(parent_ != nullptr);
  return parent_ ? parent_->subsoundName(whichSubsound) : juce::String();
}

void sfzero::SF2SoundInstance::useSubsound(int whichSubsound)
{
  if (!parent_)
  {
    jassertfalse;  // Parent should never be null
    return;
  }

  int numPresets = parent_->getNumPresets();
  if (whichSubsound < 0 || whichSubsound >= numPresets)
  {
    jassertfalse;  // Invalid subsound index
    return;
  }

  selectedPreset_ = whichSubsound;

  // Clear our regions and copy pointers from parent's preset
  regions_.clear();

  auto* preset = parent_->getPreset(whichSubsound);
  if (preset)
  {
    // Copy region pointers (not deep copy - regions are owned by parent)
    for (auto* region : preset->regions)
    {
      regions_.add(region);
    }
  }
}

sfzero::Region* sfzero::SF2SoundInstance::getRegionFor(int note, int velocity, Region::Trigger trigger)
{
  jassert(parent_ != nullptr);
  for (auto* region : regions_)
  {
    if (region->matches(note, velocity, trigger))
    {
      return region;
    }
  }
  return nullptr;
}
