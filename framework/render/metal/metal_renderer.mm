// A Metal backend for the OpenApoc framework.
//
// This is a port of the GLES 3.0 v2 renderer rather than a redesign: the same three draw
// machines (batched sprites, single textured quads, untextured colour), the same
// stb_rect_pack spritesheet paging, and deliberately the same odd blend function. Where
// Metal's model genuinely differs the divergence is called out at the site.
//
// The three structural differences worth knowing up front:
//
//  * Y flip is unconditional here. GL flips only when drawing to the window because a GL
//    framebuffer texture's row 0 is its bottom while the window's is its top. Metal render
//    targets are top-left origin whatever they are, so one rule covers every surface.
//
//  * The default surface owns a texture rather than aliasing the drawable. Presenting blits
//    that texture to the drawable. This costs one full-screen copy per frame and buys back
//    screenshots: readBack() on the default surface is called during event processing, long
//    after the previous frame's drawable was presented and handed back to CoreAnimation.
//
//  * A new render target records a pending clear instead of being filled eagerly, and that
//    clear becomes the load action of whichever pass first touches it. This is not an
//    optimisation to tidy away: a new MTLTexture's contents are undefined, where a GL
//    framebuffer texture comes back zeroed on every driver this engine has run on, and
//    forms/control.cpp leans on that -- it renders a control into its cached surface only when
//    dirty, and onRender() need not touch every pixel. Left undefined, the untouched pixels
//    rendered as solid magenta.

#include "framework/image.h"
#include "framework/logger.h"
#include "framework/options.h"
#include "framework/palette.h"
#include "framework/renderer.h"
#include "framework/renderer_interface.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <list>
#include <mutex>
#include <vector>

#include <glm/gtx/rotate_vector.hpp>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <TargetConditionals.h>

#define STBRP_STATIC
#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_ASSERT LogAssert
#include "framework/render/gles30_v2/stb_rect_pack.h"

// Metal is normally driven from a run loop, which drains an autorelease pool once per
// iteration. OpenApoc's frame loop is a plain C++ one that never does, so autoreleased
// objects -- render pass descriptors above all, which come back from a class convenience
// constructor through a C++ return -- would accumulate for the life of the process. The
// pool is therefore pushed and popped by hand around each frame's command buffer. These are
// what @autoreleasepool compiles to; they are exported by libobjc but not publicly declared.
extern "C" void *objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void *);

namespace OpenApoc
{
namespace
{

// The drawable's format. CAMetalLayer only offers BGRA (and wide-colour variants), so every
// render target uses it -- surfaces are blitted and sampled against each other, and a blit
// requires matching formats. Sampling is unaffected: the pixel format describes memory order,
// and the shader still receives components as RGBA.
constexpr MTLPixelFormat kTargetFormat = MTLPixelFormatBGRA8Unorm;

const char *const kShaderSource = R"MSL(
#include <metal_stdlib>
using namespace metal;

// The quad every draw is built from, as a triangle strip. Matches the GL renderer's
// identity_quad so that vertex ordering, and thus the texcoord mapping, is identical.
constant float2 kQuad[4] = { float2(0, 0), float2(0, 1), float2(1, 0), float2(1, 1) };

struct Viewport
{
	float2 size;
};

// ---------------------------------------------------------------- batched sprites

struct SpriteDescription
{
	int uses_palette;
	int page;
	packed_float2 spritesheet_position;
	packed_float2 spritesheet_size;
	packed_float2 screen_position;
	packed_float2 screen_size;
	uchar4 tint;
};

struct SpriteInOut
{
	float4 position [[position]];
	float2 texcoord;
	float4 tint;
	int page [[flat]];
	int uses_palette [[flat]];
};

vertex SpriteInOut sprite_vertex(uint vid [[vertex_id]], uint iid [[instance_id]],
                                 device const SpriteDescription *sprites [[buffer(0)]],
                                 constant Viewport &vp [[buffer(1)]])
{
	device const SpriteDescription &s = sprites[iid];
	const float2 p = kQuad[vid];

	SpriteInOut out;
	out.uses_palette = s.uses_palette;
	out.page = s.page;
	out.tint = float4(s.tint) / 255.0;
	out.texcoord = float2(s.spritesheet_position) + p * float2(s.spritesheet_size);

	// Screen coords (0..viewport) to clip space, then flipped: the framework's origin is
	// top-left and Metal's clip space has +Y up.
	float2 pos = float2(s.screen_position) + p * float2(s.screen_size);
	pos = (pos / vp.size) * 2.0 - 1.0;
	out.position = float4(pos.x, -pos.y, 0.0, 1.0);
	return out;
}

fragment float4 sprite_fragment(SpriteInOut in [[stage_in]],
                                texture2d_array<uint> paletted [[texture(0)]],
                                texture2d_array<float> rgb [[texture(1)]],
                                texture2d<float> palette [[texture(2)]])
{
	if (in.uses_palette == 1)
	{
		// read(), never sample(): the paletted sheet holds indices, and filtering an index
		// is meaningless. Index 0 is the transparent colour by Apocalypse convention.
		uint idx = paletted.read(uint2(in.texcoord), uint(in.page)).r;
		if (idx == 0)
		{
			discard_fragment();
			return float4(0.0);
		}
		return palette.read(uint2(idx, 0)) * in.tint;
	}
	return rgb.read(uint2(in.texcoord), uint(in.page)) * in.tint;
}

// ---------------------------------------------------------------- single textured quad

struct TexturedVertex
{
	packed_float2 position; // already in clip space, pre-flip
	packed_float2 texcoord; // 0..1 across the quad
	uchar4 tint;
};

struct TexturedUniforms
{
	float2 tex_size;
	int uses_palette;
	int pad;
};

struct TexturedInOut
{
	float4 position [[position]];
	float2 texcoord; // in texels
	float4 tint;
};

vertex TexturedInOut textured_vertex(uint vid [[vertex_id]],
                                     device const TexturedVertex *verts [[buffer(0)]],
                                     constant TexturedUniforms &u [[buffer(1)]])
{
	TexturedInOut out;
	const float2 p = float2(verts[vid].position);
	out.position = float4(p.x, -p.y, 0.0, 1.0);
	out.texcoord = float2(verts[vid].texcoord) * u.tex_size;
	out.tint = float4(verts[vid].tint) / 255.0;
	return out;
}

fragment float4 textured_fragment(TexturedInOut in [[stage_in]],
                                  constant TexturedUniforms &u [[buffer(1)]],
                                  texture2d<uint> palette_texture [[texture(0)]],
                                  texture2d<float> rgb_texture [[texture(1)]],
                                  texture2d<float> palette [[texture(2)]],
                                  sampler rgb_sampler [[sampler(0)]])
{
	float4 colour;
	if (u.uses_palette == 1)
	{
		uint idx = palette_texture.read(uint2(in.texcoord)).r;
		if (idx == 0)
		{
			discard_fragment();
			return float4(0.0);
		}
		colour = palette.read(uint2(idx, 0));
	}
	else
	{
		// Normalised here rather than in the vertex shader so the Scaler choice (which picks
		// the sampler) is the only thing that varies between nearest and linear draws.
		colour = rgb_texture.sample(rgb_sampler, in.texcoord / u.tex_size);
	}
	return colour * in.tint;
}

// ---------------------------------------------------------------- untextured colour

struct ColouredVertex
{
	packed_float2 position; // screen coords
	uchar4 colour;
};

struct ColouredInOut
{
	float4 position [[position]];
	float4 colour;
};

vertex ColouredInOut coloured_vertex(uint vid [[vertex_id]],
                                     device const ColouredVertex *verts [[buffer(0)]],
                                     constant Viewport &vp [[buffer(1)]])
{
	float2 p = float2(verts[vid].position);
	p = (p / vp.size) * 2.0 - 1.0;

	ColouredInOut out;
	out.position = float4(p.x, -p.y, 0.0, 1.0);
	out.colour = float4(verts[vid].colour) / 255.0;
	return out;
}

fragment float4 coloured_fragment(ColouredInOut in [[stage_in]]) { return in.colour; }
)MSL";

// The CPU mirrors of the shader structs. MSL packed_float2 and uchar4 are both 4-aligned, so
// plain scalar arrays reproduce the layout exactly; the static_asserts are the guard that
// keeps it that way.

struct SpriteDescription
{
	int32_t uses_palette;
	int32_t page;
	float spritesheet_position[2];
	float spritesheet_size[2];
	float screen_position[2];
	float screen_size[2];
	uint8_t tint[4];
};
static_assert(sizeof(SpriteDescription) == 44, "SpriteDescription must match the MSL layout");

struct TexturedVertex
{
	float position[2];
	float texcoord[2];
	uint8_t tint[4];
};
static_assert(sizeof(TexturedVertex) == 20, "TexturedVertex must match the MSL layout");

struct TexturedUniforms
{
	float tex_size[2];
	int32_t uses_palette;
	int32_t pad;
};
static_assert(sizeof(TexturedUniforms) == 16, "TexturedUniforms must match the MSL layout");

struct ColouredVertex
{
	float position[2];
	uint8_t colour[4];
};
static_assert(sizeof(ColouredVertex) == 12, "ColouredVertex must match the MSL layout");

struct ViewportUniform
{
	float size[2];
};

void setVec2(float dst[2], Vec2<float> v)
{
	dst[0] = v.x;
	dst[1] = v.y;
}

void setColour(uint8_t dst[4], Colour c)
{
	dst[0] = c.r;
	dst[1] = c.g;
	dst[2] = c.b;
	dst[3] = c.a;
}

// Discrete GPUs cannot see a Shared texture, so those Macs need the Managed pair-of-copies
// mode. Everything else -- Apple silicon, every iOS device -- is unified.
MTLStorageMode textureStorageMode(id<MTLDevice> device)
{
#if TARGET_OS_OSX
	return device.hasUnifiedMemory ? MTLStorageModeShared : MTLStorageModeManaged;
#else
	(void)device;
	return MTLStorageModeShared;
#endif
}

// Mirrors the GL renderer's guard: image data can outlive the renderer during shutdown, and
// touching a dead renderer through the back-pointer would be worse than leaking.
std::atomic<bool> renderer_dead{true};

class MetalRenderer;

id<MTLTexture> makeTexture2D(id<MTLDevice> device, MTLPixelFormat format, Vec2<unsigned int> size)
{
	MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
	                                                                                width:size.x
	                                                                               height:size.y
	                                                                            mipmapped:NO];
	desc.storageMode = textureStorageMode(device);
	desc.usage = MTLTextureUsageShaderRead;
	return [device newTextureWithDescriptor:desc];
}

id<MTLTexture> makeTextureArray(id<MTLDevice> device, MTLPixelFormat format, Vec2<int> size,
                                unsigned int slices)
{
	MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
	desc.textureType = MTLTextureType2DArray;
	desc.pixelFormat = format;
	desc.width = size.x;
	desc.height = size.y;
	desc.arrayLength = std::max(1u, slices);
	desc.mipmapLevelCount = 1;
	desc.storageMode = textureStorageMode(device);
	desc.usage = MTLTextureUsageShaderRead;
	return [device newTextureWithDescriptor:desc];
}

id<MTLTexture> makeRenderTarget(id<MTLDevice> device, Vec2<unsigned int> size)
{
	MTLTextureDescriptor *desc =
	    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:kTargetFormat
	                                                       width:std::max(1u, size.x)
	                                                      height:std::max(1u, size.y)
	                                                   mipmapped:NO];
	// Private: the CPU never touches these directly. readBack() goes via a blit instead, which
	// works the same on every GPU and keeps the fast path free of coherency traffic.
	desc.storageMode = MTLStorageModePrivate;
	desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	return [device newTextureWithDescriptor:desc];
}

class MetalPalette final : public RendererImageData
{
  public:
	id<MTLTexture> tex;
	MetalPalette(id<MTLDevice> device, sp<Palette> parent)
	{
		this->tex = makeTexture2D(device, MTLPixelFormatRGBA8Unorm,
		                          {(unsigned int)parent->colours.size(), 1});
		[this->tex replaceRegion:MTLRegionMake2D(0, 0, parent->colours.size(), 1)
		             mipmapLevel:0
		               withBytes:parent->colours.data()
		             bytesPerRow:parent->colours.size() * 4];
	}
	~MetalPalette() override = default;
};

class MetalRGBTexture final : public RendererImageData
{
  public:
	id<MTLTexture> tex;
	MetalRGBTexture(id<MTLDevice> device, sp<RGBImage> parent)
	{
		this->tex = makeTexture2D(device, MTLPixelFormatRGBA8Unorm, parent->size);
		RGBImageLock l(parent, ImageLockUse::Read);
		[this->tex replaceRegion:MTLRegionMake2D(0, 0, parent->size.x, parent->size.y)
		             mipmapLevel:0
		               withBytes:l.getData()
		             bytesPerRow:parent->size.x * 4];
	}
	~MetalRGBTexture() override = default;
};

class MetalPaletteTexture final : public RendererImageData
{
  public:
	id<MTLTexture> tex;
	MetalPaletteTexture(id<MTLDevice> device, sp<PaletteImage> parent)
	{
		this->tex = makeTexture2D(device, MTLPixelFormatR8Uint, parent->size);
		PaletteImageLock l(parent, ImageLockUse::Read);
		[this->tex replaceRegion:MTLRegionMake2D(0, 0, parent->size.x, parent->size.y)
		             mipmapLevel:0
		               withBytes:l.getData()
		             bytesPerRow:parent->size.x];
	}
	~MetalPaletteTexture() override = default;
};

// A render target. The default surface additionally owns the window's CAMetalLayer so a
// resize can retrack the drawable size; offscreen surfaces leave it nil.
class MetalSurface final : public RendererImageData
{
  public:
	id<MTLDevice> device;
	id<MTLCommandQueue> queue;
	id<MTLTexture> tex;
	CAMetalLayer *layer;
	MetalRenderer *owner;
	Vec2<unsigned int> size;

	// A clear is a render pass load action in Metal, not a command, so it is recorded here
	// and consumed when the next pass on this surface opens.
	bool pendingClear = false;
	Colour clearColour{0, 0, 0, 0};

	// The size the framework asked for, before clamping. Kept so that a caller repeatedly
	// asking for a degenerate size cannot make the clamped size disagree with it forever,
	// which would rebuild the texture on every single call.
	Vec2<unsigned int> requestedSize;

	MetalSurface(id<MTLDevice> device, id<MTLCommandQueue> queue, MetalRenderer *owner,
	             Vec2<unsigned int> size, CAMetalLayer *layer = nil)
	    : device(device), queue(queue), layer(layer), owner(owner), size(size),
	      requestedSize(size)
	{
		this->adopt(size);
	}
	~MetalSurface() override = default;

	// A new MTLTexture's contents are undefined, where a GL framebuffer texture created with a
	// null data pointer comes back zeroed on every driver the engine has ever run on -- and the
	// engine leans on that. forms/control.cpp renders a control into its cached surface only
	// when dirty, and onRender() need not touch every pixel; whatever it skips has to read as
	// transparent. Left undefined, those pixels show as garbage.
	//
	// The clear is recorded rather than performed: it becomes the load action of whichever pass
	// first touches this surface, which on a tile-based GPU is free. Committing a command
	// buffer here instead would cost a submission per surface, and the widget tree creates one
	// surface per control.
	void adopt(Vec2<unsigned int> clamped)
	{
		this->size = clamped;
		this->tex = makeRenderTarget(this->device, clamped);
		this->pendingClear = true;
		this->clearColour = {0, 0, 0, 0};
	}

	void resize(Vec2<unsigned int> newSize) override
	{
		this->requestedSize = newSize;
		const Vec2<unsigned int> clamped{std::max(1u, newSize.x), std::max(1u, newSize.y)};
		if (clamped == this->size && this->tex)
		{
			return;
		}
		this->adopt(clamped);
		if (this->layer)
		{
			this->layer.drawableSize = CGSizeMake(clamped.x, clamped.y);
		}
	}

	sp<Image> readBack() override;
};

// ---------------------------------------------------------------------------------------
// Spritesheet paging. Lifted from the GL renderer so packing behaviour stays bit-identical;
// only the upload calls differ.
// ---------------------------------------------------------------------------------------

class SpritesheetEntry final : public RendererImageData
{
  public:
	SpritesheetEntry(Vec2<int> size, sp<Image> parent)
	    : parent(parent), position({-1, -1}), size(size), page(-1)
	{
	}
	wp<Image> parent;
	Vec2<int> position;
	Vec2<int> size;
	// page == -1 means it's not yet packed into a spritesheet
	int page;
	~SpritesheetEntry() override = default;
};

class SpritesheetPage
{
  private:
	up<stbrp_node[]> pack_nodes;
	stbrp_context pack_context;
	int page_no;

  public:
	SpritesheetPage(int page_no, Vec2<int> size, int node_count) : page_no(page_no)
	{
		LogAssert(node_count > 0);
		LogAssert(size.x > 0);
		LogAssert(size.y > 0);
		pack_nodes.reset(new stbrp_node[node_count]);
		stbrp_init_target(&pack_context, size.x, size.y, pack_nodes.get(), node_count);
		stbrp_setup_heuristic(&pack_context, STBRP__INIT_skyline);
	}

	bool addEntry(sp<SpritesheetEntry> entry)
	{
		LogAssert(entry->page == -1);
		stbrp_rect r;
		r.w = entry->size.x;
		r.h = entry->size.y;
		stbrp_pack_rects(&pack_context, &r, 1);
		if (r.was_packed)
		{
			entry->position = {r.x, r.y};
			entry->page = this->page_no;
			this->entries.push_back(entry);
			return true;
		}
		return false;
	}

	std::list<wp<SpritesheetEntry>> entries;
};

class Spritesheet
{
	// Max number of rects stb_rect_pack will track per page
	const int node_count = 4096;

  public:
	std::vector<sp<SpritesheetPage>> pages;
	Vec2<int> page_size;
	MTLPixelFormat format;
	id<MTLDevice> device;
	id<MTLTexture> tex;

	Spritesheet(id<MTLDevice> device, Vec2<int> page_size, MTLPixelFormat format)
	    : page_size(page_size), format(format), device(device)
	{
		// A 1x1 placeholder, not a page: the fragment shader always has both sheets bound and
		// Metal will not accept a nil texture, but a real page is 4096x4096 -- 67MB for the
		// RGBA sheet and 17MB for the paletted one. Allocating those before a single sprite of
		// that kind exists wastes 84MB, which on an iPad is the difference between fitting in
		// memory and being paged. The GL renderer allocated zero array slices here for the same
		// reason; addSprite() builds the first real page.
		this->tex = makeTextureArray(device, format, {1, 1}, 1);
	}
	~Spritesheet() = default;

	void reuploadTextures()
	{
		this->tex = makeTextureArray(this->device, this->format, this->page_size,
		                             (unsigned int)this->pages.size());
		for (auto &page : this->pages)
		{
			for (auto &entry : page->entries)
			{
				auto entryPtr = entry.lock();
				// Images dropped since packing leave a hole. stb_rect_pack cannot release a
				// node, so that area is never reclaimed -- pages only ever grow. The GL
				// renderer carries a repack() to rebuild every page from the surviving
				// entries, but nothing has ever called it there either, so it is not
				// reproduced here. If a scene is ever found that exhausts a page through
				// churn rather than through genuine sprite count, this is what it needs.
				if (!entryPtr)
					continue;
				this->upload(entryPtr);
			}
		}
	}

	void upload(sp<SpritesheetEntry> entry)
	{
		LogAssert(entry->page >= 0);
		LogAssert(entry->page < (int)this->pages.size());
		auto image = entry->parent.lock();
		if (!image)
		{
			LogError("Spritesheet entry has no parent");
			return;
		}
		// A zero-area sprite -- a space glyph is 0 wide and a full line tall -- has nothing to
		// upload, and replaceRegion with an empty region is not merely useless: Apple's driver
		// rejects it with "AGX: Texture read/write assertion failed: width > 0" on every call.
		// GL's TexSubImage3D silently no-ops on the same input, which is why this is a
		// Metal-only concern and why the GL renderer needs no such guard.
		if (entry->size.x <= 0 || entry->size.y <= 0)
		{
			return;
		}
		auto region =
		    MTLRegionMake2D(entry->position.x, entry->position.y, entry->size.x, entry->size.y);
		if (image->imageType == ImageType::RGB)
		{
			LogAssert(format == MTLPixelFormatRGBA8Unorm);
			RGBImageLock l(std::static_pointer_cast<RGBImage>(image), ImageLockUse::Read);
			[this->tex replaceRegion:region
			             mipmapLevel:0
			                   slice:entry->page
			               withBytes:l.getData()
			             bytesPerRow:entry->size.x * 4
			           bytesPerImage:0];
			return;
		}
		if (image->imageType == ImageType::Palette)
		{
			LogAssert(format == MTLPixelFormatR8Uint);
			PaletteImageLock l(std::static_pointer_cast<PaletteImage>(image), ImageLockUse::Read);
			[this->tex replaceRegion:region
			             mipmapLevel:0
			                   slice:entry->page
			               withBytes:l.getData()
			             bytesPerRow:entry->size.x
			           bytesPerImage:0];
			return;
		}
		LogError("Unknown image type in spritesheet upload");
	}


	void addSprite(sp<SpritesheetEntry> entry)
	{
		LogAssert(entry->page == -1);
		LogAssert(entry->size.x < page_size.x);
		LogAssert(entry->size.y < page_size.y);
		for (auto &page : this->pages)
		{
			if (page->addEntry(entry))
			{
				this->upload(entry);
				return;
			}
		}
		LogInfo("Creating spritesheet page {0}", (int)pages.size());
		auto page = mksp<SpritesheetPage>((int)pages.size(), page_size, node_count);
		auto ret = page->addEntry(entry);
		if (!ret)
		{
			LogError("Failed to pack a {0} sized sprite in a new page of size {1}?", entry->size,
			         page_size);
		}
		this->pages.push_back(page);
		// The array texture's length is fixed at creation, so growing the page count means
		// building a new texture and re-uploading everything.
		this->reuploadTextures();
	}
};

// ---------------------------------------------------------------------------------------
// Dynamic buffers
// ---------------------------------------------------------------------------------------

// Sprite batches are too big for setVertexBytes (4KB), so they need real buffers, and a
// buffer must not be rewritten while the GPU is still reading it. Rather than the GL
// renderer's fixed ring -- which only works if you guess the depth right -- buffers are
// recycled by the command buffer's completion handler, so reuse is tied to actual GPU
// progress. Every buffer is the same size, which makes the pool a free list.
class BufferPool
{
  private:
	id<MTLDevice> device;
	NSUInteger bufferBytes;
	NSMutableArray<id<MTLBuffer>> *available;
	std::mutex mutex;

  public:
	BufferPool(id<MTLDevice> device, NSUInteger bufferBytes)
	    : device(device), bufferBytes(bufferBytes), available([[NSMutableArray alloc] init])
	{
	}
	unsigned int highWater = 0;
	unsigned int liveCount = 0;

	id<MTLBuffer> acquire()
	{
		std::lock_guard<std::mutex> lock(this->mutex);
		if (this->available.count > 0)
		{
			id<MTLBuffer> buf = this->available.lastObject;
			[this->available removeLastObject];
			return buf;
		}
		this->liveCount++;
		this->highWater = std::max(this->highWater, this->liveCount);
		return [this->device newBufferWithLength:this->bufferBytes
		                                 options:MTLResourceStorageModeShared];
	}

	// Called from the command buffer completion handler, which runs on a Metal-owned thread.
	void recycle(NSArray<id<MTLBuffer>> *used)
	{
		std::lock_guard<std::mutex> lock(this->mutex);
		[this->available addObjectsFromArray:used];
	}
};

// Command buffers report the wall-clock window in which the GPU actually executed them.
// That is the only way to separate real GPU cost from time spent blocked in nextDrawable
// waiting on the display, which otherwise both show up as "swap" in the frame profile.
// Shared, because the completion handler can outlive the renderer.
struct GpuTimer
{
	std::mutex mutex;
	double totalMs = 0.0;
	uint64_t samples = 0;

	void add(double ms)
	{
		std::lock_guard<std::mutex> lock(this->mutex);
		this->totalMs += ms;
		this->samples++;
	}
	// Returns the mean since the last call and resets.
	double takeMeanMs()
	{
		std::lock_guard<std::mutex> lock(this->mutex);
		const double mean = this->samples ? this->totalMs / (double)this->samples : 0.0;
		this->totalMs = 0.0;
		this->samples = 0;
		return mean;
	}
};

// ---------------------------------------------------------------------------------------
// Draw machines
// ---------------------------------------------------------------------------------------

id<MTLRenderPipelineState> makePipeline(id<MTLDevice> device, id<MTLLibrary> library,
                                        const char *vertexName, const char *fragmentName,
                                        const char *label)
{
	MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
	desc.label = @(label);
	desc.vertexFunction = [library newFunctionWithName:@(vertexName)];
	desc.fragmentFunction = [library newFunctionWithName:@(fragmentName)];
	if (!desc.vertexFunction || !desc.fragmentFunction)
	{
		LogError("Metal shader function missing: {0} / {1}", vertexName, fragmentName);
		return nil;
	}

	MTLRenderPipelineColorAttachmentDescriptor *att = desc.colorAttachments[0];
	att.pixelFormat = kTargetFormat;
	att.blendingEnabled = YES;
	att.rgbBlendOperation = MTLBlendOperationAdd;
	att.alphaBlendOperation = MTLBlendOperationAdd;
	// Deliberately asymmetric, matching the GL renderer's
	// BlendFuncSeparate(SRC_ALPHA, ONE_MINUS_SRC_ALPHA, SRC_ALPHA, DST_ALPHA). The alpha
	// channel's destination factor really is DST_ALPHA and not its complement; changing it
	// would alter how surfaces composite.
	att.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
	att.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
	att.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
	att.destinationAlphaBlendFactor = MTLBlendFactorDestinationAlpha;

	NSError *error = nil;
	id<MTLRenderPipelineState> state = [device newRenderPipelineStateWithDescriptor:desc
	                                                                         error:&error];
	if (!state)
	{
		LogError("Failed to create Metal pipeline \"{0}\": {1}", label,
		         error ? error.localizedDescription.UTF8String : "unknown error");
	}
	return state;
}

class SpriteMachine
{
  private:
	std::vector<SpriteDescription> staging;
	unsigned int contents = 0;

  public:
	id<MTLRenderPipelineState> pipeline;
	Spritesheet palette_spritesheet;
	Spritesheet rgb_spritesheet;

	SpriteMachine(id<MTLDevice> device, id<MTLLibrary> library, unsigned int bufferSize,
	              Vec2<int> pageSize)
	    : staging(bufferSize), palette_spritesheet(device, pageSize, MTLPixelFormatR8Uint),
	      rgb_spritesheet(device, pageSize, MTLPixelFormatRGBA8Unorm)
	{
		LogAssert(bufferSize > 0);
		this->pipeline = makePipeline(device, library, "sprite_vertex", "sprite_fragment",
		                              "OpenApoc sprites");
	}

	bool isEmpty() const { return this->contents == 0; }
	bool isFull() const { return this->contents >= this->staging.size(); }
	unsigned int count() const { return this->contents; }

	sp<SpritesheetEntry> entryFor(const sp<PaletteImage> &i)
	{
		auto entry = std::dynamic_pointer_cast<SpritesheetEntry>(i->rendererPrivateData);
		if (!entry)
		{
			entry = mksp<SpritesheetEntry>(i->size, i);
			this->palette_spritesheet.addSprite(entry);
			i->rendererPrivateData = entry;
		}
		return entry;
	}

	sp<SpritesheetEntry> entryFor(const sp<RGBImage> &i)
	{
		auto entry = std::dynamic_pointer_cast<SpritesheetEntry>(i->rendererPrivateData);
		if (!entry)
		{
			entry = mksp<SpritesheetEntry>(i->size, i);
			this->rgb_spritesheet.addSprite(entry);
			i->rendererPrivateData = entry;
		}
		return entry;
	}

	void push(const sp<SpritesheetEntry> &e, bool paletted, Vec2<float> screenPos,
	          Vec2<float> screenSize, Colour tint)
	{
		LogAssert(!this->isFull());
		LogAssert(e->page != -1);

		auto &d = this->staging[this->contents];
		this->contents++;

		d.uses_palette = paletted ? 1 : 0;
		d.page = e->page;
		setVec2(d.spritesheet_position, {(float)e->position.x, (float)e->position.y});
		setVec2(d.spritesheet_size, {(float)e->size.x, (float)e->size.y});
		setVec2(d.screen_position, screenPos);
		setVec2(d.screen_size, screenSize);
		setColour(d.tint, tint);
	}

	void encode(id<MTLRenderCommandEncoder> enc, id<MTLBuffer> instances,
	            Vec2<unsigned int> viewport, id<MTLTexture> palette)
	{
		if (this->contents == 0)
		{
			return;
		}
		std::memcpy(instances.contents, this->staging.data(),
		            this->contents * sizeof(SpriteDescription));

		ViewportUniform vp;
		setVec2(vp.size, {(float)viewport.x, (float)viewport.y});

		[enc setRenderPipelineState:this->pipeline];
		[enc setVertexBuffer:instances offset:0 atIndex:0];
		[enc setVertexBytes:&vp length:sizeof(vp) atIndex:1];
		[enc setFragmentTexture:this->palette_spritesheet.tex atIndex:0];
		[enc setFragmentTexture:this->rgb_spritesheet.tex atIndex:1];
		[enc setFragmentTexture:palette atIndex:2];
		[enc drawPrimitives:MTLPrimitiveTypeTriangleStrip
		        vertexStart:0
		        vertexCount:4
		      instanceCount:this->contents];

		this->contents = 0;
	}
};

class TexturedMachine
{
  public:
	id<MTLRenderPipelineState> pipeline;

	TexturedMachine(id<MTLDevice> device, id<MTLLibrary> library)
	{
		this->pipeline = makePipeline(device, library, "textured_vertex", "textured_fragment",
		                              "OpenApoc textured quads");
	}

	// screenPos/screenSize are in surface coords; the quad is rotated about rotationCenter
	// before translation, exactly as the GL renderer does it.
	void encode(id<MTLRenderCommandEncoder> enc, bool paletted, id<MTLTexture> texture,
	            id<MTLTexture> palette, id<MTLTexture> dummyPaletted, id<MTLTexture> dummyRGB,
	            id<MTLSamplerState> sampler, Vec2<float> texSize, Vec2<float> screenPos,
	            Vec2<float> screenSize, Vec2<float> rotationCenter, float rotationAngleRadians,
	            Vec2<unsigned int> viewport, Colour tint)
	{
		static const Vec2<float> identity_quad[4] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
		TexturedVertex verts[4];

		auto rotMatrix = glm::rotate(rotationAngleRadians, Vec3<float>{0.0f, 0.0f, 1.0f});
		for (unsigned int i = 0; i < 4; i++)
		{
			auto p = identity_quad[i];
			p *= screenSize;
			p -= rotationCenter;
			glm::vec4 transformed = rotMatrix * glm::vec4{p.x, p.y, 0.0f, 1.0f};
			p.x = transformed.x;
			p.y = transformed.y;
			p += rotationCenter;
			p += screenPos;
			// 0..viewport coords to clip space; the shader applies the Y flip.
			p /= Vec2<float>{(float)viewport.x, (float)viewport.y};
			p -= Vec2<float>{0.5f, 0.5f};
			p *= Vec2<float>{2.0f, 2.0f};

			setVec2(verts[i].position, p);
			setVec2(verts[i].texcoord, identity_quad[i]);
			setColour(verts[i].tint, tint);
		}

		TexturedUniforms u;
		setVec2(u.tex_size, texSize);
		u.uses_palette = paletted ? 1 : 0;
		u.pad = 0;

		[enc setRenderPipelineState:this->pipeline];
		[enc setVertexBytes:verts length:sizeof(verts) atIndex:0];
		[enc setVertexBytes:&u length:sizeof(u) atIndex:1];
		[enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
		// Both texture slots are always bound: the shader picks between them on a uniform, and
		// leaving the unused one nil trips Metal's validation layer.
		[enc setFragmentTexture:(paletted ? texture : dummyPaletted) atIndex:0];
		[enc setFragmentTexture:(paletted ? dummyRGB : texture) atIndex:1];
		[enc setFragmentTexture:palette atIndex:2];
		[enc setFragmentSamplerState:sampler atIndex:0];
		[enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	}
};

class ColouredMachine
{
  public:
	id<MTLRenderPipelineState> pipeline;

	ColouredMachine(id<MTLDevice> device, id<MTLLibrary> library)
	{
		this->pipeline = makePipeline(device, library, "coloured_vertex", "coloured_fragment",
		                              "OpenApoc coloured primitives");
	}

	void drawQuad(id<MTLRenderCommandEncoder> enc, const Vec2<float> positions[4],
	              const Colour colours[4], Vec2<unsigned int> viewport)
	{
		ColouredVertex verts[4];
		for (int i = 0; i < 4; i++)
		{
			setVec2(verts[i].position, positions[i]);
			setColour(verts[i].colour, colours[i]);
		}
		ViewportUniform vp;
		setVec2(vp.size, {(float)viewport.x, (float)viewport.y});

		[enc setRenderPipelineState:this->pipeline];
		[enc setVertexBytes:verts length:sizeof(verts) atIndex:0];
		[enc setVertexBytes:&vp length:sizeof(vp) atIndex:1];
		[enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	}

	// Metal has no line width -- MTLPrimitiveTypeLine is always one pixel. A hairline is drawn
	// as a real line so rasterisation matches GL exactly; anything wider becomes a quad, which
	// is what the thickness argument asks for and what GL drivers mostly refuse to give.
	void drawLine(id<MTLRenderCommandEncoder> enc, Vec2<float> p1, Vec2<float> p2, Colour c,
	              float thickness, Vec2<unsigned int> viewport)
	{
		ViewportUniform vp;
		setVec2(vp.size, {(float)viewport.x, (float)viewport.y});
		[enc setRenderPipelineState:this->pipeline];
		[enc setVertexBytes:&vp length:sizeof(vp) atIndex:1];

		if (thickness <= 1.0f)
		{
			ColouredVertex verts[2];
			setVec2(verts[0].position, p1);
			setVec2(verts[1].position, p2);
			setColour(verts[0].colour, c);
			setColour(verts[1].colour, c);
			[enc setVertexBytes:verts length:sizeof(verts) atIndex:0];
			[enc drawPrimitives:MTLPrimitiveTypeLine vertexStart:0 vertexCount:2];
			return;
		}

		auto dir = p2 - p1;
		float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
		if (len <= 0.0f)
		{
			return;
		}
		Vec2<float> normal{-dir.y / len, dir.x / len};
		Vec2<float> offset = normal * (thickness * 0.5f);

		ColouredVertex verts[4];
		const Vec2<float> corners[4] = {p1 - offset, p1 + offset, p2 - offset, p2 + offset};
		for (int i = 0; i < 4; i++)
		{
			setVec2(verts[i].position, corners[i]);
			setColour(verts[i].colour, c);
		}
		[enc setVertexBytes:verts length:sizeof(verts) atIndex:0];
		[enc drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	}
};

// ---------------------------------------------------------------------------------------
// The renderer
// ---------------------------------------------------------------------------------------

class MetalRenderer final : public Renderer
{
  private:
	id<MTLDevice> device;
	id<MTLCommandQueue> queue;
	CAMetalLayer *layer;

	id<MTLCommandBuffer> commandBuffer;
	id<MTLRenderCommandEncoder> encoder;
	// Non-null exactly while a command buffer is open; see the note above the declarations.
	void *framePool = nullptr;
	NSMutableArray<id<MTLBuffer>> *inFlightBuffers;

	id<MTLSamplerState> nearestSampler;
	id<MTLSamplerState> linearSampler;
	// Stand-ins for the texture slot the shader is not reading this draw. Metal's validation
	// layer objects to a nil binding, so something valid is always bound.
	id<MTLTexture> dummyPaletted;
	id<MTLTexture> dummyRGB;

	up<SpriteMachine> spriteMachine;
	up<TexturedMachine> texturedMachine;
	up<ColouredMachine> colouredMachine;
	std::shared_ptr<BufferPool> spriteBuffers;
	std::shared_ptr<GpuTimer> gpuTimer = std::make_shared<GpuTimer>();

	Vec2<int> spritesheetPageSize = {4096, 4096};
	Vec2<unsigned int> maxSpriteSizeToPack{256, 256};
	unsigned int spriteBufferSize = 16384;

	enum State
	{
		Idle,
		BatchingSprites,
	};
	State state = State::Idle;

	sp<Surface> default_surface;
	sp<Surface> current_surface;
	sp<Palette> current_palette;

	uint64_t drawCalls = 0;
	uint64_t spritesDrawn = 0;
	// Render passes are the metric that matters on a tile-based GPU: a pass that loads costs a
	// full-target read and a pass that stores costs a full-target write, however few triangles
	// it draws. Counting them separates "too much geometry" from "too much pass churn".
	uint64_t renderPasses = 0;
	uint64_t passLoads = 0;
	uint64_t frameCount = 0;
	uint64_t profilePasses = 0;
	uint64_t profileLoads = 0;

	MetalSurface *surfaceData(const sp<Surface> &s)
	{
		auto data = std::dynamic_pointer_cast<MetalSurface>(s->rendererPrivateData);
		if (!data)
		{
			data = mksp<MetalSurface>(this->device, this->queue, this, s->size);
			s->rendererPrivateData = data;
		}
		// The framework resizes a Surface by writing size directly and then calling resize()
		// on the private data, but a surface created before its first use can still arrive
		// here stale. Compare against the last size that was *asked* for rather than the
		// clamped one it settled on: a surface with a zero dimension clamps to 1 and would
		// otherwise never compare equal, rebuilding its texture on every call.
		if (data->requestedSize != s->size)
		{
			data->resize(s->size);
		}
		return data.get();
	}

	id<MTLTexture> paletteTexture()
	{
		if (!this->current_palette)
		{
			return this->dummyRGB;
		}
		auto pal = std::dynamic_pointer_cast<MetalPalette>(this->current_palette->rendererPrivateData);
		if (!pal)
		{
			pal = mksp<MetalPalette>(this->device, this->current_palette);
			this->current_palette->rendererPrivateData = pal;
		}
		return pal->tex;
	}

	id<MTLCommandBuffer> ensureCommandBuffer()
	{
		if (!this->commandBuffer)
		{
			this->framePool = objc_autoreleasePoolPush();
			this->commandBuffer = [this->queue commandBuffer];
			this->commandBuffer.label = @"OpenApoc frame";
			this->inFlightBuffers = [[NSMutableArray alloc] init];
		}
		return this->commandBuffer;
	}

	MTLRenderPassDescriptor *passFor(MetalSurface *target)
	{
		MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture = target->tex;
		if (target->pendingClear)
		{
			pass.colorAttachments[0].loadAction = MTLLoadActionClear;
			pass.colorAttachments[0].clearColor = MTLClearColorMake(
			    target->clearColour.r / 255.0, target->clearColour.g / 255.0,
			    target->clearColour.b / 255.0, target->clearColour.a / 255.0);
			target->pendingClear = false;
		}
		else
		{
			pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
			this->passLoads++;
		}
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		this->renderPasses++;
		return pass;
	}

	id<MTLRenderCommandEncoder> ensureEncoder()
	{
		if (this->encoder)
		{
			return this->encoder;
		}
		auto *target = this->surfaceData(this->current_surface);
		// Sequenced deliberately: ensureCommandBuffer() pushes this frame's autorelease pool,
		// and passFor() returns an autoreleased descriptor. Written as one message send the
		// evaluation order of receiver and argument is unspecified, so the descriptor could be
		// created before the pool that should own it.
		id<MTLCommandBuffer> commands = this->ensureCommandBuffer();
		MTLRenderPassDescriptor *pass = this->passFor(target);
		this->encoder = [commands renderCommandEncoderWithDescriptor:pass];
		this->encoder.label = @"OpenApoc pass";
		[this->encoder setViewport:(MTLViewport){0.0, 0.0, (double)target->size.x,
		                                         (double)target->size.y, 0.0, 1.0}];
		return this->encoder;
	}

	void endEncoder()
	{
		if (this->encoder)
		{
			[this->encoder endEncoding];
			this->encoder = nil;
		}
	}

	// A clear with no draw after it still has to happen. Opening and immediately closing a
	// pass is how you spell "just the load action" in Metal.
	void applyPendingClear(MetalSurface *target)
	{
		if (!target->pendingClear)
		{
			return;
		}
		this->endEncoder();
		// Same sequencing point as ensureEncoder(); see the note there.
		id<MTLCommandBuffer> commands = this->ensureCommandBuffer();
		MTLRenderPassDescriptor *pass = this->passFor(target);
		id<MTLRenderCommandEncoder> enc = [commands renderCommandEncoderWithDescriptor:pass];
		enc.label = @"OpenApoc clear";
		[enc endEncoding];
	}

	void setSurface(sp<Surface> s) override
	{
		// Drain the batch into the old surface before the target changes, as GL does.
		this->flush();
		this->endEncoder();
		this->current_surface = s;
		this->surfaceData(s);
	}
	sp<Surface> getSurface() override { return this->current_surface; }

	// Resolve the concrete image behind a possibly-lazy handle.
	static const sp<Image> &resolve(const sp<Image> &i)
	{
		if (i->imageType == ImageType::Lazy)
		{
			return std::static_pointer_cast<LazyImage>(i)->getRealImage();
		}
		return i;
	}

	bool packable(const sp<Image> &i) const
	{
		return i->size.x <= this->maxSpriteSizeToPack.x && i->size.y <= this->maxSpriteSizeToPack.y;
	}

	void beginSpriteBatch()
	{
		if (this->state != State::BatchingSprites)
		{
			this->flush();
			this->state = State::BatchingSprites;
		}
		if (this->spriteMachine->isFull())
		{
			this->flushSprites();
			this->state = State::BatchingSprites;
		}
	}

	void flushSprites()
	{
		if (this->spriteMachine->isEmpty())
		{
			this->state = State::Idle;
			return;
		}
		auto enc = this->ensureEncoder();
		id<MTLBuffer> instances = this->spriteBuffers->acquire();
		[this->inFlightBuffers addObject:instances];
		this->spritesDrawn += this->spriteMachine->count();
		this->spriteMachine->encode(enc, instances, this->current_surface->size,
		                            this->paletteTexture());
		this->drawCalls++;
		this->state = State::Idle;
	}

	// The single-quad path, shared by every draw too large or too rotated to batch.
	void drawTexturedQuad(const sp<Image> &image, Vec2<float> position, Vec2<float> size,
	                      Vec2<float> rotationCenter, float angle, Scaler scaler, Colour tint)
	{
		this->flush();
		auto sampler = (scaler == Scaler::Linear) ? this->linearSampler : this->nearestSampler;

		// Resolve the source texture BEFORE opening the encoder. Uploading a texture is
		// harmless mid-pass, but a Surface source may still owe a clear, and honouring that
		// ends whatever encoder is open -- which would leave an encoder captured here dangling.
		id<MTLTexture> texture = nil;
		bool paletted = false;
		switch (image->imageType)
		{
			case ImageType::Palette:
			{
				auto img = std::static_pointer_cast<PaletteImage>(image);
				auto tex = std::dynamic_pointer_cast<MetalPaletteTexture>(img->rendererPrivateData);
				if (!tex)
				{
					tex = mksp<MetalPaletteTexture>(this->device, img);
					img->rendererPrivateData = tex;
				}
				texture = tex->tex;
				paletted = true;
				break;
			}
			case ImageType::RGB:
			{
				auto img = std::static_pointer_cast<RGBImage>(image);
				auto tex = std::dynamic_pointer_cast<MetalRGBTexture>(img->rendererPrivateData);
				if (!tex)
				{
					tex = mksp<MetalRGBTexture>(this->device, img);
					img->rendererPrivateData = tex;
				}
				texture = tex->tex;
				break;
			}
			case ImageType::Surface:
			{
				auto surf = std::static_pointer_cast<Surface>(image);
				if (!surf->rendererPrivateData)
				{
					LogWarning("Drawing using undefined surface contents");
				}
				auto *data = this->surfaceData(surf);
				// Reading a surface nothing has drawn into yet: its recorded clear has no pass
				// to ride along on, so give it one before its contents are sampled.
				this->applyPendingClear(data);
				texture = data->tex;
				break;
			}
			default:
				LogError("Unknown image type {0}", (int)image->imageType);
				return;
		}
		if (!texture)
		{
			return;
		}

		auto enc = this->ensureEncoder();
		this->texturedMachine->encode(enc, paletted, texture, this->paletteTexture(),
		                              this->dummyPaletted, this->dummyRGB, sampler,
		                              {(float)image->size.x, (float)image->size.y}, position, size,
		                              rotationCenter, angle, this->current_surface->size, tint);
		this->drawCalls++;
	}

  public:
	MetalRenderer(id<MTLDevice> device, CAMetalLayer *layer, id<MTLLibrary> library)
	    : device(device), layer(layer)
	{
		this->queue = [device newCommandQueue];
		this->queue.label = @"OpenApoc";

		unsigned int maxTextureSize = 8192;
		if (@available(macOS 10.15, iOS 13.0, *))
		{
			maxTextureSize = [device supportsFamily:MTLGPUFamilyApple3] ? 16384 : 8192;
		}
		if ((unsigned int)this->spritesheetPageSize.x > maxTextureSize ||
		    (unsigned int)this->spritesheetPageSize.y > maxTextureSize)
		{
			LogWarning("Default spritesheet size {0} larger than HW limit {1} - clamping...",
			           this->spritesheetPageSize, maxTextureSize);
			this->spritesheetPageSize.x = std::min(this->spritesheetPageSize.x, (int)maxTextureSize);
			this->spritesheetPageSize.y = std::min(this->spritesheetPageSize.y, (int)maxTextureSize);
		}
		LogInfo("Set spritesheet size to {0}", this->spritesheetPageSize);

		this->spriteMachine.reset(
		    new SpriteMachine(device, library, this->spriteBufferSize, this->spritesheetPageSize));
		this->texturedMachine.reset(new TexturedMachine(device, library));
		this->colouredMachine.reset(new ColouredMachine(device, library));
		this->spriteBuffers = std::make_shared<BufferPool>(
		    device, this->spriteBufferSize * sizeof(SpriteDescription));

		MTLSamplerDescriptor *samplerDesc = [[MTLSamplerDescriptor alloc] init];
		samplerDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
		samplerDesc.minFilter = MTLSamplerMinMagFilterNearest;
		samplerDesc.magFilter = MTLSamplerMinMagFilterNearest;
		this->nearestSampler = [device newSamplerStateWithDescriptor:samplerDesc];
		samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
		samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
		this->linearSampler = [device newSamplerStateWithDescriptor:samplerDesc];

		this->dummyPaletted = makeTexture2D(device, MTLPixelFormatR8Uint, {1, 1});
		this->dummyRGB = makeTexture2D(device, MTLPixelFormatRGBA8Unorm, {1, 1});
		const uint8_t zeroPixel[4] = {0, 0, 0, 0};
		[this->dummyPaletted replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
		                      mipmapLevel:0
		                        withBytes:zeroPixel
		                      bytesPerRow:1];
		[this->dummyRGB replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
		                 mipmapLevel:0
		                   withBytes:zeroPixel
		                 bytesPerRow:4];

		// drawableSize can still be zero here: on iOS the layer is sized by the first
		// layout pass, which has not necessarily run by the time SDL hands the view back.
		// Falling back to the layer's own bounds keeps the first frame from being rendered
		// into a 1x1 target; displayRefreshSize() corrects it either way a moment later.
		CGSize drawable = layer.drawableSize;
		if (drawable.width < 1.0 || drawable.height < 1.0)
		{
			const CGFloat scale = layer.contentsScale > 0.0 ? layer.contentsScale : 1.0;
			drawable = CGSizeMake(layer.bounds.size.width * scale,
			                      layer.bounds.size.height * scale);
			LogWarning("CAMetalLayer had no drawable size; deriving {0}x{1} from its bounds",
			           (double)drawable.width, (double)drawable.height);
		}
		Vec2<unsigned int> size{(unsigned int)std::max(1.0, drawable.width),
		                        (unsigned int)std::max(1.0, drawable.height)};
		LogInfo("Metal drawable size {0}", size);

		this->default_surface = mksp<Surface>(size);
		this->default_surface->rendererPrivateData =
		    mksp<MetalSurface>(device, this->queue, this, size, layer);
		this->current_surface = this->default_surface;

		renderer_dead = false;
	}

	~MetalRenderer() override
	{
		this->endEncoder();
		if (this->commandBuffer)
		{
			[this->commandBuffer commit];
			[this->commandBuffer waitUntilCompleted];
			this->commandBuffer = nil;
		}
		LogInfo("Metal renderer used at most {0} sprite buffers", this->spriteBuffers->highWater);
		renderer_dead = true;
	}

	void clear(Colour c) override
	{
		this->flush();
		this->endEncoder();
		auto *target = this->surfaceData(this->current_surface);
		target->pendingClear = true;
		target->clearColour = c;
	}

	void setPalette(sp<Palette> p) override
	{
		this->flush();
		this->current_palette = p;
	}
	sp<Palette> getPalette() override { return this->current_palette; }

	void draw(const sp<Image> &i, Vec2<float> position) override
	{
		this->drawScaled(i, position, i->size, Scaler::Nearest);
	}

	void drawRotated(const sp<Image> &i, Vec2<float> center, Vec2<float> position,
	                 float angle) override
	{
		const auto &image = resolve(i);
		// Rotation always takes the single-quad path: the batched sprite shader only knows
		// how to place an axis-aligned rect.
		auto scaler =
		    image->imageType == ImageType::Palette ? Scaler::Nearest : Scaler::Linear;
		this->drawTexturedQuad(image, position, image->size, center, angle, scaler,
		                       {255, 255, 255, 255});
	}

	void drawScaled(const sp<Image> &i, Vec2<float> position, Vec2<float> size,
	                Scaler scaler) override
	{
		const auto &image = resolve(i);
		switch (image->imageType)
		{
			case ImageType::Palette:
				if (this->packable(image))
				{
					this->beginSpriteBatch();
					this->spriteMachine->push(
					    this->spriteMachine->entryFor(std::static_pointer_cast<PaletteImage>(image)),
					    true, position, size, {255, 255, 255, 255});
					return;
				}
				break;
			case ImageType::RGB:
				// A scaled-up RGB sprite needs filtering, which the batched path cannot do.
				if (this->packable(image) && scaler == Scaler::Nearest)
				{
					this->beginSpriteBatch();
					this->spriteMachine->push(
					    this->spriteMachine->entryFor(std::static_pointer_cast<RGBImage>(image)),
					    false, position, size, {255, 255, 255, 255});
					return;
				}
				break;
			case ImageType::Surface:
				break;
			default:
				LogError("Unknown image type {0}", (int)image->imageType);
				return;
		}
		this->drawTexturedQuad(image, position, size, {0, 0}, 0.0f, scaler, {255, 255, 255, 255});
	}

	void drawTinted(const sp<Image> &i, Vec2<float> position, Colour tint) override
	{
		const auto &image = resolve(i);
		switch (image->imageType)
		{
			case ImageType::Palette:
				if (this->packable(image))
				{
					this->beginSpriteBatch();
					this->spriteMachine->push(
					    this->spriteMachine->entryFor(std::static_pointer_cast<PaletteImage>(image)),
					    true, position, image->size, tint);
					return;
				}
				break;
			case ImageType::RGB:
				if (this->packable(image))
				{
					this->beginSpriteBatch();
					this->spriteMachine->push(
					    this->spriteMachine->entryFor(std::static_pointer_cast<RGBImage>(image)),
					    false, position, image->size, tint);
					return;
				}
				break;
			case ImageType::Surface:
				break;
			default:
				LogError("Unknown image type {0}", (int)image->imageType);
				return;
		}
		this->drawTexturedQuad(image, position, image->size, {0, 0}, 0.0f, Scaler::Nearest, tint);
	}

	void drawFilledRect(Vec2<float> position, Vec2<float> size, Colour c) override
	{
		this->flush();
		static const Vec2<float> identity_quad[4] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
		Vec2<float> positions[4];
		Colour colours[4];
		for (int i = 0; i < 4; i++)
		{
			positions[i] = position + size * identity_quad[i];
			colours[i] = c;
		}
		this->colouredMachine->drawQuad(this->ensureEncoder(), positions, colours,
		                                this->current_surface->size);
		this->drawCalls++;
	}

	// Four filled rects rather than four lines, with the corner caps shared out so they never
	// overlap -- overlap would double-blend at any alpha below 1. Geometry is the GL
	// renderer's, unchanged.
	void drawRect(Vec2<float> position, Vec2<float> size, Colour c, float thickness) override
	{
		Vec2<float> p0 = position;
		Vec2<float> p1 = position + size;

		Vec2<float> A = {p0};
		Vec2<float> sizeA = {size.x - thickness, thickness};

		Vec2<float> B = {p1.x - thickness, p0.y};
		Vec2<float> sizeB = {thickness, size.y - thickness};

		Vec2<float> C = {p0.x + thickness, p1.y - thickness};
		Vec2<float> sizeC = {size.x - thickness, thickness};

		Vec2<float> D = {p0.x, p0.y + thickness};
		Vec2<float> sizeD = {thickness, size.y - thickness};

		this->drawFilledRect(A, sizeA, c);
		this->drawFilledRect(B, sizeB, c);
		this->drawFilledRect(C, sizeC, c);
		this->drawFilledRect(D, sizeD, c);
	}

	void drawLine(Vec2<float> p1, Vec2<float> p2, Colour c, float thickness) override
	{
		this->flush();
		this->colouredMachine->drawLine(this->ensureEncoder(), p1 + Vec2<float>{0.5f, 0.5f},
		                                p2 + Vec2<float>{0.5f, 0.5f}, c, thickness,
		                                this->current_surface->size);
		this->drawCalls++;
	}

	void flush() override
	{
		if (this->state == State::BatchingSprites)
		{
			this->flushSprites();
		}
	}

	// Submit everything recorded so far without presenting. readBack() needs the drawing it
	// is about to copy to have been handed to the queue; queue order does the rest.
	void commitPending()
	{
		this->flush();
		this->endEncoder();
		this->commitCommandBuffer();
	}

	void present() override
	{
		this->flush();
		// A frame that only cleared still has to reach the screen.
		this->applyPendingClear(this->surfaceData(this->default_surface));
		this->endEncoder();

		// No nested @autoreleasepool here: commitCommandBuffer() pops the frame pool, and a
		// block-scoped pool around it would unwind out of order.
		id<CAMetalDrawable> drawable = [this->layer nextDrawable];
		if (!drawable)
		{
			// Occupied or off-screen: drop the frame rather than stall. Work already
			// recorded is still submitted so nothing accumulates.
			LogInfo("No Metal drawable available, skipping present");
			this->commitCommandBuffer();
			return;
		}

		auto *source = this->surfaceData(this->default_surface);
		const NSUInteger dstW = drawable.texture.width;
		const NSUInteger dstH = drawable.texture.height;

		if (source->tex.width != dstW || source->tex.height != dstH)
		{
			// The layer resized underneath us -- a device rotation is the usual cause, and the
			// window-resize event that corrects it has not been processed yet. The quad below
			// scales, so the frame is merely soft rather than cropped and offset.
			LogInfo("Drawable is {0}x{1} but the render target is {2}x{3}; scaling this frame",
			        (unsigned long long)dstW, (unsigned long long)dstH,
			        (unsigned long long)source->tex.width,
			        (unsigned long long)source->tex.height);
		}

		// Drawn as a full-screen quad rather than blitted. A CAMetalLayer drawable is
		// framebufferOnly, which makes it a legal render target but *not* a legal blit
		// destination -- Metal's validation layer rejects the copy with "destinationTexture
		// must not be a framebufferOnly texture", even though the driver tolerates it when
		// validation is off. Clearing framebufferOnly instead would work but costs the
		// drawable its lossless compression, which is exactly the wrong trade on a tiler.
		MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture = drawable.texture;
		pass.colorAttachments[0].loadAction = MTLLoadActionClear;
		pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		id<MTLCommandBuffer> commands = this->ensureCommandBuffer();
		id<MTLRenderCommandEncoder> enc =
		    [commands renderCommandEncoderWithDescriptor:pass];
		enc.label = @"OpenApoc present";
		[enc setViewport:(MTLViewport){0.0, 0.0, (double)dstW, (double)dstH, 0.0, 1.0}];
		const Vec2<unsigned int> dst{(unsigned int)dstW, (unsigned int)dstH};
		this->texturedMachine->encode(enc, false, source->tex, this->paletteTexture(),
		                              this->dummyPaletted, this->dummyRGB, this->linearSampler,
		                              {(float)source->size.x, (float)source->size.y},
		                              {0.0f, 0.0f}, {(float)dstW, (float)dstH}, {0.0f, 0.0f},
		                              0.0f, dst, {255, 255, 255, 255});
		[enc endEncoding];

		[this->commandBuffer presentDrawable:drawable];
		this->commitCommandBuffer();
	}

	void newFrame() override
	{
		this->profilePasses += this->renderPasses;
		this->profileLoads += this->passLoads;
		this->renderPasses = 0;
		this->passLoads = 0;
		const int every = Options::profileFrames.get();
		if (every > 0 && ++this->frameCount % (uint64_t)every == 0)
		{
			LogWarning("Metal passes/frame: {0:.1f} ({1:.1f} full-target loads), GPU {2:.2f} ms/cmdbuf",
			           this->profilePasses / (double)every, this->profileLoads / (double)every,
			           this->gpuTimer->takeMeanMs());
			this->profilePasses = 0;
			this->profileLoads = 0;
		}
	}

	uint64_t takeDrawCallCount() override
	{
		auto count = this->drawCalls;
		this->drawCalls = 0;
		return count;
	}

	uint64_t takeSpriteCount() override
	{
		auto count = this->spritesDrawn;
		this->spritesDrawn = 0;
		return count;
	}

	UString getName() override { return "Metal Renderer"; }
	sp<Surface> getDefaultSurface() override { return this->default_surface; }

	// Pipeline creation can fail at runtime -- the shader compiler runs in an XPC service that
	// can be interrupted, and a driver may reject a pipeline outright. Encoding a draw with a
	// nil pipeline state is a segfault, so the factory asks this before handing the renderer
	// out and falls back to GL instead.
	bool pipelinesValid() const
	{
		return this->spriteMachine->pipeline != nil && this->texturedMachine->pipeline != nil &&
		       this->colouredMachine->pipeline != nil;
	}

  private:
	void commitCommandBuffer()
	{
		if (!this->commandBuffer)
		{
			return;
		}
		auto pool = this->spriteBuffers;
		auto timer = this->gpuTimer;
		NSArray<id<MTLBuffer>> *used = [this->inFlightBuffers copy];
		[this->commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
		  if (used.count > 0)
		  {
			  pool->recycle(used);
		  }
		  timer->add((cb.GPUEndTime - cb.GPUStartTime) * 1000.0);
		}];
		[this->commandBuffer commit];
		this->commandBuffer = nil;
		this->inFlightBuffers = nil;
		if (this->framePool)
		{
			objc_autoreleasePoolPop(this->framePool);
			this->framePool = nullptr;
		}
	}
};

sp<Image> MetalSurface::readBack()
{
	if (renderer_dead)
	{
		LogWarning("MetalSurface read back after renderer shutdown");
		return nullptr;
	}
	this->owner->commitPending();

	const NSUInteger width = this->size.x;
	const NSUInteger height = this->size.y;
	const NSUInteger rowBytes = width * 4;
	id<MTLBuffer> staging = [this->device newBufferWithLength:rowBytes * height
	                                                  options:MTLResourceStorageModeShared];

	id<MTLCommandBuffer> cb = [this->queue commandBuffer];
	id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
	[blit copyFromTexture:this->tex
	              sourceSlice:0
	              sourceLevel:0
	             sourceOrigin:MTLOriginMake(0, 0, 0)
	               sourceSize:MTLSizeMake(width, height, 1)
	                 toBuffer:staging
	        destinationOffset:0
	   destinationBytesPerRow:rowBytes
	 destinationBytesPerImage:rowBytes * height];
	[blit endEncoding];
	[cb commit];
	[cb waitUntilCompleted];

	auto img = mksp<RGBImage>(Vec2<unsigned int>{(unsigned int)width, (unsigned int)height});
	RGBImageLock l(img);
	const uint8_t *src = (const uint8_t *)staging.contents;
	uint8_t *dst = (uint8_t *)l.getData();
	// BGRA to RGBA. No row reversal: unlike a GL framebuffer, a Metal render target's first
	// row is its top one, which is the order RGBImage already wants.
	for (NSUInteger i = 0; i < width * height; i++)
	{
		dst[i * 4 + 0] = src[i * 4 + 2];
		dst[i * 4 + 1] = src[i * 4 + 1];
		dst[i * 4 + 2] = src[i * 4 + 0];
		dst[i * 4 + 3] = src[i * 4 + 3];
	}
	return img;
}

class MetalRendererFactory : public RendererFactory
{
  private:
	CAMetalLayer *layer;
	bool alreadyInitialised = false;

  public:
	MetalRendererFactory(CAMetalLayer *layer) : layer(layer) {}

	OpenApoc::Renderer *create() override
	{
		if (this->alreadyInitialised)
		{
			LogWarning("Initialisation already attempted");
			return nullptr;
		}
		this->alreadyInitialised = true;

		if (!this->layer)
		{
			LogInfo("No CAMetalLayer supplied - window was not created for Metal");
			return nullptr;
		}
		id<MTLDevice> device = this->layer.device ?: MTLCreateSystemDefaultDevice();
		if (!device)
		{
			LogInfo("No Metal device available");
			return nullptr;
		}
		this->layer.device = device;
		this->layer.pixelFormat = kTargetFormat;
		// The framework clears to a fully transparent black, and a non-opaque layer would
		// let the desktop through wherever nothing was drawn. A GL window's alpha is ignored
		// by the compositor; this is how you say the same thing to CoreAnimation.
		this->layer.opaque = YES;
#if TARGET_OS_OSX
		// Stands in for SDL's swap interval, but only where the layer is not composited by the
		// window server -- borderless or fullscreen. In a window this has no effect whatever
		// (measured: both settings sit at the panel's refresh rate), because the window server
		// paces a windowed layer regardless of what the layer asks for. iOS has no equivalent
		// knob and is always display-synced.
		this->layer.displaySyncEnabled = Options::swapInterval.get() != 0;
#endif
		LogInfo("Metal device: {0}", device.name.UTF8String);

		// Compiled from source at startup rather than from a prebuilt .metallib, which keeps
		// the shaders in this file next to the code that binds them -- the same arrangement
		// the GL renderers use for their GLSL.
		NSError *error = nil;
		MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
		id<MTLLibrary> library = [device newLibraryWithSource:@(kShaderSource)
		                                              options:options
		                                                error:&error];
		if (!library)
		{
			LogError("Failed to compile Metal shaders: {0}",
			         error ? error.localizedDescription.UTF8String : "unknown error");
			return nullptr;
		}

		auto *renderer = new MetalRenderer(device, this->layer, library);
		if (!renderer->pipelinesValid())
		{
			LogError("Metal pipelines could not be built; declining so GL can be tried");
			delete renderer;
			return nullptr;
		}
		return renderer;
	}
};

} // anonymous namespace

RendererFactory *getMetalRendererFactory(void *metalLayer)
{
	return new MetalRendererFactory((__bridge CAMetalLayer *)metalLayer);
}

} // namespace OpenApoc
