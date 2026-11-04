// SPDX-License-Identifier: AGPL-3.0-or-later
// GUI-free commands through the real native App, no audio host or ASIO needed.
#define wWinMain lp_parameter_application_entry
#include "../src/win32_app.cpp"
#undef wWinMain
#include <cassert>
#include <iostream>
int main(){
 auto root=std::filesystem::temp_directory_path()/"lp-native-parameter-restart";std::filesystem::create_directories(root);auto file=root/"sound-parameters.settings";std::filesystem::remove(file);std::filesystem::remove(root/"sound-parameters.settings.backup");
 std::array<float,16> expected{};
 {
  App app;app.base=root;app.settingsPath=root/"audio.settings";app.loadParameterMemory();app.show.favorites[2]=lp::Patch{};app.show.favorites[2]->sfz="bank://piano";app.show.current=2;app.savedProfile=app.profileSnapshot();
  for(int index=0;index<16;++index){app.setPanelValue(index,.03f*(index+1));expected[index]=.03f*(index+1);}
  assert(app.show.favorites[2]->adsrEnabled&&!app.profileDirty());
  app.router.learn(6);app.midiEvent(0x5b28b0);expected[6]=91/127.f;assert(app.show.favorites[2]->cc[6]==40);
  app.setMasterValue(.43f);assert(!app.profileDirty());
  // Deliberately leave the automatic write pending: actual App destruction drains it.
 }
 {
  App reopened;reopened.base=root;reopened.settingsPath=root/"audio.settings";reopened.loadParameterMemory();assert(reopened.show.master==.43f);
  reopened.previewActive=true;reopened.previewPatch.sfz="bank://piano";assert(reopened.soundParameters.restore(lp::soundParameterKey(reopened.previewPatch.sfz,root),reopened.previewPatch));
  assert(reopened.previewPatch.parameters==expected&&reopened.previewPatch.adsrEnabled&&reopened.previewPatch.cc[6]==40);
  reopened.setPanelValue(6,.82f);expected[6]=.82f;reopened.flushParameterMemory();
  // A stale favorite/profile must restore the newer settings without a profile save.
  reopened.show.favorites[7]=lp::Patch{};reopened.show.favorites[7]->sfz="bank://piano";reopened.show.favorites[7]->parameters[6]=.1f;reopened.soundParameters.restoreSession(reopened.show,root);assert(reopened.show.favorites[7]->parameters[6]==.82f);
 }
 {
  App last;last.base=root;last.settingsPath=root/"audio.settings";last.loadParameterMemory();lp::Patch actual;assert(last.soundParameters.restore(lp::soundParameterKey("bank://piano",root),actual));assert(actual.parameters==expected);
 }
 std::filesystem::remove_all(root);std::cout<<"PASSOU: App real, mouse/MIDI, favoritos/categoria, fechamento sem perfil, reabertura e sobrescrita permanente\n";
}
