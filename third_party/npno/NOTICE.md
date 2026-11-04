# npno reference attribution

Layout, predictive PCM codec and velocity interpolation follow jmoo/drawbar at b680c5b0b9ec9b8b04da951c6684c39aad9afe48 (BSD-3-Clause; license alongside this notice). The public demonstration fixture is copied byte-for-byte from that commit, `crates/nord-format/tests/fixtures/demo/drawbar-tine.npno`; its audio is synthesized, not factory material.

Cross-check and independent encoder/PCM oracle: simonflore/opennord at c86b22a8014afcc9d6dc96409992f10f3c004f57, src/lib/ns4/nsp-audio.ts and nsp-synth.testing.ts (AGPL-3.0-or-later). See ../opennord/LICENSE. New port modules and fixture generation script carry AGPL-3.0-or-later. Existing engine/DSP are unchanged.
