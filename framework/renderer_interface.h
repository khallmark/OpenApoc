#pragma once

namespace OpenApoc
{

class Renderer;

class RendererFactory
{
  public:
	virtual Renderer *create() = 0;
	virtual ~RendererFactory() = default;
};

RendererFactory *getGL20RendererFactory();
RendererFactory *getGLES30RendererFactory();
#ifdef OPENAPOC_METAL
// Metal draws into the CAMetalLayer SDL created for the window. It is passed as an
// opaque pointer so callers need not be Objective-C++.
RendererFactory *getMetalRendererFactory(void *metalLayer);
#endif
}; // namespace OpenApoc
