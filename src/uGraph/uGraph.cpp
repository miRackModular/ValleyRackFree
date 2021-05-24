//
// UGraph.cpp
// Author: Dale Johnson
// Contact: valley.audio.soft@gmail.com
// Date: 5/12/2017
//
// UGraph, a port of "Mutable Instruments Grids" for VCV Rack
// Original author: Olivier Gillet (ol.gillet@gmail.com)
// https://github.com/pichenettes/eurorack/tree/master/grids
// Copyright 2012 Olivier Gillet.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "../Valley.hpp"
#include "../ValleyComponents.hpp"
#include "../Common/Metronome.hpp"
#include "../Common/Oneshot.hpp"
#include "../Topograph/TopographPatternGenerator.hpp"
#include <iomanip> // setprecision
#include <sstream> // stringstream

struct UGraph : Module {
    enum ParamIds {
        RESET_BUTTON_PARAM,
        RUN_BUTTON_PARAM,
        TEMPO_PARAM,
        MAPX_PARAM,
        MAPY_PARAM,
        CHAOS_PARAM,
        BD_DENS_PARAM,
        SN_DENS_PARAM,
        HH_DENS_PARAM,
        SWING_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        CLOCK_INPUT,
        RESET_INPUT,
        MAPX_CV,
        MAPY_CV,
        CHAOS_CV,
        BD_FILL_CV,
        SN_FILL_CV,
        HH_FILL_CV,
        SWING_CV,
        RUN_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        BD_OUTPUT,
        SN_OUTPUT,
        HH_OUTPUT,
        BD_ACC_OUTPUT,
        SN_ACC_OUTPUT,
        HH_ACC_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        RUNNING_LIGHT,
        RESET_LIGHT,
        BD_LIGHT,
        SN_LIGHT,
        HH_LIGHT,
        NUM_LIGHTS
    };

    Metronome metro;
    PatternGenerator grids;
    uint8_t numTicks;
    dsp::SchmittTrigger clockTrig;
    dsp::SchmittTrigger resetTrig;
    dsp::SchmittTrigger resetButtonTrig;
    dsp::SchmittTrigger runButtonTrig;
    dsp::SchmittTrigger runInputTrig;
    bool initExtReset = true;
    int running = 0;
    bool extClock = false;
    bool advStep = false;
    long seqStep = 0;
    float swing = 0.5;
    float swingHighTempo = 0.0;
    float swingLowTempo = 0.0;
    long elapsedTicks = 0;

    float tempoParam = 0.0;
    std::shared_ptr<float> tempo = std::make_shared<float>(120.0);
    std::shared_ptr<float> mapX = std::make_shared<float>(0.0);
    std::shared_ptr<float> mapY = std::make_shared<float>(0.0);
    std::shared_ptr<float> chaos = std::make_shared<float>(0.0);
    float BDFill = 0.0;
    float SNFill = 0.0;
    float HHFill = 0.0;

    uint8_t state = 0;

    // LED Triggers
    Oneshot drumLED[3];
    const LightIds drumLEDIds[3] = {BD_LIGHT, SN_LIGHT, HH_LIGHT};
    Oneshot BDLed;
    Oneshot SNLed;
    Oneshot HHLed;
    Oneshot resetLed;
    Oneshot runningLed;

    // Drum Triggers
    Oneshot drumTriggers[6];
    bool gateState[6];
    const OutputIds outIDs[6] = {BD_OUTPUT, SN_OUTPUT, HH_OUTPUT,
                                 BD_ACC_OUTPUT, SN_ACC_OUTPUT, HH_ACC_OUTPUT};

    enum SequencerMode {
        HENRI,
        OLIVIER,
        EUCLIDEAN
    };
    SequencerMode sequencerMode = OLIVIER;
    int inEuclideanMode = 0;

    unsigned long sequencerModeChoice = 0;
    unsigned long prevClockResChoice = 0;
    unsigned long clockResChoice = 0;

    enum TriggerOutputMode {
        PULSE,
        GATE
    };
    TriggerOutputMode triggerOutputMode = PULSE;

    enum AccOutputMode {
        INDIVIDUAL_ACCENTS,
        ACC_CLK_RST
    };
    AccOutputMode accOutputMode = INDIVIDUAL_ACCENTS;

    enum ExtClockResolution {
        EXTCLOCK_RES_4_PPQN,
        EXTCLOCK_RES_8_PPQN,
        EXTCLOCK_RES_24_PPQN,
    };
    ExtClockResolution extClockResolution = EXTCLOCK_RES_24_PPQN;


    enum ChaosKnobMode {
        CHAOS,
        SWING
    };
    ChaosKnobMode chaosKnobMode = CHAOS;

    enum RunMode {
        TOGGLE,
        MOMENTARY
    };
    RunMode runMode = TOGGLE;

    int panelStyle;
    std::string clockBPM;
    std::string mapXText = "Map X";
    std::string mapYText = "Map Y";
    std::string chaosText = "Chaos";
    int textVisible = 1;

    UGraph() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(UGraph::TEMPO_PARAM, 0.0, 1.0, 0.406, "Tempo", "BPM", 0.f, 202.020202, 37.979797);
        configParam(UGraph::MAPX_PARAM, 0.0, 1.0, 0.0, "Pattern Map X");
        configParam(UGraph::MAPY_PARAM, 0.0, 1.0, 0.0, "Pattern Map Y");
        configParam(UGraph::CHAOS_PARAM, 0.0, 1.0, 0.0, "Pattern Chaos");
        configParam(UGraph::BD_DENS_PARAM, 0.0, 1.0, 0.5, "Channel 1 Density");
        configParam(UGraph::SN_DENS_PARAM, 0.0, 1.0, 0.5, "Channel 2 Density");
        configParam(UGraph::HH_DENS_PARAM, 0.0, 1.0, 0.5, "Channel 3 Density");
        configParam(UGraph::SWING_PARAM, 0.0, 0.9, 0.0, "Swing");
        configParam(UGraph::RESET_BUTTON_PARAM, 0.0, 1.0, 0.0, "Reset");
        configParam(UGraph::RUN_BUTTON_PARAM, 0.0, 1.0, 0.0, "Run");

        float sampleRate = APP->engine->getSampleRate();
        metro = Metronome(120, sampleRate, 24.0, 0.0);
        numTicks = ticks_granularity[2];
        srand(time(NULL));
        BDLed = Oneshot(0.1, sampleRate);
        SNLed = Oneshot(0.1, sampleRate);
        HHLed = Oneshot(0.1, sampleRate);
        resetLed = Oneshot(0.1, sampleRate);
        for(int i = 0; i < 6; ++i) {
            drumTriggers[i] = Oneshot(0.008, sampleRate);
            gateState[i] = false;
        }
        for(int i = 0; i < 3; ++i) {
            drumLED[i] = Oneshot(0.1, sampleRate);
        }
        panelStyle = 1;
    }

    json_t *dataToJson() override {
        json_t *rootJ = json_object();
        json_object_set_new(rootJ, "sequencerMode", json_integer(sequencerModeChoice));
        json_object_set_new(rootJ, "triggerOutputMode", json_integer(triggerOutputMode));
        json_object_set_new(rootJ, "accOutputMode", json_integer(accOutputMode));
        json_object_set_new(rootJ, "extClockResolution", json_integer(clockResChoice));
        json_object_set_new(rootJ, "chaosKnobMode", json_integer(chaosKnobMode));
        json_object_set_new(rootJ, "runMode", json_integer(runMode));
        json_object_set_new(rootJ, "panelStyle", json_integer(panelStyle));
        json_object_set_new(rootJ, "running", json_integer(running));
        return rootJ;
    }

    void dataFromJson(json_t *rootJ) override {
        json_t *sequencerModeJ = json_object_get(rootJ, "sequencerMode");
        if (sequencerModeJ) {
            sequencerModeChoice = json_integer_value(sequencerModeJ);
		}

        json_t *triggerOutputModeJ = json_object_get(rootJ, "triggerOutputMode");
		if (triggerOutputModeJ) {
			triggerOutputMode = (UGraph::TriggerOutputMode) json_integer_value(triggerOutputModeJ);
		}

        json_t *accOutputModeJ = json_object_get(rootJ, "accOutputMode");
		if (accOutputModeJ) {
			accOutputMode = (UGraph::AccOutputMode) json_integer_value(accOutputModeJ);
            switch(accOutputMode) {
                case INDIVIDUAL_ACCENTS:
                    grids.setAccentAltMode(false);
                    break;
                case ACC_CLK_RST:
                    grids.setAccentAltMode(true);
            }
		}

        json_t *extClockResolutionJ = json_object_get(rootJ, "extClockResolution");
		if (extClockResolutionJ) {
			clockResChoice = json_integer_value(extClockResolutionJ);
            grids.reset();
		}

        json_t *chaosKnobModeJ = json_object_get(rootJ, "chaosKnobMode");
		if (chaosKnobModeJ) {
			chaosKnobMode = (UGraph::ChaosKnobMode) json_integer_value(chaosKnobModeJ);
		}

        json_t *runModeJ = json_object_get(rootJ, "runMode");
		if (runModeJ) {
			runMode = (UGraph::RunMode) json_integer_value(runModeJ);
		}

        json_t *panelStyleJ = json_object_get(rootJ, "panelStyle");
        if (panelStyleJ) {
            panelStyle = (int)json_integer_value(panelStyleJ);
        }

        json_t *runningJ = json_object_get(rootJ, "running");
        if (runningJ) {
            running = (int)json_integer_value(runningJ);
        }
	}

    void step() override;
    void onSampleRateChange() override;
    void updateUI();
    void updateOutputs();
    void onReset() override;
};

void UGraph::step() {
    if(runMode == TOGGLE) {
        if (runButtonTrig.process(params[RUN_BUTTON_PARAM].getValue()) ||
            runInputTrig.process(inputs[RUN_INPUT].getVoltage())) {
            if(runMode == TOGGLE){
                running = !running;
            }
        }
    }
    else {
        running = params[RUN_BUTTON_PARAM].getValue() + inputs[RUN_INPUT].getVoltage();
        if(running == 0) {
            metro.reset();
        }
    }
    lights[RUNNING_LIGHT].value = running ? 1.0 : 0.0;

    if(resetButtonTrig.process(params[RESET_BUTTON_PARAM].getValue()) ||
        resetTrig.process(inputs[RESET_INPUT].getVoltage())) {
        grids.reset();
        metro.reset();
        resetLed.trigger();
        seqStep = 0;
        elapsedTicks = 0;
    }


    switch(sequencerModeChoice) {
        case 0:
            grids.setPatternMode(PATTERN_OLIVIER);
            inEuclideanMode = 0;
            break;
        case 1:
            grids.setPatternMode(PATTERN_HENRI);
            inEuclideanMode = 0;
            break;
        case 2:
            grids.setPatternMode(PATTERN_EUCLIDEAN);
            inEuclideanMode = 1;
            break;
    }

    // Clock, tempo and swing
    tempoParam = params[TEMPO_PARAM].getValue();
    *tempo = rescale(tempoParam, 0.01f, 1.f, 40.f, 240.f);

    swing = clamp(params[SWING_PARAM].getValue() + inputs[SWING_CV].getVoltage() / 10.f, 0.f, 0.9f);
    swingHighTempo = *tempo / (1 - swing);
    swingLowTempo = *tempo / (1 + swing);
    if(elapsedTicks < 6) {
        metro.setTempo(swingLowTempo);
    }
    else {
        metro.setTempo(swingHighTempo);
    }

    if(clockResChoice != prevClockResChoice) {
        prevClockResChoice = clockResChoice;
        grids.reset();
    }

    // External clock select
    if(tempoParam < 0.01) {
        clockBPM = "Ext.";
        if(initExtReset) {
            grids.reset();
            initExtReset = false;
        }
        numTicks = ticks_granularity[clockResChoice];
        extClock = true;
    }
    else {
        initExtReset = true;
        numTicks = ticks_granularity[2];
        extClock = false;
        metro.process();
    }

    *mapX = params[MAPX_PARAM].getValue() + (inputs[MAPX_CV].getVoltage() / 10.f);
    *mapX = clamp(*mapX, 0.f, 1.f);
    *mapY = params[MAPY_PARAM].getValue() + (inputs[MAPY_CV].getVoltage() / 10.f);
    *mapY = clamp(*mapY, 0.f, 1.f);
    BDFill = params[BD_DENS_PARAM].getValue() + (inputs[BD_FILL_CV].getVoltage() / 10.f);
    BDFill = clamp(BDFill, 0.f, 1.f);
    SNFill = params[SN_DENS_PARAM].getValue() + (inputs[SN_FILL_CV].getVoltage() / 10.f);
    SNFill = clamp(SNFill, 0.f, 1.f);
    HHFill = params[HH_DENS_PARAM].getValue() + (inputs[HH_FILL_CV].getVoltage() / 10.f);
    HHFill = clamp(HHFill, 0.f, 1.f);
    *chaos = params[CHAOS_PARAM].getValue() + (inputs[CHAOS_CV].getVoltage() / 10.f);
    *chaos = clamp(*chaos, 0.f, 1.f);

    if(running) {
        if(extClock) {
            if(clockTrig.process(inputs[CLOCK_INPUT].getVoltage())) {
                advStep = true;
            }
        }
        else if(metro.hasTicked()){
            advStep = true;
            elapsedTicks++;
            elapsedTicks %= 12;
        }
        else {
            advStep = false;
        }

        grids.setMapX((uint8_t)(*mapX * 255.0));
        grids.setMapY((uint8_t)(*mapY * 255.0));
        grids.setBDDensity((uint8_t)(BDFill * 255.0));
        grids.setSDDensity((uint8_t)(SNFill * 255.0));
        grids.setHHDensity((uint8_t)(HHFill * 255.0));
        grids.setRandomness((uint8_t)(*chaos * 255.0));

        grids.setEuclideanLength(0, (uint8_t)(*mapX * 255.0));
        grids.setEuclideanLength(1, (uint8_t)(*mapY * 255.0));
        grids.setEuclideanLength(2, (uint8_t)(*chaos * 255.0));
    }

    if(advStep) {
        grids.tick(numTicks);
        for(int i = 0; i < 6; ++i) {
            if(grids.getDrumState(i)) {
                drumTriggers[i].trigger();
                gateState[i] = true;
                if(i < 3) {
                    drumLED[i].trigger();
                }
            }
        }
        seqStep++;
        if(seqStep >= 32) {
            seqStep = 0;
        }
        advStep = false;
    }
    updateOutputs();
    updateUI();
}

void UGraph::updateUI() {
    resetLed.process();
    for(int i = 0; i < 3; ++i) {
        drumLED[i].process();
        if(drumLED[i].getState() == 1) {
            lights[drumLEDIds[i]].value = 1.0;
        }
        else {
            lights[drumLEDIds[i]].value = 0.0;
        }
    }
    if(resetLed.getState() == 1) {
        lights[RESET_LIGHT].value = 1.0;
    }
    else {
        lights[RESET_LIGHT].value = 0.0;
    }
}

void UGraph::updateOutputs() {
    if(triggerOutputMode == PULSE) {
        for(int i = 0; i < 6; ++i) {
            drumTriggers[i].process();
            if(drumTriggers[i].getState()) {
                outputs[outIDs[i]].setVoltage(10);
            }
            else {
                outputs[outIDs[i]].setVoltage(0);
            }
        }
    }
    else if(extClock && triggerOutputMode == GATE) {
        for(int i = 0; i < 6; ++i) {
            if(inputs[CLOCK_INPUT].getVoltage() > 0 && gateState[i]) {
                gateState[i] = false;
                outputs[outIDs[i]].setVoltage(10);
            }
            if(inputs[CLOCK_INPUT].getVoltage() <= 0) {
                outputs[outIDs[i]].setVoltage(0);
            }
        }
    }
    else {
        for(int i = 0; i < 6; ++i) {
            if(metro.getElapsedTickTime() < 0.5 && gateState[i]) {
                outputs[outIDs[i]].setVoltage(10);
            }
            else {
                outputs[outIDs[i]].setVoltage(0);
                gateState[i] = false;
            }
        }
    }
}

void UGraph::onSampleRateChange() {
    float sampleRate = APP->engine->getSampleRate();
    metro.setSampleRate(sampleRate);
    for(int i = 0; i < 3; ++i) {
        drumLED[i].setSampleRate(sampleRate);
    }
    resetLed.setSampleRate(sampleRate);
    for(int i = 0; i < 6; ++i) {
        drumTriggers[i].setSampleRate(sampleRate);
    }
}

void UGraph::onReset() {
    running = false;
}

// The widget

struct PanelBorder : TransparentWidget {
	void draw(const DrawArgs &args) override {
		NVGcolor borderColor = nvgRGBAf(0.5, 0.5, 0.5, 0.5);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.5, 0.5, box.size.x - 1.0, box.size.y - 1.0);
		nvgStrokeColor(args.vg, borderColor);
		nvgStrokeWidth(args.vg, 1.0);
		nvgStroke(args.vg);
	}
};

struct UGraphDynamicText : TransparentWidget {
    std::string oldText;
    std::string* pText;
    std::shared_ptr<Font> font;
    int size;
    NVGcolor drawColour;
    int* visibility;
    DynamicViewMode viewMode;

    enum Colour {
        COLOUR_WHITE,
        COLOUR_BLACK
    };
    int* colourHandle;

    UGraphDynamicText() {
        font = APP->window->loadFont(asset::plugin(pluginInstance, "res/din1451alt.ttf"));
        size = 16;
        visibility = nullptr;
        pText = nullptr;
        viewMode = ACTIVE_HIGH_VIEW;
    }

    void draw(const DrawArgs &args) {
        nvgFontSize(args.vg, size);
        nvgFontFaceId(args.vg, font->handle);
        nvgTextLetterSpacing(args.vg, 0.f);
        Vec textPos = Vec(0.f, 0.f);
        if(colourHandle != nullptr) {
            switch(*colourHandle) {
                case COLOUR_BLACK : drawColour = nvgRGB(0x00,0x00,0x00); break;
                case COLOUR_WHITE : drawColour = nvgRGB(0xFF,0xFF,0xFF); break;
                default : drawColour = nvgRGB(0x00,0x00,0x00);
            }
        }
        else {
            drawColour = nvgRGB(0x00,0x00,0x00);
        }

        nvgFillColor(args.vg, drawColour);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        if(pText != nullptr) {
            nvgText(args.vg, textPos.x, textPos.y, pText->c_str(), NULL);
        }
    }

    void step() {
        if(visibility != nullptr) {
            if(*visibility) {
                visible = true;
            }
            else {
                visible = false;
            }
            if(viewMode == ACTIVE_LOW_VIEW) {
                visible = !visible;
            }
        }
        // else {
        //     visible = true;
        // }
    }
};

UGraphDynamicText* createUGraphDynamicText(const Vec& pos, int size, int* colourHandle, std::string* pText,
                                           int* visibilityHandle, DynamicViewMode viewMode) {
    UGraphDynamicText* dynText = new UGraphDynamicText();
    dynText->size = size;
    dynText->colourHandle = colourHandle;
    dynText->pText = pText;
    dynText->box.pos = pos;
    dynText->box.size = Vec(82,14);
    dynText->visibility = visibilityHandle;
    dynText->viewMode = viewMode;
    return dynText;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Context Menu ///////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

struct UGraphWidget : ModuleWidget {
    const std::string seqModeItemText[3] = {"Olivier", "Henri", "Euclid"};
    const std::string clockResText[3] = {"4 PPQN", "8 PPQN", "24 PPQN"};
    SvgPanel* lightPanel;
    UGraphWidget(UGraph *module);
    void appendContextMenu(Menu* menu) override;
    // void step() override;
};

UGraphWidget::UGraphWidget(UGraph *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/UGraphPanelLight.svg")));

    // if(module) {
    //     lightPanel = new SvgPanel;
    //     lightPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/UGraphPanelLight.svg")));
    //     lightPanel->visible = false;
    //     addChild(lightPanel);
    // }

    addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    auto floatToTempoText = [](float a){
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << a;
        if(a >= 40.0) {
            return stream.str();
        }
        std::string out = "Ext.";
        return out;
    };
    auto floatToEuclideanText = [](float a){
        return std::to_string(((uint8_t)(a * 255.0) >> 3) + 1);
    };

    // Tempo text
    if(module) {
        std::shared_ptr<float> i = module->tempo;
        DynamicValueText<float>* vText = new DynamicValueText<float>(i, floatToTempoText);
        vText->box.pos = Vec(53, 66.75);
        vText->size = 14;
        vText->viewMode = ACTIVE_HIGH_VIEW;
        vText->colorHandle = &module->panelStyle;
        addChild(vText);
    }

    // Map X Text
    if(module) {
        addChild(createDynamicText(Vec(53, 163), 14, "Map X", &module->inEuclideanMode,
                                 &module->panelStyle, ACTIVE_LOW_VIEW));
        std::shared_ptr<float> i = module->mapX;
        DynamicValueText<float>* vText = new DynamicValueText<float>(i, floatToEuclideanText);
        vText->box.pos = Vec(53, 163);
        vText->size = 14;
        vText->viewMode = ACTIVE_HIGH_VIEW;
        vText->visibility = &module->inEuclideanMode;
        vText->colorHandle = &module->panelStyle;
        addChild(vText);
    }

    // Map Y Text
    if(module) {
        addChild(createDynamicText(Vec(89, 163), 14, "Map Y", &module->inEuclideanMode,
                                   &module->panelStyle, ACTIVE_LOW_VIEW));

        std::shared_ptr<float> i = module->mapY;
        DynamicValueText<float>* vText = new DynamicValueText<float>(i, floatToEuclideanText);
        vText->box.pos = Vec(89, 163);
        vText->size = 14;
        vText->viewMode = ACTIVE_HIGH_VIEW;
        vText->visibility = &module->inEuclideanMode;
        vText->colorHandle = &module->panelStyle;
        addChild(vText);
    }

    // Chaos Text
    if(module) {
        addChild(createDynamicText(Vec(125, 163), 14, "Chaos", &module->inEuclideanMode,
                                 &module->panelStyle, ACTIVE_LOW_VIEW));

        std::shared_ptr<float> i = module->chaos;
        DynamicValueText<float>* vText = new DynamicValueText<float>(i, floatToEuclideanText);
        vText->box.pos = Vec(125, 163);
        vText->size = 14;
        vText->viewMode = ACTIVE_HIGH_VIEW;
        vText->visibility = &module->inEuclideanMode;
        vText->colorHandle = &module->panelStyle;
        addChild(vText);
    }

    addParam(createParam<RoganMedBlue>(Vec(36.5, 30.15), module, UGraph::TEMPO_PARAM));
    addParam(createParam<RoganSmallWhite>(Vec(43.5, 137), module, UGraph::MAPX_PARAM));
    addParam(createParam<RoganSmallWhite>(Vec(79.5, 137), module, UGraph::MAPY_PARAM));
    addParam(createParam<RoganSmallWhite>(Vec(115.5, 137), module, UGraph::CHAOS_PARAM));
    addParam(createParam<RoganSmallBrightRed>(Vec(43.5, 217.65), module, UGraph::BD_DENS_PARAM));
    addParam(createParam<RoganSmallOrange>(Vec(79.5, 217.65), module, UGraph::SN_DENS_PARAM));
    addParam(createParam<RoganSmallYellow>(Vec(115.5, 217.65), module, UGraph::HH_DENS_PARAM));
    addParam(createParam<RoganMedWhite>(Vec(108.5, 30.15), module, UGraph::SWING_PARAM));

    addInput(createInput<PJ301MDarkSmall>(Vec(8.0, 35.5), module, UGraph::CLOCK_INPUT));
    addInput(createInput<PJ301MDarkSmall>(Vec(8.0, 214), module, UGraph::RESET_INPUT));
    addInput(createInput<PJ301MDarkSmall>(Vec(42.5, 186), module, UGraph::MAPX_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(78.5, 186), module, UGraph::MAPY_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(114.5, 186), module, UGraph::CHAOS_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(42.5, 261.5), module, UGraph::BD_FILL_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(78.5, 261.5), module, UGraph::SN_FILL_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(114.5, 261.5), module, UGraph::HH_FILL_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(78.5, 35.5), module, UGraph::SWING_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(8.0, 133.35), module, UGraph::RUN_INPUT));

    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(42.5, 299.736), module, UGraph::BD_OUTPUT));
    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(78.5, 299.736), module, UGraph::SN_OUTPUT));
    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(114.5, 299.736), module, UGraph::HH_OUTPUT));
    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(42.5, 327.736), module, UGraph::BD_ACC_OUTPUT));
    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(78.5, 327.736), module, UGraph::SN_ACC_OUTPUT));
    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(114.5, 327.736), module, UGraph::HH_ACC_OUTPUT));

    addChild(createLight<SmallLight<RedLight>>(Vec(50.1, 247), module, UGraph::BD_LIGHT));
    addChild(createLight<SmallLight<RedLight>>(Vec(86.1, 247), module, UGraph::SN_LIGHT));
    addChild(createLight<SmallLight<RedLight>>(Vec(122.1, 247), module, UGraph::HH_LIGHT));

    addParam(createParam<LightLEDButton>(Vec(12.1, 189.6), module, UGraph::RESET_BUTTON_PARAM));
    addChild(createLight<MediumLight<RedLight>>(Vec(14.5, 192), module, UGraph::RESET_LIGHT));
    addParam(createParam<LightLEDButton>(Vec(12.1, 107), module, UGraph::RUN_BUTTON_PARAM));
    addChild(createLight<MediumLight<RedLight>>(Vec(14.5, 109.5), module, UGraph::RUNNING_LIGHT));

    if(module) {
        std::vector<std::string> seqModeItems(seqModeItemText, seqModeItemText + 3);
        addChild(createDynamicChoice(Vec(90, 88), 55.f, seqModeItems, &module->sequencerModeChoice, nullptr, ACTIVE_LOW_VIEW));
        std::vector<std::string> clockResItems(clockResText, clockResText + 3);
        addChild(createDynamicChoice(Vec(90, 111), 55.f, clockResItems, &module->clockResChoice, nullptr, ACTIVE_LOW_VIEW));
    }
}

struct UGraphPanelStyleItem : MenuItem {
    UGraph* module;
    int panelStyle;
    void onAction(event::Action &e) override {
        module->panelStyle = panelStyle;
    }
    void step() override {
        rightText = (module->panelStyle == panelStyle) ? "✔" : "";
        MenuItem::step();
    }
};

struct UGraphTriggerOutputModeItem : MenuItem {
    UGraph* module;
    UGraph::TriggerOutputMode triggerOutputMode;
    void onAction(event::Action &e) override {
        module->triggerOutputMode = triggerOutputMode;
    }
    void step() override {
        rightText = (module->triggerOutputMode == triggerOutputMode) ? "✔" : "";
        MenuItem::step();
    }
};

struct UGraphAccOutputModeItem : MenuItem {
    UGraph* module;
    UGraph::AccOutputMode accOutputMode;
    void onAction(event::Action &e) override {
        module->accOutputMode = accOutputMode;
        switch(accOutputMode) {
            case UGraph::INDIVIDUAL_ACCENTS:
                module->grids.setAccentAltMode(false);
                break;
            case UGraph::ACC_CLK_RST:
                module->grids.setAccentAltMode(true);
        }
    }
    void step() override {
        rightText = (module->accOutputMode == accOutputMode) ? "✔" : "";
        MenuItem::step();
    }
};

struct UGraphRunModeItem : MenuItem {
    UGraph* module;
    UGraph::RunMode runMode;
    void onAction(event::Action &e) override {
        module->runMode = runMode;
    }
    void step() override {
        rightText = (module->runMode == runMode) ? "✔" : "";
        MenuItem::step();
    }
};

void UGraphWidget::appendContextMenu(Menu *menu) {
    UGraph *module = dynamic_cast<UGraph*>(this->module);
    assert(module);

    // // Panel style
    // menu->addChild(construct<MenuLabel>());
    // menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Panel style"));
    // menu->addChild(construct<UGraphPanelStyleItem>(&MenuItem::text, "Dark", &UGraphPanelStyleItem::module,
    //                                                   module, &UGraphPanelStyleItem::panelStyle, 0));
    // menu->addChild(construct<UGraphPanelStyleItem>(&MenuItem::text, "Light", &UGraphPanelStyleItem::module,
    //                                                   module, &UGraphPanelStyleItem::panelStyle, 1));

    // Trigger Output Modes
    menu->addChild(construct<MenuLabel>());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Trigger Output Mode"));
    menu->addChild(construct<UGraphTriggerOutputModeItem>(&MenuItem::text, "Short Pulse", &UGraphTriggerOutputModeItem::module,
                                                             module, &UGraphTriggerOutputModeItem::triggerOutputMode, UGraph::PULSE));
    menu->addChild(construct<UGraphTriggerOutputModeItem>(&MenuItem::text, "Gate", &UGraphTriggerOutputModeItem::module,
                                                             module, &UGraphTriggerOutputModeItem::triggerOutputMode, UGraph::GATE));

    // Acc Output Modes
    menu->addChild(construct<MenuLabel>());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Accent Output Mode"));
    menu->addChild(construct<UGraphAccOutputModeItem>(&MenuItem::text, "Individual accents", &UGraphAccOutputModeItem::module,
                                                         module, &UGraphAccOutputModeItem::accOutputMode, UGraph::INDIVIDUAL_ACCENTS));
    menu->addChild(construct<UGraphAccOutputModeItem>(&MenuItem::text, "Accent / Clock / Reset", &UGraphAccOutputModeItem::module,
                                                         module, &UGraphAccOutputModeItem::accOutputMode, UGraph::ACC_CLK_RST));

    // Run Mode
    menu->addChild(construct<MenuLabel>());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Run Mode"));
    menu->addChild(construct<UGraphRunModeItem>(&MenuItem::text, "Toggle", &UGraphRunModeItem::module,
                                                   module, &UGraphRunModeItem::runMode, UGraph::RunMode::TOGGLE));
    menu->addChild(construct<UGraphRunModeItem>(&MenuItem::text, "Momentary", &UGraphRunModeItem::module,
                                                   module, &UGraphRunModeItem::runMode, UGraph::RunMode::MOMENTARY));
}

// void UGraphWidget::step() {
//     if(module) {
//         if(dynamic_cast<UGraph*>(module)->panelStyle == 1) {
//             panel->visible = false;
//             lightPanel->visible = true;
//         }
//         else {
//             panel->visible = true;
//             lightPanel->visible = false;
//         }
//     }
//     Widget::step();
// }

Model *modelUGraph = createModel<UGraph, UGraphWidget>("uGraph");
