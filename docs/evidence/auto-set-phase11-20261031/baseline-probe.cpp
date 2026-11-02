// Baseline replay helper: place the original PR head AutoSetSpace.h first on the include path.
// Uses the production Capture/analyser and the separately scored deterministic fixtures.
#include "AutoSetSpaceFixtures.h"
#include <juce_graphics/colour/juce_Colour.cpp>
#include <iostream>
#include <iomanip>
int main(){
 std::cout<<"rate,fixture,reference_bpm,expected_onsets,detected_onsets,detected_nps,estimated_bpm,beat_ms,delay_on,pause_fraction,sustain_ms\n";
 for(double rate:{44100.,48000.,96000.})for(const auto& f:spaceFixtures::scores()){
  const auto r=spectralforge::autoset::analyseSpace(spaceFixtures::capture(spaceFixtures::render(f,rate),rate),false,spectralforge::autoset::Style::clean);
  std::cout<<rate<<','<<f.name<<','<<f.bpm<<','<<f.notes.size()<<','<<r.feel.onsets<<','<<r.feel.onsetsPerSecond<<','<<(r.feel.beatMs>0?60000/r.feel.beatMs:0)<<','<<r.feel.beatMs<<','<<r.delay.on<<','<<r.feel.pauseFraction<<','<<r.feel.sustainMs<<'\n';
 }
}
