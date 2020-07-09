#include "ValleyWidgets.hpp"

// DynamicKnob::DynamicKnob() {
// 	canCache = false;
// 	canSquash = true;

// 	shadow = new CircularShadow;
// 	addChild(shadow);
// 	shadow->box.size = math::Vec();

// 	tw = new widget::TransformWidget;
// 	addChild(tw);

// 	sw = new widget::SvgWidget;
// 	addChild(sw);

//     _visibility = nullptr;
//     _viewMode = ACTIVE_HIGH_VIEW;
// }

// void DynamicKnob::setSvg(std::shared_ptr<Svg> svg) {
// 	sw->svg = svg;
// 	sw->wrap();
// 	tw->box.size = sw->box.size;
// 	box.size = sw->box.size;
//     shadow->box.size = sw->box.size;
// 	shadow->box.pos = Vec(0, sw->box.size.y * 0.1);
// }

/*void DynamicKnob::step() {
	// Re-transform TransformWidget if dirty
    if(_visibility != nullptr) {
        if(*_visibility) {
            visible = true;
        }
        else {
            visible = false;
        }
        if(_viewMode == ACTIVE_LOW_VIEW) {
            visible = !visible;
        }
    }
    else {
        visible = true;
    }
	fb->step();
}*/

/*void DynamicKnob::onChange(const event::Change &e) {
	if (paramQuantity) {
		float angle;
		if (paramQuantity->isBounded()) {
			angle = math::rescale(paramQuantity->getScaledValue(), 0.f, 1.f, minAngle, maxAngle);
		}
		else {
			angle = math::rescale(paramQuantity->getValue(), -1.f, 1.f, minAngle, maxAngle);
		}
		angle = std::fmod(angle, 2*M_PI);
		tw->identity();
		// Rotate Svg
		math::Vec center = sw->box.getCenter();
		tw->translate(center);
		tw->rotate(angle);
		tw->translate(center.neg());
		fb->dirty = true;
	}
	Knob::onChange(e);
}*/

////////////////////////////////////////////////////////////////////////////////

DynamicSvgKnob::DynamicSvgKnob() {
    _visibility = nullptr;
    _viewMode = ACTIVE_HIGH_VIEW;
}

void DynamicSvgKnob::step() {
    if(_visibility != nullptr) {
        int v = *_visibility;
        if(_viewMode == ACTIVE_LOW_VIEW)
            v = !v;
        if (v != _visible) {
            visible = _visible = v;
            parent->dirty = true;
        }
    }
    // else {
    //     visible = true;
    // }
 //    if (paramQuantity) {
	// 	float value = paramQuantity->getValue();
	// 	// Trigger change event when paramQuantity value changes
	// 	if (value != dirtyValue) {
	// 		dirtyValue = value;
	// 		event::Change eChange;
	// 		onChange(eChange);
	// 	}
	// }

	SvgKnob::step();
}
