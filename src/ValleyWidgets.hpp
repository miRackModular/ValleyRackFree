#ifndef DSJ_VALLEY_WIDGETS_HPP
#define DSJ_VALLEY_WIDGETS_HPP

#include "Valley.hpp"
#include "window.hpp"
#include <functional>

// Dynamic Panel

struct PanelBorderWidget : TransparentWidget {
	void draw(const DrawArgs &args) override;
};

struct DynamicPanelWidget : FramebufferWidget {
    int* mode;
    int oldMode;
    std::vector<std::shared_ptr<Svg>> panels;
    SvgWidget* visiblePanel;
    PanelBorderWidget* border;

    DynamicPanelWidget();
    void addPanel(std::shared_ptr<Svg> svg);
    // void step() override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// View mode

enum DynamicViewMode {
    ACTIVE_HIGH_VIEW,
    ACTIVE_LOW_VIEW,
    ALWAYS_ACTIVE_VIEW
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Dynamic Switch

struct DynamicSwitchWidget : SvgSwitch {
    int* _visibility;
    int _visible = -1;
    DynamicViewMode _viewMode;

    DynamicSwitchWidget();
    void step() override;
};

template <class TDynamicSwitch>
DynamicSwitchWidget* createDynamicSwitchWidget(Vec pos, Module *module, int paramId,
                                               float minValue, float maxValue, float defaultValue,
                                               int* visibilityHandle, DynamicViewMode viewMode) {
	DynamicSwitchWidget *dynSwitch = new TDynamicSwitch();
	dynSwitch->box.pos = pos;
    dynSwitch->module = module;
    dynSwitch->paramId = paramId;
    dynSwitch->setLimits(minValue, maxValue);
    dynSwitch->setDefaultValue(defaultValue);
    dynSwitch->_visibility = visibilityHandle;
    dynSwitch->_viewMode = viewMode;
	return dynSwitch;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Dynamic lights

struct DynamicModuleLightWidget : MultiLightWidget {
	Module *module = NULL;
	int firstLightId;
    int* visibility = nullptr;
//    int _visible = -1;
    DynamicViewMode viewMode = ACTIVE_HIGH_VIEW;

	void step() override;
};

template<class TDynamicModuleLightWidget>
DynamicModuleLightWidget *createDynamicLight(Vec pos, Module *module, int firstLightId,
                                             int* visibilityHandle, DynamicViewMode viewMode) {
	DynamicModuleLightWidget *light = new TDynamicModuleLightWidget();
	light->box.pos = pos;
	light->module = module;
	light->firstLightId = firstLightId;
    light->visibility = visibilityHandle;
    light->viewMode = viewMode;
	return light;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Dynamic knob

enum DynamicKnobMotion {
    SMOOTH_MOTION,
    SNAP_MOTION
};

// struct DynamicKnob : virtual Knob {
// 	/** Angles in radians */
// 	float minAngle, maxAngle;
// 	/** Not owned */
//     FramebufferWidget *fb;
// 	TransformWidget *tw;
// 	SvgWidget *sw;
//     CircularShadow *shadow;
//     int* _visibility;
//     DynamicViewMode _viewMode;

// 	DynamicKnob();
// 	void setSvg(std::shared_ptr<Svg> svg);
// };

struct DynamicSvgKnob : SvgKnob {
    int* _visibility;
    int _visible = -1;
    DynamicViewMode _viewMode;

	DynamicSvgKnob();
	void step() override;
};

template <class TParamWidget>
TParamWidget *createDynamicParam(math::Vec pos, engine::Module *module,
                                 int paramId, int* visibilityHandle,
                                 DynamicViewMode viewMode,
                                 DynamicKnobMotion motion) {
	TParamWidget *o = createParam<TParamWidget>(pos, module, paramId);
    o->_visibility = visibilityHandle;
    o->_viewMode = viewMode;
    if(motion == SNAP_MOTION) {
        o->snap = true;
    }
	return o;
}

template <class TParamWidget>
TParamWidget *createDynamicParam(math::Vec pos, engine::Module *module,
                                 int paramId, int* visibilityHandle,
                                 DynamicViewMode viewMode) {
    TParamWidget *o = createParam<TParamWidget>(pos, module, paramId);
    o->_visibility = visibilityHandle;
    o->_viewMode = viewMode;
	return o;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct SvgStepSlider : app::SvgSlider {
	void onChange(event::Change& e) override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Dynamic text

struct DynamicText : TransparentWidget {
    std::shared_ptr<std::string> text;
    std::shared_ptr<Font> font;
    int size;
    float blur;
    int* visibility;
    int _visible = -1;
    DynamicViewMode viewMode;

    enum ColorMode {
        COLOR_MODE_WHITE = 0,
        COLOR_MODE_BLACK,
        COLOR_MODE_RED
    };

    enum FontMode {
        FONT_MODE_ALTEDIN = 0,
        FONT_MODE_7SEG
    };

    int* colorHandle;
    NVGcolor textColor;
    NVGcolor customColor;
    NVGalign horzAlignment;
    NVGalign vertAlignment;

    DynamicText();
    virtual void draw(const DrawArgs &args) override;
    void step() override;
    void setFont(const FontMode& newFontMode);
};

DynamicText* createDynamicText(const Vec& pos, int size, std::string text,
                               int* visibilityHandle, DynamicViewMode viewMode);
DynamicText* createDynamicText(const Vec& pos, int size, std::string text,
                               int* visibilityHandle, int* colorHandle, DynamicViewMode viewMode);
DynamicText* createDynamicText(const Vec& pos, int size, std::shared_ptr<std::string> text,
                               int* visibilityHandle, DynamicViewMode viewMode);
DynamicText* createDynamicText(const Vec& pos, int size, std::shared_ptr<std::string> text,
                               int* visibilityHandle, int* colorHandle, DynamicViewMode viewMode);

struct DynamicFrameText : DynamicText {
    int* itemHandle;
    std::vector<std::string> textItem;

    DynamicFrameText();
    void addItem(const std::string& item);
    void draw(const DrawArgs &args) override;
};

template<class T>
class DynamicValueText : public TransformWidget {
public:
    std::shared_ptr<Font> font;
    int size;
    int* visibility;
    int _visible = -1;
    DynamicViewMode viewMode;

    enum ColorMode {
        COLOR_MODE_WHITE = 0,
        COLOR_MODE_BLACK
    };
    int* colorHandle;
    NVGcolor textColor;

    DynamicValueText(std::shared_ptr<T> value, std::function<std::string(T)> valueToText)  {
        font = APP->window->loadFont(asset::plugin(pluginInstance, "res/din1451alt.ttf"));
        size = 16;
        visibility = nullptr;
        colorHandle = nullptr;
        viewMode = ACTIVE_HIGH_VIEW;
        _value = value;
        _valueToText = valueToText;
    }

    void draw(const DrawArgs &args) override {
        nvgFontSize(args.vg, size);
        nvgFontFaceId(args.vg, font->handle);
        nvgTextLetterSpacing(args.vg, 0.f);
        Vec textPos = Vec(0.f, 0.f);
        if(colorHandle != nullptr) {
            switch((ColorMode)*colorHandle) {
                case COLOR_MODE_WHITE: textColor = nvgRGB(0xFF,0xFF,0xFF); break;
                case COLOR_MODE_BLACK: textColor = nvgRGB(0x14,0x14, 0x14); break;
                default: textColor = nvgRGB(0xFF,0xFF,0xFF);
            }
        }
        else {
            textColor = nvgRGB(0xFF,0xFF,0xFF);
        }

        nvgFillColor(args.vg, textColor);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgText(args.vg, textPos.x, textPos.y, _text.c_str(), NULL);
    }

    void step() override {
        if(_value == nullptr) {
            return;
        }
        _text = _valueToText(*_value);
        if(visibility != nullptr) {
            int v = *visibility;
            if(viewMode == ACTIVE_LOW_VIEW)
                v = !v;
            if (v != _visible) {
                visible = _visible = v;
                parent->dirty = true;
            }
        }
    }
private:
    std::shared_ptr<T> _value;
    std::function<std::string(T)> _valueToText;
    std::string _text;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Dynamic Choices

struct DynamicItem : MenuItem {
    unsigned long _itemNumber;
    unsigned long* _choice;
    DynamicItem(unsigned long itemNumber);
    void onAction(event::Action &e) override;
};

struct DynamicChoice : ChoiceButton {
    unsigned long* _choice;
    std::vector<std::string> _items;
    std::shared_ptr<std::string> _text;
    std::shared_ptr<Font> _font;
    int* _visibility;
    int _visible = -1;
    int _textSize;
    DynamicViewMode _viewMode;
    DynamicChoice();
    void step() override;
    void onAction(event::Action &e) override;
    void draw(const DrawArgs &args) override;
};

DynamicChoice* createDynamicChoice(const Vec& pos,
                                   float width,
                                   const std::vector<std::string>& items,
                                   unsigned long* choiceHandle,
                                   int* visibilityHandle,
                                   DynamicViewMode viewMode);

template<class T = SvgKnob>
T *createValleyKnob(Vec pos, Module *module, int paramId, float minAngle, float maxAngle, DynamicKnobMotion motion) {
    T *o = createParam<T>(pos, module, paramId);
    o->minAngle = minAngle;
    o->maxAngle = maxAngle;
    if(motion == SNAP_MOTION) {
        o->snap = true;
    }
	return o;
}

#endif
