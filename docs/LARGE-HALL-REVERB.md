# Large-hall upper reverb range

Based on SF2-native-glide commit ee5c0d5047a81c3f8dd03f81d382da602d13701b.

The original knob only multiplied a fixed short four-comb return. The new implementation preserves its exact response at 0–70%. Above 70%, a smoothstep blend adds a separate stereo hall with eight delay lines per channel (31–90 ms), an orthogonal Householder feedback matrix, four allpass diffusion stages, 28 ms predelay, 180 Hz input highpass and 6 kHz feedback damping. Separate channel delays provide decorrelation for centered mono material without moving independently panned layers. This shared Dsp is used by SF2 and SFZ.

The nominal unfiltered feedback RT60 target increases from 2.5 to 9 seconds; frequency damping shortens the audible decay, especially at high frequencies. This is an algorithmic musical ambience, not a measured model of a particular room and not a shimmer/pitch-shifting effect. No listening validation on the user's speakers has been performed.

Input/output hall blend and feedback gains smooth over 30 ms. The dry signal remains unchanged. Hall feedback gains are individually below unity, combined with an orthogonal matrix and passive one-pole damping. No allocation or locks are added to process(); buffers are allocated during Dsp setup. Reset clears every delay/filter state. Existing master gain/headroom remains the user's control; bounded impulse testing is not a guarantee against clipping arbitrary high-level layered inputs.

Reference for recirculating reverb, diffusion and passive feedback damping: Miller Puckette, The Theory and Technique of Electronic Music, Artificial reverberation: https://msp.ucsd.edu/techniques/latest/book-html/node111.html . Implementation is original project code.

Validation (Linux offline): 11/11 CTest cases passed, including real FluidSynth SF2 rendering and native-glide pitch measurements. New reverb tests check exact legacy response through 70%, 16-second stereo impulse tails at 8/44.1/48/96 kHz, increasing late energy at 70/85/100%, natural decay, reset and silent control sweeps. At 48 kHz, late energy (2–4 s) was 9.43e-16 / 6.84e-6 / 8.07e-4; final energy (14–16 s) at maximum was 2.14e-12. Windows x64 LivePerformanceVision target compiled locally. Windows/ASIO hardware and musical listening remain unvalidated; no new portable was generated or uploaded.

GitHub connector exposes no authenticated quota/billing check; remaining allowance is unknown. Publish this stage as one source-only commit, without running hosted Actions or uploading binaries.
