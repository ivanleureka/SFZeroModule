# SFZeroModule

## Overview

SFZeroModule is a soundfont player library for JUCE applications. It is a fork of the original SFZero project by Steve Folta and was converted into a JUCE module by Leo Olivers. The module supports loading SF2 (.sf2) and SFZ (.szf) soundfont files, parsing their regions and samples, and playing them back using JUCE’s synthesiser framework.

## Features

- **Soundfont Playback:** Load and parse SF2/SFZ files into regions and samples.
- **Preset Support:** Many soundfonts include multiple presets; SFZeroModule exposes methods to query and select these presets.
- **Polyphony:** Leverages JUCE’s synthesiser with dynamic voice allocation.
- **Voice Inspection:** Debug active voices via helper functions (e.g., `voiceInfoString()`).
- **Integration:** Easily drop into any JUCE project by adding the module.

## Requirements

- **JUCE:** Version 6 or later is recommended.
- **C++ Standard:** Requires C++17 or higher.
- **Dependencies:** This module depends on JUCE modules such as `juce_gui_basics`, `juce_audio_basics`, and `juce_audio_processors`.

## Integration

1. **Add the Module:**
   - Place the SFZeroModule folder inside your project’s `Modules` directory.
   - In Projucer, use the “Add a module from a specified folder” option and select the SFZeroModule directory.

2. **Include in Your Project:**
   - In your source files, include the module header:
     ```cpp
     #include "SFZero.h"
     ```
   - Ensure your header search paths include the module’s files.

## Basic Usage Example

Below is a code snippet that demonstrates how to set up and use the SFZeroModule in your JUCE application:

```cpp
#include "SFZero.h"

// Create an instance of the SF2 synthesiser
sfzero::Synth sf2Synth;

// Set the current sample rate (typically from your audio device)
sf2Synth.setCurrentPlaybackSampleRate(sampleRate);

// Add voices to enable polyphony
for (int i = 0; i < 128; ++i)
{
    // Create and add a new voice instance
    sf2Synth.addVoice(new sfzero::Voice());
}

// Load a SF2 soundfont file (this example uses a file from disk)
juce::File sf2File("path/to/your/soundfont.sf2");

// Create the SF2 sound object; it is reference-counted for safe memory management
auto sf2Sound = new sfzero::SF2Sound(sf2File);

// Parse the soundfont into regions
sf2Sound->loadRegions();

// Load the associated audio samples (requires an AudioFormatManager)
juce::AudioFormatManager formatManager;
formatManager.registerBasicFormats();
sf2Sound->loadSamples(&formatManager);

// If the soundfont includes multiple presets, you can query and select them:
int numPresets = sf2Sound->numSubsounds();
for (int i = 0; i < numPresets; ++i)
{
    juce::String presetName = sf2Sound->subsoundName(i);
    // Populate your UI or log the preset names as needed
}
// Set the initial preset (0-based index)
sf2Sound->useSubsound(0);

// Clear any previously loaded sounds and add the SF2 sound to the synth
sf2Synth.clearSounds();
sf2Synth.addSound(sf2Sound);

// Now, the sf2Synth is ready to receive MIDI events (e.g., via noteOn/noteOff)
// and will render audio accordingly in your audio callback.
```

## Memory Management

SFZeroModule is designed with modern C++ memory management practices to help ensure that resources are properly allocated and deallocated.

### RAII (Resource Acquisition Is Initialization)
- **Automatic Cleanup:**  
  Most objects in SFZeroModule (such as regions, samples, and voices) follow RAII principles. For instance, the destructor of the sound class (`sfzero::Sound`) iterates over its internal collections (using `juce::OwnedArray` and `juce::HashMap`) and deletes each allocated object. This means that once a sound object goes out of scope, all its associated resources are automatically freed.

### Smart Pointers and Owned Containers
- **Reference Counting:**  
  The module uses `juce::ReferenceCountedObjectPtr` for objects like `SF2Sound`. When such objects are added to the synthesiser via `addSound()`, the synthesiser automatically manages the reference count so that the sound is deallocated when no longer in use.
- **OwnedArray Containers:**  
  Collections such as regions and samples are stored in `juce::OwnedArray`, which automatically cleans up its contents when the container is destroyed.
- **Synthesiser Voices:**  
  Voices are allocated using `new` and added to the synthesiser via `addVoice()`. The synthesiser takes ownership and will handle their deletion when they’re no longer needed.

### Best Practices for Allocation and Deallocation
- **Do Not Manually Delete Owned Objects:**  
  Once you add an object (such as a voice or a sound) to a container that takes ownership, you should not delete it manually. For example, after calling:
  ```cpp
  sf2Synth.addSound(sf2Sound);
  ```
  the synthesiser manages the memory, so you should avoid calling `delete sf2Sound;` on your own.
- **Use Smart Pointers:**  
  Where possible, use smart pointers (like `std::unique_ptr` or `juce::ReferenceCountedObjectPtr`) to avoid manual memory management.
- **Clear Resources on Shutdown:**  
  When stopping audio processing or shutting down your application, call cleanup methods (such as `clearSounds()`) on the synthesiser to ensure that all dynamically allocated resources are freed.
- **Temporary Files:**  
  For temporary files (like the SF2 file example in the MainComponent), create and then delete the temporary file once the sound is loaded.

### Example

Here is an example demonstrating proper allocation and cleanup:

```cpp
// Allocate a new SF2Sound dynamically
auto* sf2Sound = new sfzero::SF2Sound(sf2File);

// Load soundfont data
sf2Sound->loadRegions();
sf2Sound->loadSamples(&formatManager);

// Add the sound to the synthesiser, which takes ownership
sf2Synth.clearSounds(); // Clear any existing sounds if necessary
sf2Synth.addSound(sf2Sound);

// Later, during shutdown or when reloading sounds:
// Turn off all notes and clear the synthesiser to deallocate owned sounds
sf2Synth.allNotesOff(0, true);
sf2Synth.clearSounds(); // This safely deallocates the SF2Sound and its resources
```

Following these practices helps ensure that SFZeroModule’s resources are managed safely, reducing memory leaks and other common issues in C++ development.

## Advanced Usage

- **Preset Management:**  
  If your soundfont contains multiple presets, use:
  - `numSubsounds()` – to get the total number of presets.
  - `subsoundName(index)` – to retrieve a preset’s name.
  - `useSubsound(index)` – to switch the active preset.
  
- **Voice Debugging:**  
  The synthesiser subclass provides functions like `numVoicesUsed()` and `voiceInfoString()` for monitoring active voices during playback.

- **SFZ File Support:**  
  In addition to SF2 files, SFZeroModule supports SFZ file formats. The loading process is analogous, with parsing routines set up for SFZ-specific parameters.

## Error Handling & Debugging

- **Error Reporting:**  
  During sample loading and region parsing, errors and warnings are recorded. You can call `dump()` on the sound object (e.g., `sf2Sound->dump()`) to retrieve a log of any issues encountered.
- **Debug Output:**  
  The module includes debug printouts (using JUCE’s `DBG` macros) to help trace processing steps during development.

## License

SFZeroModule is licensed under the MIT License. See the LICENSE file included with the module for details.

## Acknowledgements

SFZeroModule is based on the original SFZero project by Steve Folta and was adapted as a JUCE module by Leo Olivers. For more information, visit the [original SFZero repository](https://github.com/stevefolta/SFZero).

This was then forked by reFX and turned into a JUCE module, which whas then forked by William Cotton to fix some memory issues and was then forked by me to upgrade (partially) to c++ 20 and some additional polyphony changes.

## Conclusion

SFZeroModule offers a streamlined solution for integrating soundfont playback into your JUCE applications. Whether you’re building a virtual instrument, a sample-based synthesiser, or any audio application that leverages soundfonts, this module provides a flexible and efficient toolkit. Proper memory management is built into the module, ensuring that resources are allocated and deallocated safely as you create and destroy sounds and voices.
