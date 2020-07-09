#include "ValleyWidgets.hpp"

DynamicSwitchWidget::DynamicSwitchWidget() {
    _visibility = nullptr;
    _viewMode = ACTIVE_HIGH_VIEW;
}

void DynamicSwitchWidget::step() {
    if(_visibility != nullptr) {
        int v = *_visibility;
        if(_viewMode == ACTIVE_LOW_VIEW)
            v = !v;
        if (v != _visible) {
            visible = _visible = v;
            parent->dirty = true;
        }
    }
    SvgSwitch::step();
}
