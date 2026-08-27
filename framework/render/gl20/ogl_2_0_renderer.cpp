#include "framework/image.h"
#include "framework/logger.h"
#include "framework/palette.h"
#include "framework/renderer.h"
#include "framework/renderer_interface.h"
#include "library/sp.h"
#include <array>
#include <atomic>
#include <cstdlib>
#include <glm/gtx/rotate_vector.hpp>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

#include <vector>

/* Workaround MSVC not liking int64_t being defined here and in allegro */
#define GLEXT_64_TYPES_DEFINED
#include "framework/render/gl20/gl_2_0.hpp"
#include "framework/render/gl20/gl_2_0.inl"

// STATIC keeps this copy internal - the GLES3 backend links its own implementation.
#define STB_RECT_PACK_STATIC
#define STB_RECT_PACK_IMPLEMENTATION
#include "framework/render/gles30_v2/stb_rect_pack.h"

namespace
{

static std::atomic<uint64_t> drawCallCount{0};

// This backend issues one draw call per sprite, and a city frame is thousands of
// sprites. Asking the driver for the current binding before each one (glGetIntegerv)
// costs more than the draw itself, especially on macOS where GL runs on top of Metal.
// The renderer owns every binding in its context, so mirror the state here. UNKNOWN
// forces the next call through, and every delete invalidates - GL reuses names, so a
// stale entry could otherwise swallow a bind the caller needs.
class GLStateCache
{
  public:
	static constexpr int MAX_UNITS = 8;
	static constexpr int TARGET_COUNT = 3;
	static constexpr int MAX_ATTRIBS = 8;
	static constexpr GLuint UNKNOWN = 0xffffffffu;

	static int targetIndex(GLenum target)
	{
		switch (target)
		{
			case gl20::TEXTURE_1D:
				return 0;
			case gl20::TEXTURE_2D:
				return 1;
			case gl20::TEXTURE_3D:
				return 2;
			default:
				LogError("Unknown texture enum {0}", static_cast<int>(target));
				return 1;
		}
	}

	GLuint activeUnit = UNKNOWN;
	GLuint framebuffer = UNKNOWN;
	GLint unpackAlignment = -1;
	GLuint texture[MAX_UNITS][TARGET_COUNT];
	bool attribEnabled[MAX_ATTRIBS];

	GLStateCache() { reset(); }

	void reset()
	{
		activeUnit = UNKNOWN;
		framebuffer = UNKNOWN;
		unpackAlignment = -1;
		for (auto &enabled : attribEnabled)
		{
			enabled = false;
		}
		invalidateTextures();
	}

	void invalidateTextures()
	{
		for (auto &unit : texture)
		{
			for (auto &target : unit)
			{
				target = UNKNOWN;
			}
		}
	}

	bool textureBound(int unit, GLenum target, GLuint id) const
	{
		if (unit < 0 || unit >= MAX_UNITS)
		{
			return false;
		}
		return texture[unit][targetIndex(target)] == id;
	}
};

static GLStateCache glState;

// Vertex attribute arrays are never disabled by this renderer, so re-enabling one
// every sprite is pure driver overhead.
static void enableVertexAttrib(GLuint index)
{
	if (index < GLStateCache::MAX_ATTRIBS)
	{
		if (glState.attribEnabled[index])
		{
			return;
		}
		glState.attribEnabled[index] = true;
	}
	gl20::EnableVertexAttribArray(index);
}

static void disableVertexAttrib(GLuint index)
{
	if (index < GLStateCache::MAX_ATTRIBS)
	{
		if (!glState.attribEnabled[index])
		{
			return;
		}
		glState.attribEnabled[index] = false;
	}
	gl20::DisableVertexAttribArray(index);
}

using namespace OpenApoc;

std::atomic<bool> renderer_dead = true;

// Single-channel texture format for palette index data.
//
// GL_RED only became a legal external format for glTexImage2D in GL 3.0 (ARB_texture_rg).
// This backend routinely runs on a 2.1 context -- macOS hands one out whenever the 3.0
// request in displayInitialise fails, which it does there -- and on 2.1 GL_RED is
// GL_INVALID_ENUM. The call is rejected, the index texture is never populated, the driver
// reports it as unloadable and substitutes the zero texture, and every paletted sprite
// samples index 0 and draws transparent. That is the whole game UI: RGB images such as the
// mouse cursor keep working, so the window looks black rather than obviously broken.
//
// GL_LUMINANCE is the 2.1 spelling and replicates its single channel into .r, which is
// exactly what the palette shaders sample. Both spellings are valid as internal format in
// the version that accepts them, so one value serves for both arguments.
static GLenum indexTextureFormat()
{
	static const GLenum format = []() -> GLenum
	{
		const auto *version = reinterpret_cast<const char *>(gl20::GetString(gl20::VERSION));
		const int major = version ? atoi(version) : 0;
		if (major >= 3)
		{
			return gl20::RED;
		}
		LogInfo("GL version \"{0}\" predates GL_RED - using GL_LUMINANCE for palette indices",
		        version ? version : "unknown");
		return gl20::LUMINANCE;
	}();
	return format;
}

// Forward declaration needed for RendererImageData
class OGL20Renderer;

class Program
{
  public:
	GLuint prog;
	static GLuint createShader(GLenum type, const UString source)
	{
		GLuint shader = gl20::CreateShader(type);
		auto sourceString = source;
		const GLchar *string = sourceString.c_str();
		GLint stringLength = sourceString.length();
		gl20::ShaderSource(shader, 1, &string, &stringLength);
		gl20::CompileShader(shader);
		GLint compileStatus;
		gl20::GetShaderiv(shader, gl20::COMPILE_STATUS, &compileStatus);
		if (compileStatus == gl20::TRUE_)
			return shader;

		GLint logLength;
		gl20::GetShaderiv(shader, gl20::INFO_LOG_LENGTH, &logLength);

		std::unique_ptr<char[]> log(new char[logLength]);
		gl20::GetShaderInfoLog(shader, logLength, NULL, log.get());

		LogError("Shader compile error: {0}", log.get());

		gl20::DeleteShader(shader);
		return 0;
	}
	Program(const UString vertexSource, const UString fragmentSource) : prog(0)
	{
		GLuint vShader = createShader(gl20::VERTEX_SHADER, vertexSource);
		if (!vShader)
		{
			LogError("Failed to compile vertex shader");
			return;
		}
		GLuint fShader = createShader(gl20::FRAGMENT_SHADER, fragmentSource);
		if (!fShader)
		{
			LogError("Failed to compile fragment shader");
			gl20::DeleteShader(vShader);
			return;
		}

		prog = gl20::CreateProgram();
		gl20::AttachShader(prog, vShader);
		gl20::AttachShader(prog, fShader);

		gl20::DeleteShader(vShader);
		gl20::DeleteShader(fShader);

		gl20::LinkProgram(prog);

		GLint linkStatus;
		gl20::GetProgramiv(prog, gl20::LINK_STATUS, &linkStatus);
		if (linkStatus == gl20::TRUE_)
			return;

		GLint logLength;
		gl20::GetProgramiv(prog, gl20::INFO_LOG_LENGTH, &logLength);

		std::unique_ptr<char[]> log(new char[logLength]);
		gl20::GetProgramInfoLog(prog, logLength, NULL, log.get());

		LogError("Program link error: {0}", log.get());

		gl20::DeleteProgram(prog);
		prog = 0;
		return;
	}

	void uniform(GLuint loc, Colour c)
	{
		gl20::Uniform4f(loc, c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
	}

	void uniform(GLuint loc, Vec2<float> v) { gl20::Uniform2f(loc, v.x, v.y); }
	void uniform(GLuint loc, Vec2<int> v)
	{
		// FIXME: Float conversion
		gl20::Uniform2f(loc, v.x, v.y);
	}
	void uniform(GLuint loc, float v) { gl20::Uniform1f(loc, v); }
	void uniform(GLuint loc, int v) { gl20::Uniform1i(loc, v); }

	void uniform(GLuint loc, bool v) { gl20::Uniform1f(loc, (v ? 1.0f : 0.0f)); }

	virtual ~Program()
	{
		if (prog)
			gl20::DeleteProgram(prog);
	}
};

class SpriteProgram : public Program
{
  protected:
	SpriteProgram(const UString vertexSource, const UString fragmentSource)
	    : Program(vertexSource, fragmentSource)
	{
	}

  public:
	GLint posLoc = -1;
	GLint texcoordLoc = -1;
	GLint screenSizeLoc = -1;
	GLint texLoc = -1;
	GLint flipYLoc = -1;
	GLint tintLoc = -1;
};
const char *RGBProgram_vertexSource = {
    "#version 110\n"
    "attribute vec2 position;\n"
    "attribute vec2 texcoord_in;\n"
    "varying vec2 texcoord;\n"
    "uniform vec2 screenSize;\n"
    "uniform bool flipY;\n"
    "void main() {\n"
    "  texcoord = texcoord_in;\n"
    "  vec2 tmpPos = position;\n"
    "  tmpPos /= screenSize;\n"
    "  tmpPos -= vec2(0.5,0.5);\n"
    "  if (flipY) gl_Position = vec4((tmpPos.x*2.0), -(tmpPos.y*2.0),0.0,1.0);\n"
    "  else gl_Position = vec4((tmpPos.x*2.0), (tmpPos.y*2.0),0.0,1.0);\n"
    "}\n"};
const char *RGBProgram_fragmentSource = {"#version 110\n"
                                         "varying vec2 texcoord;\n"
                                         "uniform sampler2D tex;\n"
                                         "uniform vec4 tint;\n"
                                         "void main() {\n"
                                         " gl_FragColor = tint * texture2D(tex, texcoord);\n"
                                         "}\n"};
class RGBProgram : public SpriteProgram
{
  private:
	Vec2<int> currentScreenSize;
	bool currentFlipY;
	GLint currentTexUnit;
	Colour currentTint;

  public:
	RGBProgram()
	    : SpriteProgram(RGBProgram_vertexSource, RGBProgram_fragmentSource),
	      currentScreenSize(0, 0), currentFlipY(0), currentTexUnit(0), currentTint(0, 0, 0, 0)
	{
		this->posLoc = gl20::GetAttribLocation(this->prog, "position");
		if (this->posLoc < 0)
			LogError("\"position\" attribute not found in shader");
		this->texcoordLoc = gl20::GetAttribLocation(this->prog, "texcoord_in");
		if (this->texcoordLoc < 0)
			LogError("\"texcoord_in\" attribute not found in shader");
		this->screenSizeLoc = gl20::GetUniformLocation(this->prog, "screenSize");
		if (this->screenSizeLoc < 0)
			LogError("\"screenSize\" uniform not found in shader");
		this->texLoc = gl20::GetUniformLocation(this->prog, "tex");
		if (this->texLoc < 0)
			LogError("\"tex\" uniform not found in shader");
		this->flipYLoc = gl20::GetUniformLocation(this->prog, "flipY");
		if (this->flipYLoc < 0)
			LogError("\"flipY\" uniform not found in shader");
		this->tintLoc = gl20::GetUniformLocation(this->prog, "tint");
		if (this->tintLoc < 0)
			LogError("\"tint\" uniform not found in shader");
	}
	void setUniforms(Vec2<int> screenSize, bool flipY, Colour tint = {255, 255, 255, 255},
	                 GLint texUnit = 0)
	{
		if (screenSize != currentScreenSize)
		{
			currentScreenSize = screenSize;
			this->uniform(this->screenSizeLoc, screenSize);
		}
		if (texUnit != currentTexUnit)
		{
			currentTexUnit = texUnit;
			this->uniform(this->texLoc, texUnit);
		}
		if (flipY != currentFlipY)
		{
			currentFlipY = flipY;
			this->uniform(this->flipYLoc, flipY);
		}
		if (tint != currentTint)
		{
			currentTint = tint;
			this->uniform(this->tintLoc, tint);
		}
	}
};
const char *PaletteProgram_vertexSource = {
    "#version 110\n"
    "attribute vec2 position;\n"
    "attribute vec2 texcoord_in;\n"
    "varying vec2 texcoord;\n"
    "uniform vec2 screenSize;\n"
    "uniform bool flipY;\n"
    "void main() {\n"
    "  texcoord = texcoord_in;\n"
    "  vec2 tmpPos = position;\n"
    "  tmpPos /= screenSize;\n"
    "  tmpPos -= vec2(0.5,0.5);\n"
    "  if (flipY) gl_Position = vec4((tmpPos.x*2.0), -(tmpPos.y*2.0),0.0,1.0);\n"
    "  else gl_Position = vec4((tmpPos.x*2.0), (tmpPos.y*2.0),0.0,1.0);\n"
    "}\n"};
const char *PaletteProgram_fragmentSource = {
    "#version 110\n"
    "varying vec2 texcoord;\n"
    "uniform sampler2D tex;\n"
    "uniform sampler2D pal;\n"
    "uniform vec4 tint;\n"
    "void main() {\n"
    " float idx = texture2D(tex, texcoord,0.0).r;\n"
    " gl_FragColor = tint * texture2D(pal, vec2(idx,0.0),0.0);\n"
    "}\n"};
class PaletteProgram : public SpriteProgram
{
  private:
	Vec2<int> currentScreenSize;
	bool currentFlipY;
	GLint currentTexUnit;
	GLint currentPalUnit;
	Colour currentTint;

  public:
	GLint palLoc;
	PaletteProgram()
	    : SpriteProgram(PaletteProgram_vertexSource, PaletteProgram_fragmentSource),
	      currentScreenSize(0, 0), currentFlipY(false), currentTexUnit(0), currentPalUnit(0),
	      currentTint(0, 0, 0, 0)
	{
		this->posLoc = gl20::GetAttribLocation(this->prog, "position");
		this->texcoordLoc = gl20::GetAttribLocation(this->prog, "texcoord_in");
		this->screenSizeLoc = gl20::GetUniformLocation(this->prog, "screenSize");
		this->texLoc = gl20::GetUniformLocation(this->prog, "tex");
		this->palLoc = gl20::GetUniformLocation(this->prog, "pal");
		this->flipYLoc = gl20::GetUniformLocation(this->prog, "flipY");
		this->tintLoc = gl20::GetUniformLocation(this->prog, "tint");
	}
	void setUniforms(Vec2<int> screenSize, bool flipY, Colour tint = {255, 255, 255, 255},
	                 GLint texUnit = 0, GLint palUnit = 1)
	{
		if (screenSize != currentScreenSize)
		{
			currentScreenSize = screenSize;
			this->uniform(this->screenSizeLoc, screenSize);
		}
		if (texUnit != currentTexUnit)
		{
			currentTexUnit = texUnit;
			this->uniform(this->texLoc, texUnit);
		}
		if (palUnit != currentPalUnit)
		{
			currentPalUnit = palUnit;
			this->uniform(this->palLoc, palUnit);
		}
		if (currentFlipY != flipY)
		{
			currentFlipY = flipY;
			this->uniform(this->flipYLoc, flipY);
		}
		if (tint != currentTint)
		{
			currentTint = tint;
			this->uniform(this->tintLoc, tint);
		}
	}
};

// Same palette lookup as PaletteProgram, but the tint travels per vertex so sprites
// with different tints stay in one batch.
const char *PaletteBatchProgram_vertexSource = {
    "#version 110\n"
    "attribute vec2 position;\n"
    "attribute vec2 texcoord_in;\n"
    "attribute vec4 tint_in;\n"
    "varying vec2 texcoord;\n"
    "varying vec4 tint;\n"
    "uniform vec2 screenSize;\n"
    "uniform bool flipY;\n"
    "void main() {\n"
    "  texcoord = texcoord_in;\n"
    "  tint = tint_in;\n"
    "  vec2 tmpPos = position;\n"
    "  tmpPos /= screenSize;\n"
    "  tmpPos -= vec2(0.5,0.5);\n"
    "  if (flipY) gl_Position = vec4((tmpPos.x*2.0), -(tmpPos.y*2.0),0.0,1.0);\n"
    "  else gl_Position = vec4((tmpPos.x*2.0), (tmpPos.y*2.0),0.0,1.0);\n"
    "}\n"};
const char *PaletteBatchProgram_fragmentSource = {
    "#version 110\n"
    "varying vec2 texcoord;\n"
    "varying vec4 tint;\n"
    "uniform sampler2D tex;\n"
    "uniform sampler2D pal;\n"
    "void main() {\n"
    " float idx = texture2D(tex, texcoord,0.0).r;\n"
    " gl_FragColor = tint * texture2D(pal, vec2(idx,0.0),0.0);\n"
    "}\n"};
class PaletteBatchProgram : public Program
{
  private:
	Vec2<int> currentScreenSize{0, 0};
	bool currentFlipY = false;
	GLint currentTexUnit = -1;
	GLint currentPalUnit = -1;

  public:
	GLint posLoc = -1;
	GLint texcoordLoc = -1;
	GLint tintLoc = -1;
	GLint screenSizeLoc = -1;
	GLint texLoc = -1;
	GLint palLoc = -1;
	GLint flipYLoc = -1;

	PaletteBatchProgram()
	    : Program(PaletteBatchProgram_vertexSource, PaletteBatchProgram_fragmentSource)
	{
		this->posLoc = gl20::GetAttribLocation(this->prog, "position");
		this->texcoordLoc = gl20::GetAttribLocation(this->prog, "texcoord_in");
		this->tintLoc = gl20::GetAttribLocation(this->prog, "tint_in");
		this->screenSizeLoc = gl20::GetUniformLocation(this->prog, "screenSize");
		this->texLoc = gl20::GetUniformLocation(this->prog, "tex");
		this->palLoc = gl20::GetUniformLocation(this->prog, "pal");
		this->flipYLoc = gl20::GetUniformLocation(this->prog, "flipY");
	}

	bool valid() const
	{
		return prog && posLoc >= 0 && texcoordLoc >= 0 && tintLoc >= 0 && screenSizeLoc >= 0 &&
		       texLoc >= 0 && palLoc >= 0 && flipYLoc >= 0;
	}

	void setUniforms(Vec2<int> screenSize, bool flipY, GLint texUnit = 0, GLint palUnit = 1)
	{
		if (screenSize != currentScreenSize)
		{
			currentScreenSize = screenSize;
			this->uniform(this->screenSizeLoc, screenSize);
		}
		if (texUnit != currentTexUnit)
		{
			currentTexUnit = texUnit;
			this->uniform(this->texLoc, texUnit);
		}
		if (palUnit != currentPalUnit)
		{
			currentPalUnit = palUnit;
			this->uniform(this->palLoc, palUnit);
		}
		if (flipY != currentFlipY)
		{
			currentFlipY = flipY;
			this->uniform(this->flipYLoc, flipY);
		}
	}
};

const char *SolidColourProgram_vertexSource = {
    "#version 110\n"
    "attribute vec2 position;\n"
    "uniform vec2 screenSize;\n"
    "uniform bool flipY;\n"
    "void main() {\n"
    "  vec2 tmpPos = position;\n"
    "  tmpPos /= screenSize;\n"
    "  tmpPos -= vec2(0.5,0.5);\n"
    "  if (flipY) gl_Position = vec4((tmpPos.x*2.0), -(tmpPos.y*2.0),0.0,1.0);\n"
    "  else gl_Position = vec4((tmpPos.x*2.0), (tmpPos.y*2.0),0.0,1.0);\n"
    "}\n"};
const char *SolidColourProgram_fragmentSource = {"#version 110\n"
                                                 "uniform vec4 colour;\n"
                                                 "void main() {\n"
                                                 " gl_FragColor = colour;\n"
                                                 "}\n"};
class SolidColourProgram : public Program
{
  private:
	Vec2<int> currentScreenSize;
	bool currentFlipY;
	Colour currentColour;

  public:
	GLuint posLoc;
	GLuint screenSizeLoc;
	GLuint colourLoc;
	GLuint flipYLoc;
	SolidColourProgram()
	    : Program(SolidColourProgram_vertexSource, SolidColourProgram_fragmentSource),
	      currentScreenSize(0, 0), currentFlipY(false), currentColour(0, 0, 0, 0)
	{
		this->posLoc = gl20::GetAttribLocation(this->prog, "position");
		this->screenSizeLoc = gl20::GetUniformLocation(this->prog, "screenSize");
		this->colourLoc = gl20::GetUniformLocation(this->prog, "colour");
		this->flipYLoc = gl20::GetUniformLocation(this->prog, "flipY");
	}
	void setUniforms(Vec2<int> screenSize, bool flipY, Colour colour)
	{
		if (currentScreenSize != screenSize)
		{
			currentScreenSize = screenSize;
			this->uniform(this->screenSizeLoc, screenSize);
		}
		if (currentColour != colour)
		{
			currentColour = colour;
			this->uniform(this->colourLoc, colour);
		}
		if (currentFlipY != flipY)
		{
			currentFlipY = flipY;
			this->uniform(this->flipYLoc, flipY);
		}
	}
};
class Quad
{
  public:
	std::array<Vec2<float>, 4> vertices;
	std::array<Vec2<float>, 4> texcoords;
	Quad(const Rect<float> &position, const Rect<float> texCoords = {{0, 0}, {1, 1}},
	     const Vec2<float> &rotationCenter = {0.0f, 0.0f}, float rotationAngleRadians = 0.0f)
	{
		texcoords = {{
		    Vec2<float>{texCoords.p0},
		    Vec2<float>{texCoords.p1.x, texCoords.p0.y},
		    Vec2<float>{texCoords.p0.x, texCoords.p1.y},
		    Vec2<float>{texCoords.p1},
		}};

		if (rotationAngleRadians != 0.0f)
		{
			auto rotMatrix = glm::rotate(rotationAngleRadians, Vec3<float>{0.0f, 0.0f, 1.0f});
			Vec2<float> size = position.p1 - position.p0;
			vertices = {{
			    Vec2<float>{0.0f, 0.0f},
			    Vec2<float>{size.x, 0.0f},
			    Vec2<float>{0.0f, size.y},
			    Vec2<float>{size},
			}};
			for (auto &p : vertices)
			{
				p -= rotationCenter;
				glm::vec4 transformed = rotMatrix * glm::vec4{p.x, p.y, 0.0f, 1.0f};
				p.x = transformed.x;
				p.y = transformed.y;
				p += rotationCenter;
				p += position.p0;
			}
		}
		else
		{
			vertices = {{
			    Vec2<float>{position.p0},
			    Vec2<float>{position.p1.x, position.p0.y},
			    Vec2<float>{position.p0.x, position.p1.y},
			    Vec2<float>{position.p1},
			}};
		}
	}
	void draw(GLuint vertexAttribPos, GLuint texcoordAttribPos)
	{
		enableVertexAttrib(vertexAttribPos);
		gl20::VertexAttribPointer(vertexAttribPos, 2, gl20::FLOAT, gl20::FALSE_, 0, &vertices);
		enableVertexAttrib(texcoordAttribPos);
		gl20::VertexAttribPointer(texcoordAttribPos, 2, gl20::FLOAT, gl20::FALSE_, 0, &texcoords);
		gl20::DrawArrays(gl20::TRIANGLE_STRIP, 0, 4);
		drawCallCount++;
	}
	void draw(GLuint vertexAttribPos)
	{
		enableVertexAttrib(vertexAttribPos);
		gl20::VertexAttribPointer(vertexAttribPos, 2, gl20::FLOAT, gl20::FALSE_, 0, &vertices);
		gl20::DrawArrays(gl20::TRIANGLE_STRIP, 0, 4);
		drawCallCount++;
	}
};
class Line
{
  public:
	std::array<Vec2<float>, 2> vertices;
	float thickness;
	Line(Vec2<float> p0, Vec2<float> p1, float thickness) : thickness(thickness)
	{
		vertices = {{p0, p1}};
	}
	void draw(GLuint vertexAttribPos)
	{
		gl20::LineWidth(thickness);
		enableVertexAttrib(vertexAttribPos);
		gl20::VertexAttribPointer(vertexAttribPos, 2, gl20::FLOAT, gl20::FALSE_, 0, &vertices);
		gl20::DrawArrays(gl20::LINES, 0, 2);
		drawCallCount++;
	}
};
class ActiveTexture
{
	ActiveTexture(const ActiveTexture &) = delete;

  public:
	static GLenum getUnitEnum(int unit) { return gl20::TEXTURE0 + unit; }

	ActiveTexture(int unit)
	{
		const GLenum unitEnum = getUnitEnum(unit);
		if (glState.activeUnit == unitEnum)
		{
			return;
		}
		gl20::ActiveTexture(unitEnum);
		glState.activeUnit = unitEnum;
	}
};

class UnpackAlignment
{
	UnpackAlignment(const UnpackAlignment &) = delete;

  public:
	UnpackAlignment(int align)
	{
		if (glState.unpackAlignment == align)
		{
			return;
		}
		gl20::PixelStorei(gl20::UNPACK_ALIGNMENT, align);
		glState.unpackAlignment = align;
	}
};

class BindTexture
{
	BindTexture(const BindTexture &) = delete;

  public:
	GLenum bind;
	int unit;

	BindTexture(GLuint id, GLint unit = 0, GLenum bind = gl20::TEXTURE_2D) : bind(bind), unit(unit)
	{
		ActiveTexture a(unit);
		if (unit < 0 || unit >= GLStateCache::MAX_UNITS)
		{
			gl20::BindTexture(bind, id);
			return;
		}
		GLuint &cached = glState.texture[unit][GLStateCache::targetIndex(bind)];
		if (cached == id)
		{
			return;
		}
		gl20::BindTexture(bind, id);
		cached = id;
	}
};

template <GLenum param> class TexParam
{
	TexParam(const TexParam &) = delete;

  public:
	GLuint id;
	GLenum type;

	TexParam(GLuint id, GLint value, GLenum type = gl20::TEXTURE_2D) : id(id), type(type)
	{
		GLint prevValue;
		BindTexture b(id, 0, type);
		gl20::GetTexParameteriv(type, param, &prevValue);
		if (prevValue == value)
		{
			return;
		}
		gl20::TexParameteri(type, param, value);
	}
};

class BindFramebuffer
{
	BindFramebuffer(const BindFramebuffer &) = delete;

  public:
	BindFramebuffer(GLuint id)
	{
		if (glState.framebuffer == id)
		{
			return;
		}
		gl20::BindFramebufferEXT(gl20::FRAMEBUFFER_EXT, id);
		glState.framebuffer = id;
	}
};

// Defined once OGL20Renderer is complete. Reading a surface back has to see sprites
// that are still sitting in the batch buffer.
static void flushRendererBatch(OGL20Renderer *r);

class FBOData : public RendererImageData
{
  public:
	GLuint fbo;
	GLuint tex;
	Vec2<float> size;
	OGL20Renderer *owner;
	// Constructor /only/ to be used for default surface (FBO ID == 0)
	FBOData(GLuint fbo, Vec2<int> size, OGL20Renderer *owner)
	    // FIXME: Check FBO == 0
	    // FIXME: Warn if trying to texture from FBO 0
	    : fbo(fbo), tex(-1), size(size), owner(owner)
	{
	}

	void resize(Vec2<unsigned int> newSize) override
	{
		this->size = {(float)newSize.x, (float)newSize.y};
	}

	sp<Image> readBack() override
	{
		flushRendererBatch(this->owner);
		auto img = mksp<RGBImage>(size);
		BindFramebuffer f(this->fbo);

		RGBImageLock l(img);
		// Foiled once again by inverted y! Read in each line bottom->top writing top->bottom in the
		// image
		uint8_t *imgPos = reinterpret_cast<uint8_t *>(l.getData());
		unsigned imgStride = size.x * 4;
		for (int y = 0; y < size.y; y++)
		{
			gl20::ReadPixels(0, size.y - 1 - y, size.x, size.y - y, gl20::RGBA, gl20::UNSIGNED_BYTE,
			                 imgPos);
			imgPos += imgStride;
		}

		return img;
	}

	FBOData(Vec2<int> size, OGL20Renderer *owner) : size(size.x, size.y), owner(owner)
	{
		gl20::GenTextures(1, &this->tex);
		BindTexture b(this->tex);
		gl20::TexImage2D(gl20::TEXTURE_2D, 0, gl20::RGBA8, size.x, size.y, 0, gl20::RGBA,
		                 gl20::UNSIGNED_BYTE, NULL);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_MIN_FILTER, gl20::NEAREST);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_MAG_FILTER, gl20::NEAREST);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_WRAP_S, gl20::CLAMP_TO_EDGE);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_WRAP_T, gl20::CLAMP_TO_EDGE);

		gl20::GenFramebuffersEXT(1, &this->fbo);
		BindFramebuffer f(this->fbo);

		gl20::FramebufferTexture2DEXT(gl20::FRAMEBUFFER_EXT, gl20::COLOR_ATTACHMENT0_EXT,
		                              gl20::TEXTURE_2D, this->tex, 0);
		LogAssert(gl20::CheckFramebufferStatusEXT(gl20::FRAMEBUFFER_EXT) ==
		          gl20::FRAMEBUFFER_COMPLETE_EXT);
	}
	~FBOData() override;
};

class GLRGBImage : public RendererImageData
{
  public:
	GLuint texID;
	Vec2<float> size;
	std::weak_ptr<RGBImage> parent;
	OGL20Renderer *owner;
	GLRGBImage(sp<RGBImage> parent, OGL20Renderer *owner)
	    : size(parent->size), parent(parent), owner(owner)
	{
		RGBImageLock l(parent, ImageLockUse::Read);
		gl20::GenTextures(1, &this->texID);
		BindTexture b(this->texID);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_MIN_FILTER, gl20::NEAREST);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_MAG_FILTER, gl20::NEAREST);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_WRAP_S, gl20::CLAMP_TO_EDGE);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_WRAP_T, gl20::CLAMP_TO_EDGE);
		gl20::TexImage2D(gl20::TEXTURE_2D, 0, gl20::RGBA, parent->size.x, parent->size.y, 0,
		                 gl20::RGBA, gl20::UNSIGNED_BYTE, l.getData());
	}
	~GLRGBImage() override;
};

class GLPalette : public RendererImageData
{
  public:
	GLuint texID;
	Vec2<float> size;
	std::weak_ptr<Palette> parent;
	OGL20Renderer *owner;
	GLPalette(sp<Palette> parent, OGL20Renderer *owner)
	    : size(Vec2<float>(parent->colours.size(), 1)), parent(parent), owner(owner)
	{
		gl20::GenTextures(1, &this->texID);
		BindTexture b(this->texID);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_MIN_FILTER, gl20::NEAREST);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_MAG_FILTER, gl20::NEAREST);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_WRAP_S, gl20::CLAMP_TO_EDGE);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_WRAP_T, gl20::CLAMP_TO_EDGE);
		gl20::TexImage2D(gl20::TEXTURE_2D, 0, gl20::RGBA, parent->colours.size(), 1, 0, gl20::RGBA,
		                 gl20::UNSIGNED_BYTE, parent->colours.data());
	}
	~GLPalette() override;
};

// A city frame is thousands of paletted sprites. One texture each means one draw call
// each, which is what makes this backend slow. Packing them into shared pages lets a
// whole screen of tiles go out as a handful of batched draws instead.
class PaletteAtlas
{
  public:
	static constexpr int PAGE_SIZE = 2048;
	// Keep a transparent gutter so nearest sampling at a quad edge can never land on a
	// neighbour.
	static constexpr int PADDING = 1;
	// Anything approaching page size would evict everything else; those keep their own
	// texture and their own draw call.
	static constexpr int MAX_SPRITE = 512;

	struct Page
	{
		GLuint texID = 0;
		stbrp_context context{};
		std::vector<stbrp_node> nodes;
	};

	void reset() { pages.clear(); }

	GLuint pageTexture(int page) const
	{
		if (page < 0 || page >= (int)pages.size())
		{
			return 0;
		}
		return pages[page]->texID;
	}

	// Returns false if the sprite should keep its own texture.
	bool add(const sp<PaletteImage> &image, int &outPage, Vec2<int> &outPos)
	{
		const int w = (int)image->size.x;
		const int h = (int)image->size.y;
		if (w <= 0 || h <= 0 || w > MAX_SPRITE || h > MAX_SPRITE)
		{
			return false;
		}

		for (size_t i = 0; i < pages.size(); i++)
		{
			if (packInto(*pages[i], w, h, outPos))
			{
				outPage = (int)i;
				upload(*pages[i], image, outPos);
				return true;
			}
		}

		pages.push_back(newPage());
		if (!packInto(*pages.back(), w, h, outPos))
		{
			LogError("Failed to pack {0}x{1} sprite into an empty atlas page", w, h);
			pages.pop_back();
			return false;
		}
		outPage = (int)pages.size() - 1;
		upload(*pages.back(), image, outPos);
		return true;
	}

  private:
	std::vector<up<Page>> pages;

	static up<Page> newPage()
	{
		auto page = std::make_unique<Page>();
		// stb_rect_pack wants roughly one node per column of the target.
		page->nodes.resize(PAGE_SIZE);
		stbrp_init_target(&page->context, PAGE_SIZE, PAGE_SIZE, page->nodes.data(),
		                  (int)page->nodes.size());

		gl20::GenTextures(1, &page->texID);
		BindTexture b(page->texID);
		UnpackAlignment align(1);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_MIN_FILTER, gl20::NEAREST);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_MAG_FILTER, gl20::NEAREST);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_WRAP_S, gl20::CLAMP_TO_EDGE);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_WRAP_T, gl20::CLAMP_TO_EDGE);
		// Index 0 is the transparent palette entry, so a zeroed page reads as empty.
		const std::vector<uint8_t> blank((size_t)PAGE_SIZE * PAGE_SIZE, 0);
		gl20::TexImage2D(gl20::TEXTURE_2D, 0, 1, PAGE_SIZE, PAGE_SIZE, 0, gl20::RED,
		                 gl20::UNSIGNED_BYTE, blank.data());
		LogInfo("Created palette atlas page {0}x{0}", PAGE_SIZE);
		return page;
	}

	static bool packInto(Page &page, int w, int h, Vec2<int> &outPos)
	{
		stbrp_rect rect{};
		rect.w = (stbrp_coord)(w + PADDING);
		rect.h = (stbrp_coord)(h + PADDING);
		stbrp_pack_rects(&page.context, &rect, 1);
		if (!rect.was_packed)
		{
			return false;
		}
		outPos = {rect.x, rect.y};
		return true;
	}

	static void upload(Page &page, const sp<PaletteImage> &image, Vec2<int> pos)
	{
		PaletteImageLock l(image, ImageLockUse::Read);
		BindTexture b(page.texID);
		UnpackAlignment align(1);
		gl20::TexSubImage2D(gl20::TEXTURE_2D, 0, pos.x, pos.y, image->size.x, image->size.y,
		                    gl20::RED, gl20::UNSIGNED_BYTE, l.getData());
	}
};

static PaletteAtlas paletteAtlas;

class GLPaletteImage : public RendererImageData
{
  public:
	GLuint texID = 0;
	Vec2<float> size;
	// >= 0 when this sprite lives in a shared atlas page and can be batched.
	int atlasPage = -1;
	Vec2<int> atlasPos{0, 0};
	std::weak_ptr<PaletteImage> parent;
	OGL20Renderer *owner;
	GLPaletteImage(sp<PaletteImage> parent, OGL20Renderer *owner)
	    : size(parent->size), parent(parent), owner(owner)
	{
		if (paletteAtlas.add(parent, this->atlasPage, this->atlasPos))
		{
			return;
		}

		PaletteImageLock l(parent, ImageLockUse::Read);
		gl20::GenTextures(1, &this->texID);
		BindTexture b(this->texID);
		UnpackAlignment align(1);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_MIN_FILTER, gl20::NEAREST);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_MAG_FILTER, gl20::NEAREST);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_WRAP_S, gl20::CLAMP_TO_EDGE);
		gl20::TexParameteri(gl20::TEXTURE_2D, gl20::TEXTURE_WRAP_T, gl20::CLAMP_TO_EDGE);
		const GLenum indexFormat = indexTextureFormat();
		gl20::TexImage2D(gl20::TEXTURE_2D, 0, indexFormat, parent->size.x, parent->size.y, 0,
		                 indexFormat, gl20::UNSIGNED_BYTE, l.getData());
	}
	~GLPaletteImage() override;
};

class OGL20Renderer : public Renderer
{
  private:
	sp<RGBProgram> rgbProgram;
	sp<SolidColourProgram> colourProgram;
	sp<PaletteProgram> paletteProgram;
	sp<PaletteBatchProgram> paletteBatchProgram;
	GLuint currentBoundProgram;
	GLuint currentBoundFBO;

	struct BatchVertex
	{
		float x, y;
		float u, v;
		uint8_t r, g, b, a;
	};
	std::vector<BatchVertex> batchVertices;
	GLuint batchVBO = 0;
	int batchPage = -1;
	GLuint batchPalTex = 0;
	bool batchFlipY = false;

	sp<Surface> currentSurface;
	Vec2<unsigned int> currentViewport{0, 0};
	sp<Palette> currentPalette;

	friend class RendererSurfaceBinding;
	void setSurface(sp<Surface> s) override
	{
		// Identity alone is not enough to skip the rebind: the default surface keeps its
		// identity across a window resize while its size changes underneath us, and the
		// viewport would stay at the old extent.
		if (this->currentSurface == s && this->currentViewport == s->size)
		{
			return;
		}
		this->flush();
		this->currentSurface = s;
		if (!s->rendererPrivateData)
			s->rendererPrivateData.reset(new FBOData(s->size, this));

		FBOData *fbo = static_cast<FBOData *>(s->rendererPrivateData.get());
		BindFramebuffer b(fbo->fbo);
		this->currentBoundFBO = fbo->fbo;
		gl20::Viewport(0, 0, s->size.x, s->size.y);
		this->currentViewport = s->size;
	}
	sp<Surface> getSurface() override { return currentSurface; }
	sp<Surface> defaultSurface;

	std::thread::id bound_thread;
	std::mutex destroyed_texture_list_mutex;
	std::list<GLuint> destroyed_texture_list;
	std::mutex destroyed_framebuffer_list_mutex;
	std::list<GLuint> destroyed_framebuffer_list;

  public:
	OGL20Renderer()
	    : rgbProgram(new RGBProgram()), colourProgram(new SolidColourProgram()),
	      paletteProgram(new PaletteProgram()), paletteBatchProgram(new PaletteBatchProgram()),
	      currentBoundProgram(0), currentBoundFBO(0)
	{
		glState.reset();
		paletteAtlas.reset();
		gl20::GenBuffers(1, &this->batchVBO);
		this->batchVertices.reserve(6 * 4096);
		if (!this->paletteBatchProgram->valid())
		{
			LogWarning("Palette batch shader unavailable - falling back to one draw per sprite");
		}
		this->bound_thread = std::this_thread::get_id();
		GLint viewport[4];
		gl20::GetIntegerv(gl20::VIEWPORT, viewport);
		LogInfo("Viewport {{{0},{1},{2},{3}}}", viewport[0], viewport[1], viewport[2], viewport[3]);
		LogAssert(viewport[0] == 0 && viewport[1] == 0);
		this->defaultSurface = mksp<Surface>(Vec2<int>{viewport[2], viewport[3]});
		this->defaultSurface->rendererPrivateData.reset(
		    new FBOData(0, {viewport[2], viewport[3]}, this));
		this->currentSurface = this->defaultSurface;

		GLint maxTexUnits;
		gl20::GetIntegerv(gl20::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTexUnits);
		LogInfo("MAX_COMBINED_TEXTURE_IMAGE_UNITS: {0}", maxTexUnits);
		gl20::Enable(gl20::BLEND);
		gl20::BlendFuncSeparate(gl20::SRC_ALPHA, gl20::ONE_MINUS_SRC_ALPHA, gl20::SRC_ALPHA,
		                        gl20::DST_ALPHA);
		renderer_dead = false;
	}
	~OGL20Renderer() override
	{
		if (this->batchVBO)
		{
			gl20::DeleteBuffers(1, &this->batchVBO);
		}
		renderer_dead = true;
	};
	void clear(Colour c = Colour{0, 0, 0, 0}) override
	{
		this->flush();
		gl20::ClearColor(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
		gl20::Clear(gl20::COLOR_BUFFER_BIT);
	}
	void setPalette(sp<Palette> p) override
	{
		if (p == this->currentPalette)
			return;
		this->flush();
		if (!p->rendererPrivateData)
			p->rendererPrivateData.reset(new GLPalette(p, this));
		this->currentPalette = p;
	}
	sp<Palette> getPalette() override { return this->currentPalette; }
	void draw(sp<Image> image, Vec2<float> position) override
	{
		drawScaled(image, position, image->size, Scaler::Nearest);
	}
	void drawRotated(sp<Image> image, Vec2<float> center, Vec2<float> position,
	                 float angle) override
	{
		auto size = image->size;
		sp<RGBImage> rgbImage = std::dynamic_pointer_cast<RGBImage>(image);
		if (rgbImage)
		{
			GLRGBImage *img = dynamic_cast<GLRGBImage *>(rgbImage->rendererPrivateData.get());
			if (!img)
			{
				img = new GLRGBImage(rgbImage, this);
				image->rendererPrivateData.reset(img);
			}
			this->drawRgb(*img, position, size, Scaler::Linear, center, angle);
			return;
		}

		sp<PaletteImage> paletteImage = std::dynamic_pointer_cast<PaletteImage>(image);
		LogError("Unsupported image type");
	}
	void drawScaled(sp<Image> image, Vec2<float> position, Vec2<float> size,
	                Scaler scaler = Scaler::Linear) override
	{
		drawScaledImage(image, position, size, scaler);
	}
	void drawScaledTinted(sp<Image> image, Vec2<float> position, Vec2<float> size, Colour tint,
	                      Scaler scaler = Scaler::Nearest) override
	{
		drawScaledImage(image, position, size, scaler, tint);
	}
	void drawScaledImage(sp<Image> image, Vec2<float> position, Vec2<float> size,
	                     Scaler scaler = Scaler::Linear, Colour tint = {255, 255, 255, 255})
	{

		sp<RGBImage> rgbImage = std::dynamic_pointer_cast<RGBImage>(image);
		if (rgbImage)
		{
			GLRGBImage *img = dynamic_cast<GLRGBImage *>(rgbImage->rendererPrivateData.get());
			if (!img)
			{
				img = new GLRGBImage(rgbImage, this);
				image->rendererPrivateData.reset(img);
			}
			this->drawRgb(*img, position, size, scaler, {0, 0}, 0, tint);
			return;
		}

		sp<PaletteImage> paletteImage = std::dynamic_pointer_cast<PaletteImage>(image);
		if (paletteImage)
		{
			GLPaletteImage *img =
			    dynamic_cast<GLPaletteImage *>(paletteImage->rendererPrivateData.get());
			if (!img)
			{
				img = new GLPaletteImage(paletteImage, this);
				image->rendererPrivateData.reset(img);
			}
			if (scaler != Scaler::Nearest)
			{
				// blending indices doesn't make sense. You'll have to render
				// it to an RGB surface then scale that
				LogError("Only nearest scaler is supported on paletted images");
			}
			this->drawPalette(*img, position, size, tint);
			return;
		}

		sp<Surface> surface = std::dynamic_pointer_cast<Surface>(image);
		if (surface)
		{
			FBOData *fbo = dynamic_cast<FBOData *>(surface->rendererPrivateData.get());
			if (!fbo)
			{
				fbo = new FBOData(image->size, this);
				image->rendererPrivateData.reset(fbo);
			}
			this->drawSurface(*fbo, position, size, scaler, tint);
			return;
		}
		LogError("Unsupported image type");
	}

	void drawTinted(sp<Image> i, Vec2<float> position, Colour tint) override
	{
		drawScaledImage(i, position, i->size, Scaler::Nearest, tint);
	}
	void drawFilledRect(Vec2<float> position, Vec2<float> size, Colour c) override
	{
		bindProgram(colourProgram);
		Rect<float> pos(position, position + size);
		bool flipY = false;
		if (currentBoundFBO == 0)
			flipY = true;
		colourProgram->setUniforms(this->currentSurface->size, flipY, c);
		Quad q(pos, Rect<float>{{0, 0}, {1, 1}});
		q.draw(colourProgram->posLoc);
	}
	void drawRect(Vec2<float> position, Vec2<float> size, Colour c, float thickness = 1.0) override
	{

		// The lines are all shifted in x/y by {capsize} to ensure the corners are correctly covered
		// and don't overlap (which may be an issue if alpha != 1.0f:
		//
		// The cap 'ownership' for lines 1,2,3,4 is shifted by (x-1), (y-1), (x+1), (y+1)
		// In picture form:
		//
		// 4333333
		// 4     2
		// 4     2
		// 1111112
		//
		// At the corners we have a bit of complexity to correctly cap & avoid overlap:
		//
		// p0 = position
		// p1 = position + size
		//
		//  p1.y|----+-------+---------------------------+
		//      |    |       |                           |
		//      v    |       |   Line 3                  |
		//      Y    |       |                           |
		//      ^    |       C-------------------+-------+
		//      |    | Line 4|                   |       |
		//      |    |       |                   |       |
		//      |    |       |                   |       |
		//      |    |       |                   | Line 2|
		//      |    |       |                   |       |
		//      |    D-------+-------------------+       |
		//      |    |            ^              |       |
		//      |    |  Line 1    | thickness    |       |
		//      |    |            v              |       |
		//  p0.y|----A---------------------------B-------+
		//      |    |                                   |
		//     0+----------------> X <-----------------------
		//      0   p0.x                                 p1.x
		//
		// As wide lines are apparently a massive ballache in opengl to stick to any kind of raster
		// standard, this is actually implemented using rects for each line.
		// Assuming that wide lines are centered around {+0.5, +0.5} A.y (for example) would be
		//
		// Line1 goes from origin A (p0) to with size (size.x - thickness, thickness)
		// Line2 goes from origin B (p1.x - thickness, p0.y) with size (thickness, size.y -
		// thickness)
		// Line3 goes from origin C (p0.x + thickness, p1.y - thickness) with size (size.x -
		// thickness, thickness)
		// Line4 goes from origin D(p0.x, p0.y + thickness) with size (thickness, size.y -
		// thickness)
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
	void flush() override
	{
		this->flushBatch();
		// Cleanup any outstanding destroyed texture or framebuffer objects
		{
			std::lock_guard<std::mutex> lock(this->destroyed_texture_list_mutex);

			for (auto &id : this->destroyed_texture_list)
			{
				gl20::DeleteTextures(1, &id);
			}
			if (!this->destroyed_texture_list.empty())
			{
				glState.invalidateTextures();
			}
			this->destroyed_texture_list.clear();
		}
		{
			std::lock_guard<std::mutex> lock(this->destroyed_framebuffer_list_mutex);

			for (auto &id : this->destroyed_framebuffer_list)
			{
				gl20::DeleteFramebuffersEXT(1, &id);
			}
			if (!this->destroyed_framebuffer_list.empty())
			{
				glState.framebuffer = GLStateCache::UNKNOWN;
			}
			this->destroyed_framebuffer_list.clear();
		}
	}
	uint64_t takeDrawCallCount() override { return drawCallCount.exchange(0); }
	UString getName() override { return "OGL2.0 Renderer"; }
	sp<Surface> getDefaultSurface() override { return this->defaultSurface; }

	void bindProgram(sp<Program> p)
	{
		// Anything that is not another batched sprite ends the batch, which keeps the
		// painter's-algorithm draw order the tile views rely on.
		if (p != this->paletteBatchProgram)
		{
			this->flushBatch();
		}
		if (this->currentBoundProgram == p->prog)
			return;
		gl20::UseProgram(p->prog);
		this->currentBoundProgram = p->prog;
	}

	void flushBatch()
	{
		if (this->batchVertices.empty())
		{
			return;
		}
		// Swap out before binding: bindProgram() would otherwise recurse back in here.
		std::vector<BatchVertex> verts;
		verts.swap(this->batchVertices);

		bindProgram(this->paletteBatchProgram);
		this->paletteBatchProgram->setUniforms(this->currentSurface->size, this->batchFlipY);
		BindTexture pal(this->batchPalTex, 1);
		BindTexture tex(paletteAtlas.pageTexture(this->batchPage), 0);

		gl20::BindBuffer(gl20::ARRAY_BUFFER, this->batchVBO);
		gl20::BufferData(gl20::ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(BatchVertex)),
		                 verts.data(), gl20::STREAM_DRAW);

		const GLsizei stride = sizeof(BatchVertex);
		enableVertexAttrib(this->paletteBatchProgram->posLoc);
		gl20::VertexAttribPointer(this->paletteBatchProgram->posLoc, 2, gl20::FLOAT, gl20::FALSE_,
		                          stride, (const void *)offsetof(BatchVertex, x));
		enableVertexAttrib(this->paletteBatchProgram->texcoordLoc);
		gl20::VertexAttribPointer(this->paletteBatchProgram->texcoordLoc, 2, gl20::FLOAT,
		                          gl20::FALSE_, stride, (const void *)offsetof(BatchVertex, u));
		enableVertexAttrib(this->paletteBatchProgram->tintLoc);
		gl20::VertexAttribPointer(this->paletteBatchProgram->tintLoc, 4, gl20::UNSIGNED_BYTE,
		                          gl20::TRUE_, stride, (const void *)offsetof(BatchVertex, r));

		gl20::DrawArrays(gl20::TRIANGLES, 0, (GLsizei)verts.size());
		drawCallCount++;

		// No other program re-points the tint attribute, so leaving it enabled would let
		// later draws read the batch buffer past its end.
		disableVertexAttrib(this->paletteBatchProgram->tintLoc);
		// The single-sprite paths below feed GL from client memory, which only works
		// while no array buffer is bound.
		gl20::BindBuffer(gl20::ARRAY_BUFFER, 0);

		verts.clear();
		this->batchVertices.swap(verts);
	}

	// Returns false when the sprite cannot be batched and needs its own draw call.
	bool batchPaletteSprite(GLPaletteImage &img, Vec2<float> offset, Vec2<float> size, Colour tint,
	                        bool flipY)
	{
		if (img.atlasPage < 0 || !this->paletteBatchProgram->valid())
		{
			return false;
		}
		// Scaled draws would let nearest sampling reach past the sprite's own texels.
		if (size != img.size)
		{
			return false;
		}

		const GLuint palTexID =
		    static_cast<GLPalette *>(this->currentPalette->rendererPrivateData.get())->texID;
		if (this->batchPage != img.atlasPage || this->batchPalTex != palTexID ||
		    this->batchFlipY != flipY)
		{
			this->flushBatch();
			this->batchPage = img.atlasPage;
			this->batchPalTex = palTexID;
			this->batchFlipY = flipY;
		}

		constexpr float pageSize = (float)PaletteAtlas::PAGE_SIZE;
		const float u0 = (float)img.atlasPos.x / pageSize;
		const float v0 = (float)img.atlasPos.y / pageSize;
		const float u1 = ((float)img.atlasPos.x + size.x) / pageSize;
		const float v1 = ((float)img.atlasPos.y + size.y) / pageSize;
		const float x0 = offset.x;
		const float y0 = offset.y;
		const float x1 = offset.x + size.x;
		const float y1 = offset.y + size.y;

		const BatchVertex tl{x0, y0, u0, v0, tint.r, tint.g, tint.b, tint.a};
		const BatchVertex tr{x1, y0, u1, v0, tint.r, tint.g, tint.b, tint.a};
		const BatchVertex bl{x0, y1, u0, v1, tint.r, tint.g, tint.b, tint.a};
		const BatchVertex br{x1, y1, u1, v1, tint.r, tint.g, tint.b, tint.a};
		this->batchVertices.insert(this->batchVertices.end(), {tl, tr, bl, bl, tr, br});
		return true;
	}
	void drawRgb(GLRGBImage &img, Vec2<float> offset, Vec2<float> size, Scaler scaler,
	             Vec2<float> rotationCenter = {0, 0}, float rotationAngleRadians = 0,
	             Colour tint = {255, 255, 255, 255})
	{
		GLenum filter;
		Rect<float> pos(offset, offset + size);
		switch (scaler)
		{
			case Scaler::Linear:
				filter = gl20::LINEAR;
				break;
			case Scaler::Nearest:
				filter = gl20::NEAREST;
				break;
			default:
				LogError("Unknown scaler requested");
				filter = gl20::NEAREST;
				break;
		}
		bindProgram(rgbProgram);
		bool flipY = false;
		if (currentBoundFBO == 0)
			flipY = true;
		rgbProgram->setUniforms(this->currentSurface->size, flipY, tint);
		BindTexture t(img.texID);
		TexParam<gl20::TEXTURE_MAG_FILTER> mag(img.texID, filter);
		TexParam<gl20::TEXTURE_MIN_FILTER> min(img.texID, filter);
		Quad q(pos, Rect<float>{{0, 0}, {1, 1}}, rotationCenter, rotationAngleRadians);
		q.draw(rgbProgram->posLoc, rgbProgram->texcoordLoc);
	}
	void drawPalette(GLPaletteImage &img, Vec2<float> offset, Vec2<float> size,
	                 Colour tint = {255, 255, 255, 255})
	{
		bool flipY = false;
		if (currentBoundFBO == 0)
			flipY = true;
		if (batchPaletteSprite(img, offset, size, tint, flipY))
		{
			return;
		}
		bindProgram(paletteProgram);
		Rect<float> pos(offset, offset + size);
		paletteProgram->setUniforms(this->currentSurface->size, flipY, tint);
		// Bind the palette first so the sprite bind leaves unit 0 active: the palette is
		// the same for a whole screen, so this skips two glActiveTexture calls per sprite.
		const GLuint palTexID =
		    static_cast<GLPalette *>(this->currentPalette->rendererPrivateData.get())->texID;
		if (!glState.textureBound(1, gl20::TEXTURE_2D, palTexID))
		{
			BindTexture p(palTexID, 1);
		}
		BindTexture t(img.texID, 0);
		Quad q(pos, Rect<float>{{0, 0}, {1, 1}});
		q.draw(paletteProgram->posLoc, paletteProgram->texcoordLoc);
	}

	void drawSurface(FBOData &fbo, Vec2<float> offset, Vec2<float> size, Scaler scaler,
	                 Colour tint = {255, 255, 255, 255})
	{
		GLenum filter;
		Rect<float> pos(offset, offset + size);
		switch (scaler)
		{
			case Scaler::Linear:
				filter = gl20::LINEAR;
				break;
			case Scaler::Nearest:
				filter = gl20::NEAREST;
				break;
			default:
				LogError("Unknown scaler requested");
				filter = gl20::NEAREST;
				break;
		}
		bindProgram(rgbProgram);
		bool flipY = false;
		if (currentBoundFBO == 0)
			flipY = true;
		rgbProgram->setUniforms(this->currentSurface->size, flipY, tint);
		BindTexture t(fbo.tex);
		TexParam<gl20::TEXTURE_MAG_FILTER> mag(fbo.tex, filter);
		TexParam<gl20::TEXTURE_MIN_FILTER> min(fbo.tex, filter);
		Quad q(pos, Rect<float>{{0, 0}, {1, 1}});
		q.draw(rgbProgram->posLoc, rgbProgram->texcoordLoc);
	}

	void drawLine(Vec2<float> p0, Vec2<float> p1, Colour c, float thickness) override
	{
		bindProgram(colourProgram);
		bool flipY = false;
		if (currentBoundFBO == 0)
			flipY = true;
		colourProgram->setUniforms(this->currentSurface->size, flipY, c);
		Line l(p0, p1, thickness);
		l.draw(colourProgram->posLoc);
	}
	// These can be called from any thread - e.g. from the Image destructors
	void delete_texture_object(GLuint id)
	{
		// If we're already on the bound thread, just immediately destroy
		if (this->bound_thread == std::this_thread::get_id())
		{
			gl20::DeleteTextures(1, &id);
			glState.invalidateTextures();
			return;
		}
		// Otherwise add it to a list for future destruction
		{
			std::lock_guard<std::mutex> lock(this->destroyed_texture_list_mutex);
			this->destroyed_texture_list.push_back(id);
		}
	}
	void delete_framebuffer_object(GLuint id)
	{
		// If we're already on the bound thread, just immediately destroy
		if (this->bound_thread == std::this_thread::get_id())
		{
			gl20::DeleteFramebuffersEXT(1, &id);
			glState.framebuffer = GLStateCache::UNKNOWN;
			return;
		}
		// Otherwise add it to a list for future destruction
		{
			std::lock_guard<std::mutex> lock(this->destroyed_framebuffer_list_mutex);
			this->destroyed_framebuffer_list.push_back(id);
		}
	}
};

class OGL20RendererFactory : public OpenApoc::RendererFactory
{
	bool alreadyInitialised;
	bool functionLoadSuccess;

  public:
	OGL20RendererFactory() : alreadyInitialised(false), functionLoadSuccess(false) {}
	OpenApoc::Renderer *create() override
	{
		if (!alreadyInitialised)
		{
			alreadyInitialised = true;
			auto success = gl20::sys::LoadFunctions();
			if (!success)
			{
				LogInfo("failed to load GL implementation functions");
				return nullptr;
			}
			if (success.GetNumMissing())
			{
				LogInfo("GL implementation missing {0} functions", success.GetNumMissing());
				return nullptr;
			}
			if (!gl20::sys::IsVersionGEQ(2, 0))
			{
				LogInfo("GL version not at least 2.0, got {0}.{1}", gl20::sys::GetMajorVersion(),
				        gl20::sys::GetMinorVersion());
				return nullptr;
			}
			functionLoadSuccess = true;
		}
		if (functionLoadSuccess)
		{
			return new OGL20Renderer{};
		}
		return nullptr;
	}
};

FBOData::~FBOData()
{
	if (renderer_dead)
	{
		LogWarning("FBOData being destroyed after renderer");
		return;
	}
	if (tex)
		owner->delete_texture_object(tex);
	if (fbo)
		owner->delete_framebuffer_object(fbo);
}
GLRGBImage::~GLRGBImage()
{
	if (renderer_dead)
	{
		LogWarning("GLRGBImage being destroyed after renderer");
		return;
	}
	owner->delete_texture_object(this->texID);
}
GLPalette::~GLPalette()
{
	if (renderer_dead)
	{
		LogWarning("GLPalette being destroyed after renderer");
		return;
	}
	owner->delete_texture_object(this->texID);
}
static void flushRendererBatch(OGL20Renderer *r)
{
	if (r && !renderer_dead)
	{
		r->flushBatch();
	}
}

GLPaletteImage::~GLPaletteImage()
{
	if (this->atlasPage >= 0)
	{
		// Atlas space is owned by the page and is not reclaimed.
		return;
	}
	if (renderer_dead)
	{
		LogWarning("GLPaletteImage being destroyed after renderer");
		return;
	}
	owner->delete_texture_object(this->texID);
}

} // anonymous namespace

namespace OpenApoc
{
RendererFactory *getGL20RendererFactory() { return new OGL20RendererFactory(); }
} // namespace OpenApoc
