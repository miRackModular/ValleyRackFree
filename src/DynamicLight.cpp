#include "ValleyWidgets.hpp"

void DynamicModuleLightWidget::step() {
    if(visibility != nullptr) {
        int v = *visibility;
        if(viewMode == ACTIVE_LOW_VIEW)
            v = !v;
        visible = v;
        // if (v != _visible) {
        //     visible = _visible = v;
        // }
    }

    assert(module);
    assert(module->lights.size() >= firstLightId + baseColors.size());
    std::vector<float> values(baseColors.size());

    for (size_t i = 0; i < baseColors.size(); i++) {
        float value = module->lights[firstLightId + i].getBrightness();
        value = clamp(value, 0.0, 1.0);
        values[i] = value;
    }
    setValues(values);
}
