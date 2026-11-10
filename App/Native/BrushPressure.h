#pragma once
#include <Brush/BrushState.h>
#include <algorithm>

namespace congregation::native {
// The panel's opacity control has only Off and Pen Pressure. In particular,
// movement speed and acceleration never become an opacity input.
inline void configure_pressure_opacity(BrushState& brush, bool enabled,
                                       double minimum, double center, double maximum,
                                       double jitter) {
    auto& dynamics=brush.dynamics;
    dynamics.pressureToOpacity=0;
    dynamics.velocityToOpacity=0;
    dynamics.velocityToOpacityEnabled=false;
    dynamics.opacityVariation.enabled=enabled && jitter>0;
    dynamics.opacityVariation.control=BrushDynamicsControl::Off;
    dynamics.opacityVariation.jitter=std::clamp(jitter,0.,1.);
    for(auto& binding:dynamics.bindings)
        if(binding.target==BrushDynamicsTarget::Opacity && binding.source==BrushDynamicsSource::Pressure)
            binding.enabled=enabled;
    auto& response=dynamics.opacityResponse;
    response.enabled=enabled;
    response.velocity.enabled=response.tilt.enabled=response.random.enabled=false;
    response.pressure={true,std::clamp(minimum,0.,1.),std::clamp(center,0.,1.),std::clamp(maximum,0.,1.)};
}
}
