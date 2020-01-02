////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CS_GRAPHICS_TEXTURE_TRACER_CPU_HPP
#define CS_GRAPHICS_TEXTURE_TRACER_CPU_HPP

#include "BodyProperties.hpp"
#include "Photon.hpp"
#include "TextureTracer.hpp"

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace cs::graphics {
class TextureTracerCPU : public TextureTracer {
 public:
  std::vector<DoublePixel> traceThroughTexture(
      std::variant<GPUBuffer, CPUBuffer> const& photonBuffer, BodyWithAtmosphere const& body);
};
} // namespace cs::graphics

#endif // CS_GRAPHICS_TEXTURE_TRACER_CPU_HPP