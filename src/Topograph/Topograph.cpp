//
// Topograph.cpp
// Author: Dale Johnson
// Contact: valley.audio.soft@gmail.com
// Date: 5/12/2017
//
// Topograph, a port of "Mutable Instruments Grids" for VCV Rack
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
#include "TopographPatternGenerator.hpp"
#include <iomanip> // setprecision
#include <sstream> // stringstream

struct Topograph : Module {
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
    SequencerMode sequencerMode = HENRI;
    int inEuclideanMode = 0;

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
    int textVisible = 1;

    Topograph() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(Topograph::TEMPO_PARAM, 0.0, 1.0, 0.406, "Tempo", "BPM", 0.f, 202.020202, 37.979797);
        configParam(Topograph::MAPX_PARAM, 0.0, 1.0, 0.0, "Pattern Map X");
        configParam(Topograph::MAPY_PARAM, 0.0, 1.0, 0.0, "Pattern Map Y");
        configParam(Topograph::CHAOS_PARAM, 0.0, 1.0, 0.0, "Pattern Chaos");
        configParam(Topograph::BD_DENS_PARAM, 0.0, 1.0, 0.5, "Channel 1 Density");
        configParam(Topograph::SN_DENS_PARAM, 0.0, 1.0, 0.5, "Channel 2 Density");
        configParam(Topograph::HH_DENS_PARAM, 0.0, 1.0, 0.5, "Channel 3 Density");
        configParam(Topograph::SWING_PARAM, 0.0, 0.9, 0.0, "Swing");
        configParam(Topograph::RESET_BUTTON_PARAM, 0.0, 1.0, 0.0, "Reset");
        configParam(Topograph::RUN_BUTTON_PARAM, 0.0, 1.0, 0.0, "Run");

        metro = Metronome(120, APP->engine->getSampleRate(), 24.0, 0.0);
        numTicks = ticks_granularity[2];
        srand(time(NULL));
        BDLed = Oneshot(0.1, APP->engine->getSampleRate());
        SNLed = Oneshot(0.1, APP->engine->getSampleRate());
        HHLed = Oneshot(0.1, APP->engine->getSampleRate());
        resetLed = Oneshot(0.1, APP->engine->getSampleRate());

        for(int i = 0; i < 6; ++i) {
            drumTriggers[i] = Oneshot(0.008, APP->engine->getSampleRate());
            gateState[i] = false;
        }
        for(int i = 0; i < 3; ++i) {
            drumLED[i] = Oneshot(0.1, APP->engine->getSampleRate());
        }
        panelStyle = 1;
    }

    json_t *dataToJson() override {
        json_t *rootJ = json_object();
        json_object_set_new(rootJ, "sequencerMode", json_integer(sequencerMode));
        json_object_set_new(rootJ, "triggerOutputMode", json_integer(triggerOutputMode));
        json_object_set_new(rootJ, "accOutputMode", json_integer(accOutputMode));
        json_object_set_new(rootJ, "extClockResolution", json_integer(extClockResolution));
        json_object_set_new(rootJ, "chaosKnobMode", json_integer(chaosKnobMode));
        json_object_set_new(rootJ, "runMode", json_integer(runMode));
        json_object_set_new(rootJ, "panelStyle", json_integer(panelStyle));
        json_object_set_new(rootJ, "running", json_integer(running));
        return rootJ;
    }

    void dataFromJson(json_t *rootJ) override {
        json_t *sequencerModeJ = json_object_get(rootJ, "sequencerMode");
        if (sequencerModeJ) {
            sequencerMode = (Topograph::SequencerMode) json_integer_value(sequencerModeJ);
            inEuclideanMode = 0;
            switch(sequencerMode) {
                case HENRI:
                    grids.setPatternMode(PATTERN_HENRI);
                    break;
                case OLIVIER:
                    grids.setPatternMode(PATTERN_OLIVIER);
                    break;
                case EUCLIDEAN:
                    grids.setPatternMode(PATTERN_EUCLIDEAN);
                    inEuclideanMode = 1;
                    break;
            }
		}

        json_t *triggerOutputModeJ = json_object_get(rootJ, "triggerOutputMode");
		if (triggerOutputModeJ) {
			triggerOutputMode = (Topograph::TriggerOutputMode) json_integer_value(triggerOutputModeJ);
		}

        json_t *accOutputModeJ = json_object_get(rootJ, "accOutputMode");
		if (accOutputModeJ) {
			accOutputMode = (Topograph::AccOutputMode) json_integer_value(accOutputModeJ);
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
			extClockResolution = (Topograph::ExtClockResolution) json_integer_value(extClockResolutionJ);
            grids.reset();
		}

        json_t *chaosKnobModeJ = json_object_get(rootJ, "chaosKnobMode");
		if (chaosKnobModeJ) {
			chaosKnobMode = (Topograph::ChaosKnobMode) json_integer_value(chaosKnobModeJ);
		}

        json_t *runModeJ = json_object_get(rootJ, "runMode");
		if (runModeJ) {
			runMode = (Topograph::RunMode) json_integer_value(runModeJ);
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

    void process(const ProcessArgs &args) override;
    void onSampleRateChange() override;
    void updateUI();
    void updateOutputs();
    void onReset() override;
};

void Topograph::process(const ProcessArgs &args) {
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

    // External clock select
    if(tempoParam < 0.01) {
        if(initExtReset) {
            grids.reset();
            initExtReset = false;
        }
        numTicks = ticks_granularity[extClockResolution];
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

void Topograph::updateUI() {

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

void Topograph::updateOutputs() {
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

void Topograph::onSampleRateChange() {
    metro.setSampleRate(APP->engine->getSampleRate());
    for(int i = 0; i < 3; ++i) {
        drumLED[i].setSampleRate(APP->engine->getSampleRate());
    }
    resetLed.setSampleRate(APP->engine->getSampleRate());
    for(int i = 0; i < 6; ++i) {
        drumTriggers[i].setSampleRate(APP->engine->getSampleRate());
    }
}

void Topograph::onReset() {
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

////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Context Menu ///////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

struct TopographWidget : ModuleWidget {
    TopographWidget(Topograph *topograph);
    void appendContextMenu(Menu* menu) override;
    // void step() override;
    SvgPanel* lightPanel;
};

TopographWidget::TopographWidget(Topograph *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TopographPanelWhite.svg")));

    // if(module) {
    //     lightPanel = new SvgPanel;
    //     lightPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TopographPanelWhite.svg")));
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
        vText->box.pos = Vec(69, 83);
        vText->size = 14;
        vText->viewMode = ACTIVE_HIGH_VIEW;
        vText->colorHandle = &module->panelStyle;
        addChild(vText);
    }

    // Map X Text
    if(module) {
        addChild(createDynamicText(Vec(27.1,208.5), 14, "Map X", &module->inEuclideanMode,
                                 &module->panelStyle, ACTIVE_LOW_VIEW));
        addChild(createDynamicText(Vec(22.1,208.5), 14, "Len:", &module->inEuclideanMode,
                                   &module->panelStyle, ACTIVE_HIGH_VIEW));
        std::shared_ptr<float> i = module->mapX;
        DynamicValueText<float>* vText = new DynamicValueText<float>(i, floatToEuclideanText);
        vText->box.pos = Vec(42.1,208.5);
        vText->size = 14;
        vText->viewMode = ACTIVE_HIGH_VIEW;
        vText->visibility = &module->inEuclideanMode;
        vText->colorHandle = &module->panelStyle;
        addChild(vText);
    }

    // Map Y Text
    if(module) {
        addChild(createDynamicText(Vec(27.1,268.5), 14, "Map Y", &module->inEuclideanMode,
                                   &module->panelStyle, ACTIVE_LOW_VIEW));
        addChild(createDynamicText(Vec(22.1,268.5), 14, "Len:", &module->inEuclideanMode,
                                   &module->panelStyle, ACTIVE_HIGH_VIEW));

        std::shared_ptr<float> i = module->mapY;
        DynamicValueText<float>* vText = new DynamicValueText<float>(i, floatToEuclideanText);
        vText->box.pos = Vec(42.1,268.5);
        vText->size = 14;
        vText->viewMode = ACTIVE_HIGH_VIEW;
        vText->visibility = &module->inEuclideanMode;
        vText->colorHandle = &module->panelStyle;
        addChild(vText);
    }

    // Chaos Text
    if(module) {
        addChild(createDynamicText(Vec(27.1,329), 14, "Chaos", &module->inEuclideanMode,
                                 &module->panelStyle, ACTIVE_LOW_VIEW));
        addChild(createDynamicText(Vec(22.1,329), 14, "Len:", &module->inEuclideanMode,
                                   &module->panelStyle, ACTIVE_HIGH_VIEW));

        std::shared_ptr<float> i = module->chaos;
        DynamicValueText<float>* vText = new DynamicValueText<float>(i, floatToEuclideanText);
        vText->box.pos = Vec(42.1,329);
        vText->size = 14;
        vText->viewMode = ACTIVE_HIGH_VIEW;
        vText->visibility = &module->inEuclideanMode;
        vText->colorHandle = &module->panelStyle;
        addChild(vText);
    }

    addParam(createParam<Rogan1PSBlue>(Vec(49, 40.15), module, Topograph::TEMPO_PARAM));
    addParam(createParam<Rogan1PSWhite>(Vec(49, 166.15), module, Topograph::MAPX_PARAM));
    addParam(createParam<Rogan1PSWhite>(Vec(49, 226.15), module, Topograph::MAPY_PARAM));
    addParam(createParam<Rogan1PSWhite>(Vec(49, 286.15), module, Topograph::CHAOS_PARAM));
    addParam(createParam<Rogan1PSBrightRed>(Vec(121, 40.15), module, Topograph::BD_DENS_PARAM));
    addParam(createParam<Rogan1PSOrange>(Vec(157, 103.15), module, Topograph::SN_DENS_PARAM));
    addParam(createParam<Rogan1PSYellow>(Vec(193, 166.15), module, Topograph::HH_DENS_PARAM));
    addParam(createParam<Rogan1PSWhite>(Vec(193, 40.15), module, Topograph::SWING_PARAM));

    addInput(createInput<PJ301MDarkSmall>(Vec(17.0, 50.0), module, Topograph::CLOCK_INPUT));
    addInput(createInput<PJ301MDarkSmall>(Vec(17.0, 113.0), module, Topograph::RESET_INPUT));
    addInput(createInput<PJ301MDarkSmall>(Vec(17.0, 176.0), module, Topograph::MAPX_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(17.0, 236.0), module, Topograph::MAPY_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(17.0, 296.0), module, Topograph::CHAOS_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(131.0, 236.0), module, Topograph::BD_FILL_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(167.0, 236.0), module, Topograph::SN_FILL_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(203.0, 236.0), module, Topograph::HH_FILL_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(167.0, 50.0), module, Topograph::SWING_CV));
    addInput(createInput<PJ301MDarkSmall>(Vec(74.5, 113.0), module, Topograph::RUN_INPUT));

    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(131.2, 272.536), module, Topograph::BD_OUTPUT));
    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(167.2, 272.536), module, Topograph::SN_OUTPUT));
    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(203.2, 272.536), module, Topograph::HH_OUTPUT));
    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(131.2, 308.536), module, Topograph::BD_ACC_OUTPUT));
    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(167.2, 308.536), module, Topograph::SN_ACC_OUTPUT));
    addOutput(createOutput<PJ301MDarkSmallOut>(Vec(203.2, 308.536), module, Topograph::HH_ACC_OUTPUT));

    addChild(createLight<SmallLight<RedLight>>(Vec(138.6, 218), module, Topograph::BD_LIGHT));
    addChild(createLight<SmallLight<RedLight>>(Vec(174.6, 218), module, Topograph::SN_LIGHT));
    addChild(createLight<SmallLight<RedLight>>(Vec(210.6, 218), module, Topograph::HH_LIGHT));

    addParam(createParam<LightLEDButton>(Vec(55, 116), module, Topograph::RESET_BUTTON_PARAM));
    addChild(createLight<MediumLight<RedLight>>(Vec(57.5, 118.5), module, Topograph::RESET_LIGHT));
    addParam(createParam<LightLEDButton>(Vec(112, 116), module, Topograph::RUN_BUTTON_PARAM));
    addChild(createLight<MediumLight<RedLight>>(Vec(114.5, 118.5), module, Topograph::RUNNING_LIGHT));
}

struct TopographPanelStyleItem : MenuItem {
    Topograph* module;
    int panelStyle;
    void onAction(event::Action &e) override {
        module->panelStyle = panelStyle;
    }
    void step() override {
        rightText = (module->panelStyle == panelStyle) ? "✔" : "";
        MenuItem::step();
    }
};

struct TopographSequencerModeItem : MenuItem {
    Topograph* module;
    Topograph::SequencerMode sequencerMode;
    void onAction(event::Action &e) override {
        module->sequencerMode = sequencerMode;
        module->inEuclideanMode = 0;
        switch(sequencerMode) {
            case Topograph::HENRI:
                module->grids.setPatternMode(PATTERN_HENRI);
                break;
            case Topograph::OLIVIER:
                module->grids.setPatternMode(PATTERN_OLIVIER);
                break;
            case Topograph::EUCLIDEAN:
                module->grids.setPatternMode(PATTERN_EUCLIDEAN);
                module->inEuclideanMode = 1;
                break;
        }
    }
    void step() override {
        rightText = (module->sequencerMode == sequencerMode) ? "✔" : "";
        MenuItem::step();
    }
};

struct TopographTriggerOutputModeItem : MenuItem {
    Topograph* module;
    Topograph::TriggerOutputMode triggerOutputMode;
    void onAction(event::Action &e) override {
        module->triggerOutputMode = triggerOutputMode;
    }
    void step() override {
        rightText = (module->triggerOutputMode == triggerOutputMode) ? "✔" : "";
        MenuItem::step();
    }
};

struct TopographAccOutputModeItem : MenuItem {
    Topograph* module;
    Topograph::AccOutputMode accOutputMode;
    void onAction(event::Action &e) override {
        module->accOutputMode = accOutputMode;
        switch(accOutputMode) {
            case Topograph::INDIVIDUAL_ACCENTS:
                module->grids.setAccentAltMode(false);
                break;
            case Topograph::ACC_CLK_RST:
                module->grids.setAccentAltMode(true);
        }
    }
    void step() override {
        rightText = (module->accOutputMode == accOutputMode) ? "✔" : "";
        MenuItem::step();
    }
};

struct TopographClockResolutionItem : MenuItem {
    Topograph* module;
    Topograph::ExtClockResolution extClockResolution;
    void onAction(event::Action &e) override {
        module->extClockResolution = extClockResolution;
        module->grids.reset();
    }
    void step() override {
        rightText = (module->extClockResolution == extClockResolution) ? "✔" : "";
        MenuItem::step();
    }
};

struct TopographRunModeItem : MenuItem {
    Topograph* module;
    Topograph::RunMode runMode;
    void onAction(event::Action &e) override {
        module->runMode = runMode;
    }
    void step() override {
        rightText = (module->runMode == runMode) ? "✔" : "";
        MenuItem::step();
    }
};

void TopographWidget::appendContextMenu(Menu *menu) {
    Topograph *module = dynamic_cast<Topograph*>(this->module);
    assert(module);

    // // Panel style
    // menu->addChild(construct<MenuLabel>());
    // menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Panel style"));
    // menu->addChild(construct<TopographPanelStyleItem>(&MenuItem::text, "Dark", &TopographPanelStyleItem::module,
    //                                                   module, &TopographPanelStyleItem::panelStyle, 0));
    // menu->addChild(construct<TopographPanelStyleItem>(&MenuItem::text, "Light", &TopographPanelStyleItem::module,
    //                                                   module, &TopographPanelStyleItem::panelStyle, 1));

    // Sequencer Modes
    menu->addChild(construct<MenuLabel>());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Sequencer Mode"));
    menu->addChild(construct<TopographSequencerModeItem>(&MenuItem::text, "Henri", &TopographSequencerModeItem::module,
                                                         module, &TopographSequencerModeItem::sequencerMode, Topograph::HENRI));
    menu->addChild(construct<TopographSequencerModeItem>(&MenuItem::text, "Olivier", &TopographSequencerModeItem::module,
                                                         module, &TopographSequencerModeItem::sequencerMode, Topograph::OLIVIER));
    menu->addChild(construct<TopographSequencerModeItem>(&MenuItem::text, "Euclidean", &TopographSequencerModeItem::module,
                                                         module, &TopographSequencerModeItem::sequencerMode, Topograph::EUCLIDEAN));

    // Trigger Output Modes
    menu->addChild(construct<MenuLabel>());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Trigger Output Mode"));
    menu->addChild(construct<TopographTriggerOutputModeItem>(&MenuItem::text, "Short Pulse", &TopographTriggerOutputModeItem::module,
                                                             module, &TopographTriggerOutputModeItem::triggerOutputMode, Topograph::PULSE));
    menu->addChild(construct<TopographTriggerOutputModeItem>(&MenuItem::text, "Gate", &TopographTriggerOutputModeItem::module,
                                                             module, &TopographTriggerOutputModeItem::triggerOutputMode, Topograph::GATE));

    // Acc Output Modes
    menu->addChild(construct<MenuLabel>());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Accent Output Mode"));
    menu->addChild(construct<TopographAccOutputModeItem>(&MenuItem::text, "Individual accents", &TopographAccOutputModeItem::module,
                                                         module, &TopographAccOutputModeItem::accOutputMode, Topograph::INDIVIDUAL_ACCENTS));
    menu->addChild(construct<TopographAccOutputModeItem>(&MenuItem::text, "Accent / Clock / Reset", &TopographAccOutputModeItem::module,
                                                         module, &TopographAccOutputModeItem::accOutputMode, Topograph::ACC_CLK_RST));

    // External clock resolution
    menu->addChild(construct<MenuLabel>());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Ext. Clock Resolution"));
    menu->addChild(construct<TopographClockResolutionItem>(&MenuItem::text, "4 PPQN", &TopographClockResolutionItem::module,
                                                           module, &TopographClockResolutionItem::extClockResolution, Topograph::EXTCLOCK_RES_4_PPQN));
    menu->addChild(construct<TopographClockResolutionItem>(&MenuItem::text, "8 PPQN", &TopographClockResolutionItem::module,
                                                           module, &TopographClockResolutionItem::extClockResolution, Topograph::EXTCLOCK_RES_8_PPQN));
    menu->addChild(construct<TopographClockResolutionItem>(&MenuItem::text, "24 PPQN", &TopographClockResolutionItem::module,
                                                           module, &TopographClockResolutionItem::extClockResolution, Topograph::EXTCLOCK_RES_24_PPQN));

    // Acc Output Modes
    menu->addChild(construct<MenuLabel>());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Run Mode"));
    menu->addChild(construct<TopographRunModeItem>(&MenuItem::text, "Toggle", &TopographRunModeItem::module,
                                                   module, &TopographRunModeItem::runMode, Topograph::RunMode::TOGGLE));
    menu->addChild(construct<TopographRunModeItem>(&MenuItem::text, "Momentary", &TopographRunModeItem::module,
                                                   module, &TopographRunModeItem::runMode, Topograph::RunMode::MOMENTARY));
}

// void TopographWidget::step() {
//     if(module) {
//         if(dynamic_cast<Topograph*>(module)->panelStyle == 1) {
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

Model *modelTopograph = createModel<Topograph, TopographWidget>("Topograph");
