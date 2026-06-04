/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#ifndef SFZSYNTH_H_INCLUDED
#define SFZSYNTH_H_INCLUDED

#include "SFZCommon.h"

namespace sfzero
{

class Voice;

class Synth : public juce::Synthesiser
{
public:
  Synth() noexcept;
  virtual ~Synth() override {}

  // Rule of five: copy ops deleted by JUCE_DECLARE_NON_COPYABLE below; delete
  // move ops too (C26432) without re-declaring copy.
  Synth(Synth &&) = delete;
  Synth &operator=(Synth &&) = delete;

  void noteOn(int midiChannel, int midiNoteNumber, float velocity) override;
  void noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override;

  int numVoicesUsed() noexcept;
  juce::String voiceInfoString();

  //==============================================================================
  /** Voice Pool Support
      These methods allow external ownership of voices for real-time safe voice
      management. Voices can be pre-allocated in a pool and transferred to/from
      synths without allocating on the audio thread.
  */

  /** Release a voice from this synth without deleting it.
      Returns the voice pointer and removes it from the internal array.
      Caller takes ownership and is responsible for deletion.
      @param index  The index of the voice to release (0 to getNumVoices()-1)
      @return       The released voice, or nullptr if index is invalid
  */
  Voice* releaseVoice(int index);

  /** Release all voices from this synth without deleting them.
      Returns the voices in a vector, caller takes ownership.
      The synth will have zero voices after this call.
      @return  Vector of released voice pointers
  */
  std::vector<Voice*> releaseAllVoices();

  /** Get a voice by index (const access for inspection).
      @param index  The index of the voice
      @return       The voice at the given index, or nullptr if invalid
  */
  Voice* getVoiceAt(int index) const noexcept;

private:
  // Per-note velocity cache, indexed by MIDI note (0-127). Default-initialised
  // so noteOff() can never read an indeterminate value before the matching
  // noteOn() populated it (root-cause fix for C26495).
  int noteVelocities_[128] = {};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Synth)
};
}

#endif // SFZSYNTH_H_INCLUDED
