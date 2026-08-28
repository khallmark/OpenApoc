#include "framework/renderer.h"
#include "framework/logger.h"
#include "library/sp.h"

namespace OpenApoc
{

RendererSurfaceBinding::RendererSurfaceBinding(Renderer &r, sp<Surface> s)
    : prevBinding(r.getSurface()), r(r)
{
	r.setSurface(s);
}

RendererSurfaceBinding::~RendererSurfaceBinding() { r.setSurface(prevBinding); }

Renderer::~Renderer() = default;

RendererImageData::~RendererImageData() = default;

sp<Image> RendererImageData::readBack()
{
	LogWarning("NOT IMPLEMENTED");
	return nullptr;
}

void RendererImageData::resize(Vec2<unsigned int>) {}

}; // namespace OpenApoc
