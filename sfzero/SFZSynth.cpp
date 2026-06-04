/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SFZSynth.h"
#include "SFZSound.h"
#include "SF2SoundInstance.h"
#include "SFZVoice.h"
#include "SFZSafeCast.h"

sfzero::Synth::Synth() noexcept : Synthesiser() {}

void sfzero::Synth::noteOn(int midiChannel, int midiNoteNumber, float velocity)
{
  int i = 0;

  const juce::ScopedLock locker(lock);

  const int midiVelocity = narrowCast<int>(velocity * 127);

  // Get the sound - check for both Sound and SF2SoundInstance
  sfzero::Sound *sound = nullptr;
  sfzero::SF2SoundInstance *soundInstance = nullptr;

  if (getNumSounds() > 0)
  {
    auto* rawSound = getSound(0).get();
    sound = dynamic_cast<sfzero::Sound *>(rawSound);
    if (sound == nullptr)
    {
      soundInstance = dynamic_cast<sfzero::SF2SoundInstance *>(rawSound);
    }
  }

  // First, stop any currently-playing sounds in any group this note triggers.
  // Phase C (V1 fix): a single MIDI note may match multiple regions
  // (multi-layered drums in particular), and each can declare a distinct group.
  // We must collect *all* the group IDs from matching regions and silence any
  // voice whose off_by points at any of them - not just the first match.
  juce::SortedSet<int> groupsTriggered;
  const sfzero::Region::Trigger groupCheckTrigger = sfzero::Region::first;
  if (sound)
  {
    const int numRegions = sound->getNumRegions();
    for (int r = 0; r < numRegions; ++r)
    {
      const sfzero::Region *region = sound->regionAt(r);
      if (region && region->matches(midiNoteNumber, midiVelocity, groupCheckTrigger) && region->group != 0)
      {
        groupsTriggered.add(region->group);
      }
    }
  }
  else if (soundInstance)
  {
    const int numRegions = soundInstance->getNumRegions();
    for (int r = 0; r < numRegions; ++r)
    {
      const sfzero::Region *region = soundInstance->regionAt(r);
      if (region && region->matches(midiNoteNumber, midiVelocity, groupCheckTrigger) && region->group != 0)
      {
        groupsTriggered.add(region->group);
      }
    }
  }

  if (!groupsTriggered.isEmpty())
  {
    for (i = voices.size(); --i >= 0;)
    {
      sfzero::Voice *voice = dynamic_cast<sfzero::Voice *>(voices.getUnchecked(i));
      if (voice == nullptr)
      {
        continue;
      }
      const int voiceOffBy = narrowCast<int>(voice->getOffBy());
      if (voiceOffBy != 0 && groupsTriggered.contains(voiceOffBy))
      {
        voice->stopNoteForGroup();
      }
    }
  }

  // Are any notes playing?  (Needed for first/legato trigger handling.)
  // Also stop any voices still playing this note.
  bool anyNotesPlaying = false;
  for (i = voices.size(); --i >= 0;)
  {
    sfzero::Voice *voice = dynamic_cast<sfzero::Voice *>(voices.getUnchecked(i));
    if (voice == nullptr)
    {
      continue;
    }
    if (voice->isPlayingChannel(midiChannel))
    {
      if (voice->isPlayingNoteDown())
      {
        if (voice->getCurrentlyPlayingNote() == midiNoteNumber)
        {
          if (!voice->isPlayingOneShot())
          {
            voice->stopNoteQuick();
          }
        }
        else
        {
          anyNotesPlaying = true;
        }
      }
    }
  }

  // Play *all* matching regions.
  const sfzero::Region::Trigger trigger = (anyNotesPlaying ? sfzero::Region::legato : sfzero::Region::first);

  // Handle Sound type
  if (sound)
  {
    const int numRegions = sound->getNumRegions();
    for (i = 0; i < numRegions; ++i)
    {
      sfzero::Region *region = sound->regionAt(i);
      if (region->matches(midiNoteNumber, midiVelocity, trigger))
      {
        sfzero::Voice *voice =
            dynamic_cast<sfzero::Voice *>(findFreeVoice(sound, midiNoteNumber, midiChannel, isNoteStealingEnabled()));
        if (voice)
        {
          voice->setRegion(region);
          startVoice(voice, sound, midiChannel, midiNoteNumber, velocity);
        }
      }
    }
  }
  // Handle SF2SoundInstance type
  else if (soundInstance)
  {
    const int numRegions = soundInstance->getNumRegions();
    for (i = 0; i < numRegions; ++i)
    {
      sfzero::Region *region = soundInstance->regionAt(i);
      if (region->matches(midiNoteNumber, midiVelocity, trigger))
      {
        sfzero::Voice *voice =
            dynamic_cast<sfzero::Voice *>(findFreeVoice(soundInstance, midiNoteNumber, midiChannel, isNoteStealingEnabled()));
        if (voice)
        {
          voice->setRegion(region);
          startVoice(voice, soundInstance, midiChannel, midiNoteNumber, velocity);
        }
      }
    }
  }

  // midiNoteNumber is a MIDI note (0-127) and noteVelocities_ is int[128];
  // index is in range by the MIDI spec. .at() would add a throwing check on the
  // real-time note path, so the fixed-array index is suppressed (C26446/C26482).
#pragma warning(suppress : 26446 26482)
  noteVelocities_[midiNoteNumber] = midiVelocity;
}

void sfzero::Synth::noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff)
{
  const juce::ScopedLock locker(lock);

  Synthesiser::noteOff(midiChannel, midiNoteNumber, velocity, allowTailOff);

  // Start release region.
  // Get the sound - check for both Sound and SF2SoundInstance
  sfzero::Sound *sound = nullptr;
  sfzero::SF2SoundInstance *soundInstance = nullptr;

  if (getNumSounds() > 0)
  {
    auto* rawSound = getSound(0).get();
    sound = dynamic_cast<sfzero::Sound *>(rawSound);
    if (sound == nullptr)
    {
      soundInstance = dynamic_cast<sfzero::SF2SoundInstance *>(rawSound);
    }
  }

  sfzero::Region *region = nullptr;

  // Cache the stored velocity once. midiNoteNumber is a MIDI note (0-127) and
  // noteVelocities_ is int[128], so the index is in range by the MIDI spec;
  // .at() would add a throwing check on the real-time note path (C26446/C26482).
#pragma warning(suppress : 26446 26482)
  const int noteVelocity = noteVelocities_[midiNoteNumber];

  if (sound)
  {
    region = sound->getRegionFor(midiNoteNumber, noteVelocity, sfzero::Region::release);
    if (region)
    {
      sfzero::Voice *voice = dynamic_cast<sfzero::Voice *>(findFreeVoice(sound, midiNoteNumber, midiChannel, false));
      if (voice)
      {
        // Synthesiser is too locked-down (ivars are private rt protected), so
        // we have to use a "setRegion()" mechanism.
        voice->setRegion(region);
        startVoice(voice, sound, midiChannel, midiNoteNumber, noteVelocity / 127.0f);
      }
    }
  }
  else if (soundInstance)
  {
    region = soundInstance->getRegionFor(midiNoteNumber, noteVelocity, sfzero::Region::release);
    if (region)
    {
      sfzero::Voice *voice = dynamic_cast<sfzero::Voice *>(findFreeVoice(soundInstance, midiNoteNumber, midiChannel, false));
      if (voice)
      {
        voice->setRegion(region);
        startVoice(voice, soundInstance, midiChannel, midiNoteNumber, noteVelocity / 127.0f);
      }
    }
  }
}

int sfzero::Synth::numVoicesUsed() noexcept
{
  int numUsed = 0;

  for (int i = voices.size(); --i >= 0;)
  {
    if (voices.getUnchecked(i)->getCurrentlyPlayingNote() >= 0)
    {
      numUsed += 1;
    }
  }
  return numUsed;
}

juce::String sfzero::Synth::voiceInfoString()
{
  enum
  {
    maxShownVoices = 20,
  };

  juce::StringArray lines;
  int numUsed = 0, numShown = 0;
  for (int i = voices.size(); --i >= 0;)
  {
    sfzero::Voice *voice = dynamic_cast<sfzero::Voice *>(voices.getUnchecked(i));
    if (voice->getCurrentlyPlayingNote() < 0)
    {
      continue;
    }
    numUsed += 1;
    if (numShown >= maxShownVoices)
    {
      continue;
    }
    lines.add(voice->infoString());
  }
  lines.insert(0, "voices used: " + juce::String(numUsed));
  return lines.joinIntoString("\n");
}

//==============================================================================
// Voice Pool Support

sfzero::Voice* sfzero::Synth::releaseVoice(int index)
{
  const juce::ScopedLock locker(lock);

  if (index < 0 || index >= voices.size())
    return nullptr;

  // Stop the voice if it's playing
  auto* voice = voices.getUnchecked(index);
  if (voice->getCurrentlyPlayingNote() >= 0)
  {
    voice->stopNote(0.0f, false);
  }

  // Release from OwnedArray without deleting
  // OwnedArray::removeAndReturn() removes the item and returns it without deleting
  return dynamic_cast<sfzero::Voice*>(voices.removeAndReturn(index));
}

std::vector<sfzero::Voice*> sfzero::Synth::releaseAllVoices()
{
  const juce::ScopedLock locker(lock);

  std::vector<sfzero::Voice*> released;
  released.reserve(static_cast<size_t>(voices.size()));

  // Stop all voices and release them
  while (voices.size() > 0)
  {
    auto* voice = voices.getUnchecked(0);
    if (voice->getCurrentlyPlayingNote() >= 0)
    {
      voice->stopNote(0.0f, false);
    }

    auto* releasedVoice = dynamic_cast<sfzero::Voice*>(voices.removeAndReturn(0));
    if (releasedVoice)
    {
      released.push_back(releasedVoice);
    }
  }

  return released;
}

sfzero::Voice* sfzero::Synth::getVoiceAt(int index) const noexcept
{
  if (index < 0 || index >= voices.size())
    return nullptr;

  return dynamic_cast<sfzero::Voice*>(voices.getUnchecked(index));
}
