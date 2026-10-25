#include "recomp_settings_model.h"
#include <iostream>
int main() {
    using namespace openshao::recomp;
    unsigned failures=0;
    auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<text<<'\n';}};
    openshao::launcher::Settings original;original.master_volume=42;original.mouse.sensitivity=165;
    Model model;model.Open(original);
    check(model.Rows().size()==7 && model.CurrentPage()==Page::Home,"Recompilation Home must include five categories and explicit save/discard");
    check(model.Rows()[5].key=="settings.save" && model.Rows()[6].key=="settings.discard","Enhancements must have a separate main-menu entry");
    model.Enter(5);check(model.CurrentPage()==Page::Home,"Saving must not enter a configuration category");
    model.Go(Page::Enhancements);
    check(model.Title()=="recomp.enhancements" && model.Rows().size()==3 && model.Rows()[0].key=="settings.scorpion_unfiltered" &&
          model.Rows()[1].key=="settings.save" && model.Rows()[2].key=="settings.discard",
          "The main-menu Enhancements page must expose its toggle and explicit Save/Discard actions");
    model.Change(0,1);
    check(openshao::launcher::Merge({},model.Draft())["enhancements"]["scorpion_unfiltered"].value_or(false) && model.Dirty(),
          "Enabling the Scorpion voice preference must edit the settings draft");
    model.Discard();
    check(!openshao::launcher::Merge({},model.Draft())["enhancements"]["scorpion_unfiltered"].value_or(true),
          "Discarding the enhancement must restore the original voice distribution");
    check(model.Rows()[4].key=="settings.language","The fifth category must contain language selection without duplicating original game settings");
    model.Enter(4);
    check(model.Rows().size()==2 && model.Rows()[0].key=="settings.text_language" && model.Rows()[0].value=="language.en","Language must display an autonym from the text catalog");
    check(model.Rows().size()>1 && model.Rows()[1].key=="settings.voice_language" && model.Rows()[1].value=="language.en",
          "Language must expose an explicit English voice selection by default");
    model.Change(1,1);
    check(openshao::launcher::Merge({},model.Draft())["voice_language"].value_or(std::string{})=="fr" && model.Draft().language=="en",
          "Editing voices must preserve text language");
    model.Change(0,1);model.Change(0,1);
    check(model.Draft().language=="de" && openshao::launcher::Merge({},model.Draft())["voice_language"].value_or(std::string{})=="fr",
          "Editing text must preserve an explicit voice choice");
    const std::array<const char*,6> voices{"en","fr","de","es","it","pt-BR"};
    for (const auto* text : openshao::launcher::languages) {
        auto settings=original;settings.language=text;model.Open(settings);model.Go(Page::Language);
        for (size_t index=1;index<=voices.size();++index) {
            model.Change(1,1);
            const std::string expected=voices[index%voices.size()];
            check(model.Draft().voice_language==expected && model.Rows()[1].value=="language."+expected && model.Draft().language==text,
                  "Every text language must offer all six explicit voice autonyms and wrap without changing text");
        }
        model.Change(1,-1);
        check(model.Draft().voice_language=="pt-BR","Voice selection must wrap backward from English to Brazilian Portuguese");
    }
    model.Open(original);
    model.Discard();model.Go(Page::Language);
    model.Change(2,1);model.Change(3,1);model.Change(4,1);model.Change(5,1);
    check(model.Draft()==original,"Removed native settings rows must not modify brightness, versus rules, camera control or native persistence");
    model.Back();
    model.Enter(0);check(model.CurrentPage()==Page::Display,"Display must open from Home");
    model.Change(1,1);check(model.Draft().width>=320 && model.Draft().height>=240,"A custom width must initialize both resolution dimensions");
    model.Back();model.Enter(3);model.Change(0,1);
    check(model.Draft().master_volume>42 && original.master_volume==42,"Edits must remain in an isolated draft until saved");
    check(model.Rows().size()==1 && model.Rows()[0].key=="settings.master","Recompilation audio must expose only the global output volume");
    model.Change(1,1);model.Change(2,-1);model.Change(3,1);
    check(model.Draft().music_volume==original.music_volume && model.Draft().effects_volume==original.effects_volume && model.Draft().speech_volume==original.speech_volume && model.Draft().original_override==original.original_override,
        "Global audio edits must preserve the native music, effects and speech controls and persistence policy");
    model.Back();model.Enter(2);model.Enter(0);
    check(model.CurrentPage()==Page::Controller && model.Rows().size()==25,"Every controller direction and device reset must be available");
    model.ControllerBinding(0,"rightx-");
    check(model.Draft().bindings[0]=="a" && model.Draft().bindings[22]=="rightx-","An occupied controller input must be refused without changing either action");
    model.ControllerBinding(22,"none");model.ControllerBinding(0,"rightx-");check(model.Draft().bindings[0]=="rightx-","A released analog direction must be assignable");
    check(model.Rows()[0].value=="source.rightx-","Controller values must use localized catalog keys");
    check(model.Conflict(0).empty(),"A refused duplicate must not leave a shared controller binding");
    model.Enter(24);check(model.Draft().bindings==openshao::bindings::controller_defaults,"Device defaults must restore controller bindings");
    check(model.Draft().master_volume>42 && model.Draft().mouse.sensitivity==165,"Controller reset must preserve unrelated devices and audio");
    model.Back();model.Enter(1);model.KeyboardBinding(0,0,'Z');
    check(model.Rows()[0].value=="key.90","Captured keyboard keys must retain their numeric identity for localized display");
    model.KeyboardBinding(1,0,'Z');
    check(model.Draft().keyboard.keys[1][0]==27,"A captured keyboard input already used by another action must be refused");
    model.KeyboardBinding(1,0,73);model.Change(1,1);
    check(model.Draft().keyboard.keys[1][0]==76 && model.Conflict(1).empty(),"Cycling keyboard inputs must skip J and K, which belong to other actions");
    model.Enter(24);check(model.CurrentPage()==Page::KeyboardAlternate,"Alternate keyboard bindings must have their own page");
    model.KeyboardBinding(0,1,'V');
    check(model.Draft().keyboard.keys[0][0]=='Z' && model.Draft().keyboard.keys[0][1]=='V',"Primary and alternate keys must remain independent");
    model.KeyboardBinding(2,1,'Z');
    check(model.Draft().keyboard.keys[2][1]==0,"Alternate keyboard inputs must also reject conflicts with a primary binding");
    model.Go(Page::Controller);model.Change(0,1);
    check(model.Draft().bindings[0]!="b" && model.Conflict(0).empty(),"Controller cycling must skip inputs owned by other actions");
    model.Discard();check(model.Draft()==original,"Discard must restore the complete opening settings snapshot");
    for(const auto page:{Page::Display,Page::Graphics,Page::Controls,Page::Controller,Page::Keyboard,Page::KeyboardAlternate,Page::Mouse,Page::Audio,Page::Language}) {
        model.Go(page);
        for(size_t row=0;row<model.Rows().size();++row)for(int repeat=0;repeat<60;++repeat)model.Change(row,repeat%2?1:-1);
        std::string error;check(openshao::launcher::Validate(model.Draft(),error),"Every field edit must preserve settings validation");
        check(openshao::launcher::Read(openshao::launcher::Merge({},model.Draft()))==model.Draft(),"Every settings page must round-trip all device and game fields without loss");
    }
    return failures?1:0;
}
