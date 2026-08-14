/*
 Copyright (c) 2013 yvt

 This file is part of OpenSpades.

 OpenSpades is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 OpenSpades is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with OpenSpades.  If not, see <http://www.gnu.org/licenses/>.

 */

#include <Imports/OpenGL.h>
#include <Imports/SDL.h>

#include "SDLGLDevice.h"
#include <Core/Debug.h>
#include <Core/Exception.h>
#include <Core/Settings.h>

using namespace spades::draw;

#ifndef __APPLE__
#define GLEW 1
#endif

#ifdef _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCDNAME__
#endif

DEFINE_SPADES_SETTING(r_ignoreGLErrors, "1");

static uint32_t vertCount = 0;
static uint32_t drawOps = 0;
// static Uint32 lastFrame = 0;

namespace spades {
	namespace gui {

#define CheckError()                                                                               \
	do {                                                                                           \
		GLenum err;                                                                                \
		if (!r_ignoreGLErrors) {                                                                   \
			err = glGetError();                                                                    \
			if (err != GL_NO_ERROR)                                                                \
				ReportError(err, __LINE__, __PRETTY_FUNCTION__);                                   \
		}                                                                                          \
	} while (0)

#define CheckErrorAlways()                                                                         \
	do {                                                                                           \
		GLenum err;                                                                                \
		err = glGetError();                                                                        \
		if (err != GL_NO_ERROR)                                                                    \
			ReportError(err, __LINE__, __PRETTY_FUNCTION__);                                       \
	} while (0)

// lm: The macro would not work on windows, the application simply fails to start if dependency's
// are missing.
//	unline ?mac?, runtime dependency's are all resolved at application start.
//	one would need a construction like OpenAL, where functions are resolved dynamically
//(GetProcAddress / dlsym)
// on GCC this was giving me warnings aswell...

#if defined(_MSC_VER) || defined(__GNUC__)
#define CheckExistence(func)
#else
#define CheckExistence(func)                                                                       \
	do {                                                                                           \
		if (!func) {                                                                               \
			ReportMissingFunc(#func);                                                              \
		}                                                                                          \
	} while (0)
#endif

		static std::string ErrorToString(GLenum err) {
			switch (err) {
				case GL_NO_ERROR: return "No Error";
				case GL_INVALID_ENUM: return "Invalid Enum";
				case GL_INVALID_VALUE: return "Invalid Value";
				case GL_INVALID_OPERATION: return "Invalid Operation";
				case GL_INVALID_FRAMEBUFFER_OPERATION: return "Invalid Framebuffer Operation";
				case GL_OUT_OF_MEMORY: return "Out of Memory";
				default: {
					char buf[256];
					sprintf(buf, "0x%08x", (unsigned int)err);
					return buf;
				}
			}
		}

		static void ReportError(GLenum err, int line, const char *func) {
			std::string msg;
			msg = ErrorToString(err);
			while ((err = glGetError()) != GL_NO_ERROR) {
				msg += ", ";
				msg += ErrorToString(err);
			}
			if (r_ignoreGLErrors) {
				SPRaise("GL error %s in %s at %s:%d\n\n"
				        "WARNING: r_ignoreGLErrors is enabled. "
				        "Information contained in this message is "
				        "inaccurate and non-informative.",
				        msg.c_str(), func, __FILE__, line);
			} else {
				SPRaise("GL error %s in %s at %s:%d", msg.c_str(), func, __FILE__, line);
			}
		}
#ifdef GLEW
		static void ReportMissingFunc(const char *func) { SPRaise("GL function %s missing", func); }
#endif

		SDLGLDevice::SDLGLDevice(SDL_Window *s) : window(s) {
			enabledStates.fill(-1);
			SPLog("starting SDLGLDevice");

			SDL_GetWindowSize(window, &w, &h);
			context = SDL_GL_CreateContext(s);
			if (!context) {
				const char *err = SDL_GetError();
				SPLog("Failed to create GL context!: %s", err);
				SPRaise("Failed to create GL context: %s", err);
			}

			SDL_GL_MakeCurrent(window, context);

#ifndef __APPLE__
			GLenum err = glewInit();
			if (GLEW_OK != err) {
				SPRaise("GLEW error: %s", glewGetErrorString(err));
			}
#endif
			SPLog("GLEW initialized");

			GLint maxTextureUnits = 0;
			glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
			if (maxTextureUnits > 0)
				textureBindings.resize(static_cast<size_t>(maxTextureUnits));

			GLint maxVertexAttribs = 0;
			glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
			if (maxVertexAttribs > 0)
				vertexAttribArrayStates.resize(static_cast<size_t>(maxVertexAttribs), -1);

			SPLog("--- OpenGL Renderer Info ---");
			const char *ret;
			if ((ret = (const char *)glGetString(GL_VENDOR)) != NULL) {
				SPLog("Vendor: %s", ret);
			}
			if ((ret = (const char *)glGetString(GL_RENDERER)) != NULL) {
				SPLog("Name: %s", ret);
			}
			if ((ret = (const char *)glGetString(GL_VERSION)) != NULL) {
				SPLog("Version: %s", ret);
			}
			if ((ret = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION)) != NULL) {
				SPLog("Shading Language Version: %s", ret);
			}
			SPLog("--- Extensions ---");
#ifdef GLEW
			// function ptr provided by GLEW
			if (glGetStringi)
#else
			// normal function on macOS (always available)
			if (true)
#endif
			{
				GLint cnt = 0;
				glGetIntegerv(GL_NUM_EXTENSIONS, &cnt);
				if (cnt <= 0)
					goto retrvFail;
				for (GLint i = 0; i < cnt; i++) {
					ret = (const char *)glGetStringi(GL_EXTENSIONS, i);
					SPLog("%s", ret);
				}
			} else {
			retrvFail:
				if ((ret = (const char *)glGetString(GL_EXTENSIONS)) != NULL) {
					std::vector<std::string> strs = Split(ret, " ");
					for (size_t i = 0; i < strs.size(); i++)
						SPLog("%s", strs[i].c_str());
				} else {
					SPLog("no information");
				}
			}
			SPLog("------------------");

			CheckExistence(glFrontFace);
			glFrontFace(GL_CW);
			frontFace = CW;
			frontFaceValid = true;

			if (r_ignoreGLErrors) {
				SPLog("NOTICE: r_ignoreGLErrors is enabled. "
				      "OpenGL error detection might not work correctly.");
			} else {
				SPLog("NOTICE: r_ignoreGLErrors is disabled. "
				      "OpenGL error is checked for every GL call, but "
				      "performance may be reduced.");
			}

			// clear error state
			while (glGetError() != GL_NO_ERROR)
				;
		}

		SDLGLDevice::~SDLGLDevice() { SDL_GL_DeleteContext(context); }

		void SDLGLDevice::DepthRange(Float near, Float far) {
			CheckExistence(glDepthRange);
			glDepthRange(near, far);
			CheckError();
		}
		void SDLGLDevice::Viewport(Integer x, Integer y, Sizei width, Sizei height) {
			if (viewportValid && viewportX == x && viewportY == y && viewportWidth == width &&
			    viewportHeight == height)
				return;

			CheckExistence(glViewport);
			glViewport(x, y, width, height);
			CheckError();
			viewportX = x;
			viewportY = y;
			viewportWidth = width;
			viewportHeight = height;
			viewportValid = true;
		}

		void SDLGLDevice::ClearDepth(float v) {
			CheckExistence(glClearDepth);
			glClearDepth(v);
			CheckError();
		}
		void SDLGLDevice::ClearColor(float r, float g, float b, float a) {
			CheckExistence(glClearColor);
			glClearColor(r, g, b, a);
			CheckError();
		}
		void SDLGLDevice::Clear(Enum bits) {
			GLbitfield v = 0;
			if (bits & ColorBufferBit)
				v |= GL_COLOR_BUFFER_BIT;
			if (bits & DepthBufferBit)
				v |= GL_DEPTH_BUFFER_BIT;
			if (bits & StencilBufferBit)
				v |= GL_STENCIL_BUFFER_BIT;
			CheckExistence(glClear);
			glClear(v);
			CheckError();
		}

		void SDLGLDevice::Swap() {
			// glFinish();
			CheckErrorAlways();
			SDL_GL_SwapWindow(window);
#if 0
			Uint32 t = SDL_GetTicks();
			if(lastFrame == 0) t = lastFrame - 30;
			double dur = (double)(t - lastFrame) / 1000.;
			lastFrame = t;

			printf("FPS:%.02f, Vertices: %d (%.02f/sec), Drawcalls: %d (%.02f/sec)\n", 1./dur,
				   vertCount, vertCount / dur,
				   drawOps, drawOps / dur);

			//printf("%.02f,%.02f,%.02f\n", 1./dur, vertCount / dur, drawOps / dur);
#endif
			vertCount = 0;
			drawOps = 0;
		}

		void SDLGLDevice::Finish() {
			CheckExistence(glFinish);
			glFinish();
			CheckError();
		}
		void SDLGLDevice::Flush() {
			CheckExistence(glFlush);
			glFlush();
			CheckError();
		}

		void SDLGLDevice::DepthMask(bool b) {
			if (depthMaskValid && depthMask == b)
				return;

			CheckExistence(glDepthMask);
			glDepthMask(b ? GL_TRUE : GL_FALSE);
			CheckError();
			depthMask = b;
			depthMaskValid = true;
		}

		void SDLGLDevice::ColorMask(bool r, bool g, bool b, bool a) {
			unsigned int mask = (r ? 1U : 0U) | (g ? 2U : 0U) | (b ? 4U : 0U) | (a ? 8U : 0U);
			if (colorMaskValid && colorMask == mask)
				return;

			CheckExistence(glColorMask);
			glColorMask(r ? GL_TRUE : GL_FALSE, g ? GL_TRUE : GL_FALSE, b ? GL_TRUE : GL_FALSE,
			            a ? GL_TRUE : GL_FALSE);
			CheckError();
			colorMask = mask;
			colorMaskValid = true;
		}

		void SDLGLDevice::FrontFace(Enum val) {
			GLenum mode;
			switch (val) {
				case draw::IGLDevice::CW: mode = GL_CW; break;
				case draw::IGLDevice::CCW: mode = GL_CCW; break;
				default: SPInvalidEnum("val", val);
			}
			if (frontFaceValid && frontFace == val)
				return;

			CheckExistence(glFrontFace);
			glFrontFace(mode);
			CheckError();
			frontFace = val;
			frontFaceValid = true;
		}

		void SDLGLDevice::Enable(spades::draw::IGLDevice::Enum state, bool b) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum type;
			int stateIndex = -1;
			switch (state) {
				case DepthTest:
					type = GL_DEPTH_TEST;
					stateIndex = 0;
					break;
				case CullFace:
					type = GL_CULL_FACE;
					stateIndex = 1;
					break;
				case Blend:
					type = GL_BLEND;
					stateIndex = 2;
					break;
				case Texture2D:
					// This legacy enable is specific to the active texture unit.
					type = GL_TEXTURE_2D;
					break;
				case Multisample:
					type = GL_MULTISAMPLE;
					stateIndex = 3;
					break;
				case FramebufferSRGB:
					type = GL_FRAMEBUFFER_SRGB;
					stateIndex = 4;
					break;
				default: SPInvalidEnum("state", state);
			}
			if (stateIndex >= 0 && enabledStates[stateIndex] == (b ? 1 : 0))
				return;

			if (b)
				glEnable(type);
			else
				glDisable(type);
			CheckError();
			if (stateIndex >= 0)
				enabledStates[stateIndex] = b ? 1 : 0;
		}

		IGLDevice::Integer SDLGLDevice::GetInteger(Enum type) {
			SPADES_MARK_FUNCTION();
			GLint v;
			switch (type) {
				case draw::IGLDevice::FramebufferBinding:
					glGetIntegerv(GL_FRAMEBUFFER_BINDING, &v);
					break;
				default: SPInvalidEnum("type", type);
			}
			CheckError();
			return v;
		}

		const char *SDLGLDevice::GetString(spades::draw::IGLDevice::Enum type) {
			SPADES_MARK_FUNCTION();
			switch (type) {
				case Vendor: return (const char *)glGetString(GL_VENDOR);
				case Renderer: return (const char *)glGetString(GL_RENDERER);
				case Version: return (const char *)glGetString(GL_VERSION);
				case ShadingLanguageVersion:
					return (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
				default: SPInvalidEnum("type", type);
			}
		}
		const char *SDLGLDevice::GetIndexedString(spades::draw::IGLDevice::Enum type,
		                                          UInteger index) {
			SPADES_MARK_FUNCTION();
			switch (type) {
				case draw::IGLDevice::Extensions:
					return (const char *)glGetStringi(GL_EXTENSIONS, index);
				default: SPInvalidEnum("type", type);
			}
		}

		GLenum SDLGLDevice::parseBlendEquation(spades::draw::IGLDevice::Enum v) {
			SPADES_MARK_FUNCTION_DEBUG();
			switch (v) {
				case Add: return GL_FUNC_ADD;
				case Subtract: return GL_FUNC_SUBTRACT;
				case ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
				case MinOp: return GL_MIN;
				case MaxOp: return GL_MAX;
				default: SPInvalidEnum("v", v);
			}
		}

		GLenum SDLGLDevice::parseBlendFunction(spades::draw::IGLDevice::Enum v) {
			SPADES_MARK_FUNCTION_DEBUG();
			switch (v) {
				case Zero: return GL_ZERO;
				case One: return GL_ONE;
				case SrcColor: return GL_SRC_COLOR;
				case DestColor: return GL_DST_COLOR;
				case OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
				case OneMinusDestColor: return GL_ONE_MINUS_DST_COLOR;
				case SrcAlpha: return GL_SRC_ALPHA;
				case DestAlpha: return GL_DST_ALPHA;
				case OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
				case OneMinusDestAlpha: return GL_ONE_MINUS_DST_ALPHA;
				case ConstantColor: return GL_CONSTANT_COLOR;
				case ConstantAlpha: return GL_CONSTANT_ALPHA;
				case OneMinusConstantColor: return GL_ONE_MINUS_CONSTANT_COLOR;
				case OneMinusConstantAlpha: return GL_ONE_MINUS_CONSTANT_ALPHA;
				default: SPInvalidEnum("v", v);
			}
		}

		void SDLGLDevice::BlendEquation(spades::draw::IGLDevice::Enum mode) {
			GLenum glMode = parseBlendEquation(mode);
			if (blendEquationValid && blendEquationRgb == mode && blendEquationAlpha == mode)
				return;

			CheckExistence(glBlendEquation);
			glBlendEquation(glMode);
			CheckError();
			blendEquationRgb = mode;
			blendEquationAlpha = mode;
			blendEquationValid = true;
		}

		void SDLGLDevice::BlendEquation(spades::draw::IGLDevice::Enum rgb,
		                                spades::draw::IGLDevice::Enum alpha) {
			GLenum glRgb = parseBlendEquation(rgb);
			GLenum glAlpha = parseBlendEquation(alpha);
			if (blendEquationValid && blendEquationRgb == rgb && blendEquationAlpha == alpha)
				return;

			CheckExistence(glBlendEquationSeparate);
			glBlendEquationSeparate(glRgb, glAlpha);
			CheckError();
			blendEquationRgb = rgb;
			blendEquationAlpha = alpha;
			blendEquationValid = true;
		}
		void SDLGLDevice::BlendFunc(Enum src, Enum dest) {
			GLenum glSource = parseBlendFunction(src);
			GLenum glDestination = parseBlendFunction(dest);
			if (blendFunctionValid && blendSourceRgb == src && blendDestinationRgb == dest &&
			    blendSourceAlpha == src && blendDestinationAlpha == dest)
				return;

			CheckExistence(glBlendFunc);
			glBlendFunc(glSource, glDestination);
			CheckError();
			blendSourceRgb = src;
			blendDestinationRgb = dest;
			blendSourceAlpha = src;
			blendDestinationAlpha = dest;
			blendFunctionValid = true;
		}
		void SDLGLDevice::BlendFunc(Enum srcRgb, Enum destRgb, Enum srcAlpha, Enum destAlpha) {
			GLenum glSourceRgb = parseBlendFunction(srcRgb);
			GLenum glDestinationRgb = parseBlendFunction(destRgb);
			GLenum glSourceAlpha = parseBlendFunction(srcAlpha);
			GLenum glDestinationAlpha = parseBlendFunction(destAlpha);
			if (blendFunctionValid && blendSourceRgb == srcRgb && blendDestinationRgb == destRgb &&
			    blendSourceAlpha == srcAlpha && blendDestinationAlpha == destAlpha)
				return;

			CheckExistence(glBlendFuncSeparate);
			glBlendFuncSeparate(glSourceRgb, glDestinationRgb, glSourceAlpha, glDestinationAlpha);
			CheckError();
			blendSourceRgb = srcRgb;
			blendDestinationRgb = destRgb;
			blendSourceAlpha = srcAlpha;
			blendDestinationAlpha = destAlpha;
			blendFunctionValid = true;
		}
		void SDLGLDevice::BlendColor(Float r, Float g, Float b, Float a) {
			CheckExistence(glBlendColor);
			glBlendColor(r, g, b, a);
			CheckError();
		}
		void SDLGLDevice::LineWidth(Float w) {
			CheckExistence(glLineWidth);
			glLineWidth(w);
			CheckError();
		}
		void SDLGLDevice::DepthFunc(Enum func) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum glFunction;
			switch (func) {
				case Never: glFunction = GL_NEVER; break;
				case Always: glFunction = GL_ALWAYS; break;
				case Less: glFunction = GL_LESS; break;
				case LessOrEqual: glFunction = GL_LEQUAL; break;
				case Equal: glFunction = GL_EQUAL; break;
				case Greater: glFunction = GL_GREATER; break;
				case GreaterOrEqual: glFunction = GL_GEQUAL; break;
				case NotEqual: glFunction = GL_NOTEQUAL; break;
				default: SPInvalidEnum("func", func);
			}
			if (depthFunctionValid && depthFunction == func)
				return;

			CheckExistence(glDepthFunc);
			glDepthFunc(glFunction);
			CheckError();
			depthFunction = func;
			depthFunctionValid = true;
		}

		IGLDevice::UInteger SDLGLDevice::GenBuffer() {
			SPADES_MARK_FUNCTION_DEBUG();
			GLuint i = 0;
#if GLEW
			if (glGenBuffers)
				glGenBuffers(1, &i);
			else if (glGenBuffersARB)
				glGenBuffersARB(1, &i);
			else
				ReportMissingFunc("glGenBuffers");
#else
			CheckExistence(glGenBuffers);
			glGenBuffers(1, &i);
#endif
			CheckError();
			return i;
		}

		void SDLGLDevice::DeleteBuffer(UInteger i) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLuint v = (GLuint)i;
#if GLEW
			if (glDeleteBuffers)
				glDeleteBuffers(1, &v);
			else if (glDeleteBuffersARB)
				glDeleteBuffersARB(1, &v);
			else
				ReportMissingFunc("glDeleteBuffers");
#else
			CheckExistence(glDeleteBuffers);
			glDeleteBuffers(1, &v);
#endif
			CheckError();
			for (CachedUInteger &binding : bufferBindings) {
				if (binding.valid && binding.value == i)
					binding.value = 0;
			}
		}

		void *SDLGLDevice::MapBuffer(Enum target, Enum access) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum acc;
			switch (access) {
				case draw::IGLDevice::ReadOnly: acc = GL_READ_ONLY; break;
				case draw::IGLDevice::WriteOnly: acc = GL_WRITE_ONLY; break;
				case draw::IGLDevice::ReadWrite: acc = GL_READ_WRITE; break;
				default: SPInvalidEnum("access", access);
			}
			void *ret = nullptr;
#if GLEW
			if (glMapBuffer)
				ret = glMapBuffer(parseBufferTarget(target), acc);
			else if (glMapBufferARB)
				ret = glMapBufferARB(parseBufferTarget(target), acc);
			else
				ReportMissingFunc("glMapBuffer");
#else
			CheckExistence(glMapBuffer);
			ret = glMapBuffer(parseBufferTarget(target), acc);
#endif
			CheckError();
			return ret;
		}

		void SDLGLDevice::UnmapBuffer(Enum target) {
#if GLEW
			if (glUnmapBuffer)
				glUnmapBuffer(parseBufferTarget(target));
			else if (glUnmapBufferARB)
				glUnmapBufferARB(parseBufferTarget(target));
			else
				ReportMissingFunc("glUnmapBuffer");
#else
			CheckExistence(glUnmapBuffer);
			glUnmapBuffer(parseBufferTarget(target));
#endif
			CheckError();
		}

		GLenum SDLGLDevice::parseBufferTarget(spades::draw::IGLDevice::Enum v) {
			SPADES_MARK_FUNCTION_DEBUG();
			switch (v) {
				case ArrayBuffer: return GL_ARRAY_BUFFER;
				case ElementArrayBuffer: return GL_ELEMENT_ARRAY_BUFFER;
				case PixelPackBuffer: return GL_PIXEL_PACK_BUFFER;
				case PixelUnpackBuffer: return GL_PIXEL_UNPACK_BUFFER;
				default: SPInvalidEnum("v", v);
			}
		}

		void SDLGLDevice::BindBuffer(Enum target, UInteger i) {
			GLenum glTarget = parseBufferTarget(target);
			int bindingIndex = 0;
			switch (target) {
				case ArrayBuffer: bindingIndex = 0; break;
				case ElementArrayBuffer: bindingIndex = 1; break;
				case PixelPackBuffer: bindingIndex = 2; break;
				case PixelUnpackBuffer: bindingIndex = 3; break;
				default: SPInvalidEnum("target", target);
			}
			CachedUInteger &binding = bufferBindings[bindingIndex];
			if (binding.valid && binding.value == i)
				return;

#if GLEW
			if (glBindBuffer)
				glBindBuffer(glTarget, (GLuint)i);
			else if (glBindBufferARB)
				glBindBufferARB(glTarget, (GLuint)i);
			else
				ReportMissingFunc("glBindBuffer");
#else
			CheckExistence(glBindBuffer);
			glBindBuffer(glTarget, (GLuint)i);
#endif
			CheckError();
			binding.value = i;
			binding.valid = true;
		}

		void SDLGLDevice::BufferData(Enum target, Sizei size, const void *data, Enum usage) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum usageVal;
			switch (usage) {
				case StaticDraw: usageVal = GL_STATIC_DRAW; break;
				case StreamDraw: usageVal = GL_STREAM_DRAW; break;
				case DynamicDraw: usageVal = GL_DYNAMIC_DRAW; break;
				default: SPInvalidEnum("usage", usage);
			}
#if GLEW
			if (glBufferData)
				glBufferData(parseBufferTarget(target), (GLsizeiptr)size, data, usageVal);
			else if (glBufferDataARB)
				glBufferDataARB(parseBufferTarget(target), (GLsizeiptr)size, data, usageVal);
			else
				ReportMissingFunc("glBufferData");
#else
			CheckExistence(glBufferData);
			glBufferData(parseBufferTarget(target), (GLsizeiptr)size, data, usageVal);
#endif
			CheckError();
		}
		void SDLGLDevice::BufferSubData(Enum target, Sizei offset, Sizei size, const void *data) {
#if GLEW
			if (glBufferSubData)
				glBufferSubData(parseBufferTarget(target), offset, size, data);
			else if (glBufferSubDataARB)
				glBufferSubDataARB(parseBufferTarget(target), offset, size, data);
			else
				ReportMissingFunc("glBufferSubData");
#else
			CheckExistence(glBufferSubData);
			glBufferSubData(parseBufferTarget(target), offset, size, data);
#endif
			CheckError();
		}

		IGLDevice::UInteger SDLGLDevice::GenQuery() {
			SPADES_MARK_FUNCTION_DEBUG();
			GLuint val = 0;
#if GLEW
			if (glGenQueries)
				glGenQueries(1, &val);
			else if (glGenQueriesARB)
				glGenQueriesARB(1, &val);
			else
				ReportMissingFunc("glGenQueries");
#else
			CheckExistence(glGenQueries);
			glGenQueries(1, &val);
#endif
			CheckError();
			return val;
		}

		void SDLGLDevice::DeleteQuery(UInteger query) {
			SPADES_MARK_FUNCTION_DEBUG();
#if GLEW
			if (glDeleteQueries)
				glDeleteQueries(1, &query);
			else if (glDeleteQueriesARB)
				glDeleteQueriesARB(1, &query);
			else
				ReportMissingFunc("glDeleteQueries");
#else
			CheckExistence(glDeleteQueries);
			glDeleteQueries(1, &query);
#endif
			CheckError();
		}

		static GLenum parseQueryTarget(IGLDevice::Enum target) {
			SPADES_MARK_FUNCTION_DEBUG();
			switch (target) {
				case IGLDevice::SamplesPassed: return GL_SAMPLES_PASSED;
				case IGLDevice::AnySamplesPassed: return GL_ANY_SAMPLES_PASSED;
				case IGLDevice::TimeElapsed: return GL_TIME_ELAPSED;
				default: SPInvalidEnum("target", target);
			}
		}

		void SDLGLDevice::BeginQuery(Enum target, UInteger query) {
			SPADES_MARK_FUNCTION_DEBUG();
#if GLEW
			if (glBeginQuery)
				glBeginQuery(parseQueryTarget(target), query);
			else if (glBeginQueryARB)
				glBeginQueryARB(parseQueryTarget(target), query);
			else
				ReportMissingFunc("glBeginQuery");
#else
			CheckExistence(glBeginQuery);
			glBeginQuery(parseQueryTarget(target), query);
#endif
			CheckError();
		}

		void SDLGLDevice::EndQuery(Enum target) {
			SPADES_MARK_FUNCTION_DEBUG();
#if GLEW
			if (glEndQuery)
				glEndQuery(parseQueryTarget(target));
			else if (glEndQueryARB)
				glEndQueryARB(parseQueryTarget(target));
			else
				ReportMissingFunc("glBeginQuery");
#else
			CheckExistence(glEndQuery);
			glEndQuery(parseQueryTarget(target));
#endif
			CheckError();
		}

		IGLDevice::UInteger SDLGLDevice::GetQueryObjectUInteger(UInteger query, Enum pname) {
			GLuint val = 0;
			SPADES_MARK_FUNCTION_DEBUG();

#if GLEW
			if (glGetQueryObjectuiv) {
				switch (pname) {
					case draw::IGLDevice::QueryResult:
						glGetQueryObjectuiv(query, GL_QUERY_RESULT, &val);
						break;
					case draw::IGLDevice::QueryResultAvailable:
						glGetQueryObjectuiv(query, GL_QUERY_RESULT_AVAILABLE, &val);
						break;
					default: SPInvalidEnum("pname", pname);
				}
			} else if (glGetQueryObjectuivARB) {
				switch (pname) {
					case draw::IGLDevice::QueryResult:
						glGetQueryObjectuivARB(query, GL_QUERY_RESULT, &val);
						break;
					case draw::IGLDevice::QueryResultAvailable:
						glGetQueryObjectuivARB(query, GL_QUERY_RESULT_AVAILABLE, &val);
						break;
					default: SPInvalidEnum("pname", pname);
				}
			} else {
				ReportMissingFunc("glGetQueryObjectuiv");
			}
#else
			CheckExistence(glGetQueryObjectuiv);
			switch (pname) {
				case draw::IGLDevice::QueryResult:
					glGetQueryObjectuiv(query, GL_QUERY_RESULT, &val);
					break;
				case draw::IGLDevice::QueryResultAvailable:
					glGetQueryObjectuiv(query, GL_QUERY_RESULT_AVAILABLE, &val);
					break;
				default: SPInvalidEnum("pname", pname);
			}
#endif
			CheckError();
			return val;
		}

		IGLDevice::UInteger64 SDLGLDevice::GetQueryObjectUInteger64(UInteger query, Enum pname) {
			GLuint64 val = 0;
			SPADES_MARK_FUNCTION_DEBUG();

#if GLEW
			if (glGetQueryObjectui64v) {
				switch (pname) {
					case draw::IGLDevice::QueryResult:
						glGetQueryObjectui64v(query, GL_QUERY_RESULT, &val);
						break;
					default: SPInvalidEnum("pname", pname);
				}
			} else if (glGetQueryObjectui64vEXT) {
				switch (pname) {
					case draw::IGLDevice::QueryResult:
						glGetQueryObjectui64vEXT(query, GL_QUERY_RESULT, &val);
						break;
					default: SPInvalidEnum("pname", pname);
				}
			} else {
				ReportMissingFunc("glGetQueryObjectui64v");
			}
#else
			CheckExistence(glGetQueryObjectui64v);
			switch (pname) {
				case draw::IGLDevice::QueryResult:
					glGetQueryObjectui64v(query, GL_QUERY_RESULT, &val);
					break;
				default: SPInvalidEnum("pname", pname);
			}
#endif
			CheckError();
			return val;
		}

		void SDLGLDevice::BeginConditionalRender(UInteger query, Enum mode) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum md;
			switch (mode) {
				case draw::IGLDevice::QueryWait: md = GL_QUERY_WAIT; break;
				case draw::IGLDevice::QueryNoWait: md = GL_QUERY_NO_WAIT; break;
				case draw::IGLDevice::QueryByRegionWait: md = GL_QUERY_BY_REGION_WAIT; break;
				case draw::IGLDevice::QueryByRegionNoWait: md = GL_QUERY_BY_REGION_NO_WAIT; break;
				default: SPInvalidEnum("mode", mode);
			}

#if GLEW
			if (glBeginConditionalRender)
				glBeginConditionalRender(query, md);
			else if (glBeginConditionalRenderNV)
				glBeginConditionalRenderNV(query, md);
			else
				ReportMissingFunc("glBeginConditionalRender");
#else
			CheckExistence(glBeginConditionalRender);
			glBeginConditionalRender(query, md);
#endif
			CheckError();
		}

		void SDLGLDevice::EndConditionalRender() {
			SPADES_MARK_FUNCTION_DEBUG();
#if GLEW
			if (glEndConditionalRender)
				glEndConditionalRender();
			else if (glEndConditionalRenderNV)
				glEndConditionalRenderNV();
			else
				ReportMissingFunc("glEndConditionalRender");
#else
			CheckExistence(glEndConditionalRender);
			glEndConditionalRender();
#endif
			CheckError();
		}

		IGLDevice::UInteger SDLGLDevice::GenTexture() {
			GLuint i;
			CheckExistence(glGenTextures);
			glGenTextures(1, &i);
			return i;
		}

		void SDLGLDevice::DeleteTexture(UInteger i) {
			GLuint v = (GLuint)i;
			CheckExistence(glDeleteTextures);
			glDeleteTextures(1, &v);
			CheckError();
			textureParameters.erase(i);
			for (auto &stageBindings : textureBindings) {
				for (CachedUInteger &binding : stageBindings) {
					if (binding.valid && binding.value == i)
						binding.value = 0;
				}
			}
		}

		GLenum SDLGLDevice::parseTextureTarget(Enum v) {
			SPADES_MARK_FUNCTION_DEBUG();
			switch (v) {
				case Texture2D: return GL_TEXTURE_2D;
				case Texture3D: return GL_TEXTURE_3D;
				case Texture2DArray: return GL_TEXTURE_2D_ARRAY;
				default: SPInvalidEnum("v", v);
			}
		}

		void SDLGLDevice::ActiveTexture(UInteger stage) {
			if (activeTextureStageValid && activeTextureStage == stage)
				return;

#if GLEW
			if (glActiveTexture)
				glActiveTexture(GL_TEXTURE0 + stage);
			else if (glActiveTextureARB)
				glActiveTextureARB(GL_TEXTURE0 + stage);
			else
				ReportMissingFunc("glActiveTexture");
#else
			CheckExistence(glActiveTexture);
			glActiveTexture(GL_TEXTURE0 + stage);
#endif
			CheckError();
			if (stage < textureBindings.size()) {
				activeTextureStage = stage;
				activeTextureStageValid = true;
			} else {
				activeTextureStageValid = false;
			}
		}

		void SDLGLDevice::BindTexture(Enum target, UInteger tex) {
			GLenum glTarget = parseTextureTarget(target);
			int bindingIndex = 0;
			switch (target) {
				case Texture2D: bindingIndex = 0; break;
				case Texture3D: bindingIndex = 1; break;
				case Texture2DArray: bindingIndex = 2; break;
				default: SPInvalidEnum("target", target);
			}

			CachedUInteger *binding = nullptr;
			if (activeTextureStageValid && activeTextureStage < textureBindings.size()) {
				binding = &textureBindings[activeTextureStage][bindingIndex];
				if (binding->valid && binding->value == tex)
					return;
			}

			CheckExistence(glBindTexture);
			glBindTexture(glTarget, tex);
			CheckError();
			if (binding) {
				binding->value = tex;
				binding->valid = true;
			}
		}

		GLenum SDLGLDevice::parseTextureInternalFormat(Enum v) {
			SPADES_MARK_FUNCTION_DEBUG();
			switch (v) {
				case 1:
				case 2:
				case 3:
				case 4: return (int)v;
				case Red: return GL_RED;
				case RG: return GL_RG;
				case RGB: return GL_RGB;
				case RGBA: return GL_RGBA;
				case DepthComponent: return GL_DEPTH_COMPONENT;
				case DepthComponent24: return GL_DEPTH_COMPONENT24;
				case StencilIndex: return GL_STENCIL_INDEX;

				case RGB10A2: return GL_RGB10_A2;
				case RGB16F: return GL_RGB16F;
				case RGBA16F: return GL_RGBA16F;
				case R16F: return GL_R16F;
				case RGB5: return GL_RGB5;
				case RGB5A1: return GL_RGB5_A1;
				case RGB8: return GL_RGB8;
				case RGBA8: return GL_RGBA8;
				case SRGB8: return GL_SRGB8;
				case SRGB8Alpha: return GL_SRGB8_ALPHA8;
				default: SPInvalidEnum("v", v);
			}
		}

		GLenum SDLGLDevice::parseTextureFormat(Enum v) {
			SPADES_MARK_FUNCTION_DEBUG();
			switch (v) {
				case Red: return GL_RED;
				case RG: return GL_RG;
				case RGB: return GL_RGB;
				case RGBA: return GL_RGBA;
				case BGRA: return GL_BGRA;
				case DepthComponent: return GL_DEPTH_COMPONENT;
				case StencilIndex: return GL_STENCIL_INDEX;
				default: SPInvalidEnum("v", v);
			}
		}

		GLenum SDLGLDevice::parseType(Enum v) {
			SPADES_MARK_FUNCTION_DEBUG();
			switch (v) {
				case Int: return GL_INT;
				case UnsignedInt: return GL_UNSIGNED_INT;
				case Short: return GL_SHORT;
				case UnsignedShort: return GL_UNSIGNED_SHORT;
				case Byte: return GL_BYTE;
				case UnsignedByte: return GL_UNSIGNED_BYTE;
				case FloatType: return GL_FLOAT;
				case UnsignedShort5551: return GL_UNSIGNED_SHORT_5_5_5_1;
				case UnsignedShort1555Rev: return GL_UNSIGNED_SHORT_1_5_5_5_REV;
				case UnsignedInt2101010Rev: return GL_UNSIGNED_INT_2_10_10_10_REV;
				default: SPInvalidEnum("v", v);
			}
		}

		void SDLGLDevice::TexImage2D(Enum target, Integer level, Enum intFmt, Sizei width,
		                             Sizei height, Integer border, Enum format, Enum type,
		                             const void *data) {
			CheckExistence(glTexImage2D);
			glTexImage2D(parseTextureTarget(target), level, parseTextureInternalFormat(intFmt),
			             width, height, border, parseTextureFormat(format), parseType(type), data);
			CheckErrorAlways();
		}

		void SDLGLDevice::TexImage3D(Enum target, Integer level, Enum intFmt, Sizei width,
		                             Sizei height, Sizei depth, Integer border, Enum format,
		                             Enum type, const void *data) {
#if GLEW
			if (glTexImage3D)
				glTexImage3D(parseTextureTarget(target), level, parseTextureInternalFormat(intFmt),
				             width, height, depth, border, parseTextureFormat(format),
				             parseType(type), data);
			else if (glTexImage3DEXT)
				glTexImage3DEXT(parseTextureTarget(target), level,
				                parseTextureInternalFormat(intFmt), width, height, depth, border,
				                parseTextureFormat(format), parseType(type), data);
			else
				ReportMissingFunc("glTexImage3D");
#else
			CheckExistence(glTexImage3D);
			glTexImage3D(parseTextureTarget(target), level, parseTextureInternalFormat(intFmt),
			             width, height, depth, border, parseTextureFormat(format), parseType(type),
			             data);
#endif
			CheckErrorAlways();
		}

		void SDLGLDevice::TexSubImage2D(Enum target, Integer level, Integer x, Integer y,
		                                Sizei width, Sizei height, Enum format, Enum type,
		                                const void *data) {
			CheckExistence(glTexSubImage2D);
			glTexSubImage2D(parseTextureTarget(target), level, x, y, width, height,
			                parseTextureFormat(format), parseType(type), data);
			CheckError();
		}

		void SDLGLDevice::TexSubImage3D(Enum target, Integer level, Integer x, Integer y, Integer z,
		                                Sizei width, Sizei height, Sizei depth, Enum format,
		                                Enum type, const void *data) {
#if GLEW
			if (glTexSubImage3D)
				glTexSubImage3D(parseTextureTarget(target), level, x, y, z, width, height, depth,
				                parseTextureFormat(format), parseType(type), data);
			else if (glTexSubImage3DEXT)
				glTexSubImage3DEXT(parseTextureTarget(target), level, x, y, z, width, height, depth,
				                   parseTextureFormat(format), parseType(type), data);
			else
				ReportMissingFunc("glTexSubImage3D");
#else
			CheckExistence(glTexSubImage3D);
			glTexSubImage3D(parseTextureTarget(target), level, x, y, z, width, height, depth,
			                parseTextureFormat(format), parseType(type), data);
#endif
			CheckError();
		}

		void SDLGLDevice::CopyTexSubImage2D(Enum target, Integer level, Integer destinationX,
		                                    Integer destinationY, Integer srcX, Integer srcY,
		                                    Sizei width, Sizei height) {
			CheckExistence(glCopyTexSubImage2D);
			glCopyTexSubImage2D(parseTextureTarget(target), level, destinationX, destinationY, srcX,
			                    srcY, width, height);
			CheckError();
		}

		void SDLGLDevice::TexParamater(Enum target, Enum param, Enum val) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum glTarget = parseTextureTarget(target);
			GLenum glParameter = 0;
			GLint glValue = 0;
			int parameterIndex = 0;

			switch (param) {
				case TextureMinFilter:
					glParameter = GL_TEXTURE_MIN_FILTER;
					parameterIndex = 0;
					switch (val) {
						case Nearest: glValue = GL_NEAREST; break;
						case Linear: glValue = GL_LINEAR; break;
						case NearestMipmapLinear: glValue = GL_NEAREST_MIPMAP_LINEAR; break;
						case LinearMipmapLinear: glValue = GL_LINEAR_MIPMAP_LINEAR; break;
						case NearestMipmapNearest: glValue = GL_NEAREST_MIPMAP_NEAREST; break;
						case LinearMipmapNearest: glValue = GL_LINEAR_MIPMAP_NEAREST; break;
						default: SPInvalidEnum("val", val);
					}
					break;
				case TextureMagFilter:
					glParameter = GL_TEXTURE_MAG_FILTER;
					parameterIndex = 1;
					switch (val) {
						case Nearest: glValue = GL_NEAREST; break;
						case Linear: glValue = GL_LINEAR; break;
						default: SPInvalidEnum("val", val);
					}
					break;
				case TextureWrapS:
					glParameter = GL_TEXTURE_WRAP_S;
					parameterIndex = 2;
					switch (val) {
						case ClampToEdge: glValue = GL_CLAMP_TO_EDGE; break;
						case Repeat: glValue = GL_REPEAT; break;
						default: SPInvalidEnum("val", val);
					}
					break;
				case TextureWrapT:
					glParameter = GL_TEXTURE_WRAP_T;
					parameterIndex = 3;
					switch (val) {
						case ClampToEdge: glValue = GL_CLAMP_TO_EDGE; break;
						case Repeat: glValue = GL_REPEAT; break;
						default: SPInvalidEnum("val", val);
					}
					break;
				case TextureWrapR:
					glParameter = GL_TEXTURE_WRAP_R;
					parameterIndex = 4;
					switch (val) {
						case ClampToEdge: glValue = GL_CLAMP_TO_EDGE; break;
						case Repeat: glValue = GL_REPEAT; break;
						default: SPInvalidEnum("val", val);
					}
					break;
				case TextureCompareMode:
					glParameter = GL_TEXTURE_COMPARE_MODE;
					parameterIndex = 5;
					switch (val) {
						case draw::IGLDevice::CompareRefToTexture:
							glValue = GL_COMPARE_REF_TO_TEXTURE;
							break;
						case draw::IGLDevice::None: glValue = GL_NONE; break;
						default: SPInvalidEnum("val", val);
					}
					break;
				case TextureCompareFunc:
					glParameter = GL_TEXTURE_COMPARE_FUNC;
					parameterIndex = 6;
					switch (val) {
						case IGLDevice::LessOrEqual: glValue = GL_LEQUAL; break;
						case IGLDevice::GreaterOrEqual: glValue = GL_GEQUAL; break;
						case IGLDevice::Less: glValue = GL_LESS; break;
						case IGLDevice::Greater: glValue = GL_GREATER; break;
						case IGLDevice::Equal: glValue = GL_EQUAL; break;
						case IGLDevice::NotEqual: glValue = GL_NOTEQUAL; break;
						case IGLDevice::Always: glValue = GL_ALWAYS; break;
						case IGLDevice::Never: glValue = GL_NEVER; break;
						default: SPInvalidEnum("val", val);
					}
					break;
				default: SPInvalidEnum("param", param);
			}

			int bindingIndex = 0;
			switch (target) {
				case Texture2D: bindingIndex = 0; break;
				case Texture3D: bindingIndex = 1; break;
				case Texture2DArray: bindingIndex = 2; break;
				default: SPInvalidEnum("target", target);
			}

			UInteger texture = 0;
			if (activeTextureStageValid && activeTextureStage < textureBindings.size()) {
				const CachedUInteger &binding = textureBindings[activeTextureStage][bindingIndex];
				if (binding.valid)
					texture = binding.value;
			}

			if (texture != 0) {
				auto it = textureParameters.find(texture);
				if (it != textureParameters.end() && it->second.integerValuesValid[parameterIndex] &&
				    it->second.integerValues[parameterIndex] == glValue)
					return;
			}

			CheckExistence(glTexParameteri);
			glTexParameteri(glTarget, glParameter, glValue);
			CheckError();

			if (texture != 0) {
				TextureParameterState &state = textureParameters[texture];
				state.integerValues[parameterIndex] = glValue;
				state.integerValuesValid[parameterIndex] = true;
			}
		}

		void SDLGLDevice::TexParamater(Enum target, Enum param, float val) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum glTarget = parseTextureTarget(target);
			if (param != TextureMaxAnisotropy)
				SPInvalidEnum("param", param);

			int bindingIndex = 0;
			switch (target) {
				case Texture2D: bindingIndex = 0; break;
				case Texture3D: bindingIndex = 1; break;
				case Texture2DArray: bindingIndex = 2; break;
				default: SPInvalidEnum("target", target);
			}

			UInteger texture = 0;
			if (activeTextureStageValid && activeTextureStage < textureBindings.size()) {
				const CachedUInteger &binding = textureBindings[activeTextureStage][bindingIndex];
				if (binding.valid)
					texture = binding.value;
			}

			if (texture != 0) {
				auto it = textureParameters.find(texture);
				if (it != textureParameters.end() && it->second.floatValuesValid[0] &&
				    it->second.floatValues[0] == val)
					return;
			}

			CheckExistence(glTexParameterf);
			glTexParameterf(glTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, val);
			CheckError();

			if (texture != 0) {
				TextureParameterState &state = textureParameters[texture];
				state.floatValues[0] = val;
				state.floatValuesValid[0] = true;
			}
		}

		void SDLGLDevice::GenerateMipmap(spades::draw::IGLDevice::Enum target) {
#if GLEW
			if (glGenerateMipmap)
				glGenerateMipmap(parseTextureTarget(target));
			else if (glGenerateMipmapEXT)
				glGenerateMipmapEXT(parseTextureTarget(target));
			else
				ReportMissingFunc("glGenerateMipmap");
#else
			CheckExistence(glGenerateMipmap);
			glGenerateMipmap(parseTextureTarget(target));
#endif
			CheckError();
		}

		void SDLGLDevice::VertexAttrib(UInteger index, Float x) {
#if GLEW
			if (glVertexAttrib1f)
				glVertexAttrib1f(index, x);
			else if (glVertexAttrib1fARB)
				glVertexAttrib1fARB(index, x);
			else
				ReportMissingFunc("glVertexAttrib1f");
#else
			CheckExistence(glVertexAttrib1f);
			glVertexAttrib1f(index, x);
#endif
			CheckError();
		}

		void SDLGLDevice::VertexAttrib(UInteger index, Float x, Float y) {
#if GLEW
			if (glVertexAttrib2f)
				glVertexAttrib2f(index, x, y);
			else if (glVertexAttrib2fARB)
				glVertexAttrib2fARB(index, x, y);
			else
				ReportMissingFunc("glVertexAttrib2f");
#else
			CheckExistence(glVertexAttrib2f);
			glVertexAttrib2f(index, x, y);
#endif
			CheckError();
		}

		void SDLGLDevice::VertexAttrib(UInteger index, Float x, Float y, Float z) {
#if GLEW
			if (glVertexAttrib3f)
				glVertexAttrib3f(index, x, y, z);
			else if (glVertexAttrib3fARB)
				glVertexAttrib3fARB(index, x, y, z);
			else
				ReportMissingFunc("glVertexAttrib3f");
#else
			CheckExistence(glVertexAttrib2f);
			glVertexAttrib3f(index, x, y, z);
#endif
			CheckError();
		}

		void SDLGLDevice::VertexAttrib(UInteger index, Float x, Float y, Float z, Float w) {
#if GLEW
			if (glVertexAttrib4f)
				glVertexAttrib4f(index, x, y, z, w);
			else if (glVertexAttrib4fARB)
				glVertexAttrib4fARB(index, x, y, z, w);
			else
				ReportMissingFunc("glVertexAttrib4f");
#else
			CheckExistence(glVertexAttrib4f);
			glVertexAttrib4f(index, x, y, z, w);
#endif
			CheckError();
		}

		void SDLGLDevice::VertexAttribPointer(UInteger index, Integer size, Enum type,
		                                      bool normalized, Sizei stride, const void *data) {
#if GLEW
			if (glVertexAttribPointer)
				glVertexAttribPointer(index, size, parseType(type), normalized, stride, data);
			else if (glVertexAttribPointerARB)
				glVertexAttribPointerARB(index, size, parseType(type), normalized, stride, data);
			else
				ReportMissingFunc("glVertexAttribPointer");
#else
			CheckExistence(glVertexAttribPointer);
			glVertexAttribPointer(index, size, parseType(type), normalized, stride, data);
#endif
			CheckError();
		}

		void SDLGLDevice::VertexAttribIPointer(UInteger index, Integer size, Enum type,
		                                       Sizei stride, const void *data) {
#if GLEW
			if (glVertexAttribIPointer)
				glVertexAttribIPointer(index, size, parseType(type), stride, data);
			else if (glVertexAttribIPointerEXT)
				glVertexAttribIPointerEXT(index, size, parseType(type), stride, data);
			else
				ReportMissingFunc("glVertexAttribPointer");
#else
			CheckExistence(glVertexAttribIPointer);
			glVertexAttribIPointer(index, size, parseType(type), stride, data);
#endif
			CheckError();
		}

		void SDLGLDevice::EnableVertexAttribArray(UInteger index, bool b) {
			if (index < vertexAttribArrayStates.size() &&
			    vertexAttribArrayStates[index] == (b ? 1 : 0))
				return;

#if GLEW
			if (glEnableVertexAttribArray) {
				if (b)
					glEnableVertexAttribArray(index);
				else
					glDisableVertexAttribArray(index);
			} else if (glEnableVertexAttribArrayARB) {
				if (b)
					glEnableVertexAttribArrayARB(index);
				else
					glDisableVertexAttribArrayARB(index);
			} else
				ReportMissingFunc("glEnableVertexAttribArray");
#else
			CheckExistence(glEnableVertexAttribArray);
			CheckExistence(glDisableVertexAttribArray);
			if (b)
				glEnableVertexAttribArray(index);
			else
				glDisableVertexAttribArray(index);
#endif
			CheckError();
			if (index < vertexAttribArrayStates.size())
				vertexAttribArrayStates[index] = b ? 1 : 0;
		}

		void SDLGLDevice::VertexAttribDivisor(UInteger index, UInteger divisor) {
			CheckExistence(glVertexAttribDivisorARB);
			glVertexAttribDivisorARB(index, divisor);
			CheckError();
		}

		void SDLGLDevice::DrawArrays(Enum mode, Integer first, Sizei count) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum md;
			switch (mode) {
				case Points: md = GL_POINTS; break;
				case LineStrip: md = GL_LINE_STRIP; break;
				case LineLoop: md = GL_LINE_LOOP; break;
				case Lines: md = GL_LINES; break;
				case TriangleStrip: md = GL_TRIANGLE_STRIP; break;
				case TriangleFan: md = GL_TRIANGLE_FAN; break;
				case Triangles: md = GL_TRIANGLES; break;
				default: SPInvalidEnum("mode", mode);
			}
			vertCount += count;
			drawOps++;
			CheckExistence(glDrawArrays);
			glDrawArrays(md, first, count);
			CheckError();
		}

		void SDLGLDevice::DrawElements(Enum mode, Sizei count, Enum type, const void *indices) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum md;
			switch (mode) {
				case Points: md = GL_POINTS; break;
				case LineStrip: md = GL_LINE_STRIP; break;
				case LineLoop: md = GL_LINE_LOOP; break;
				case Lines: md = GL_LINES; break;
				case TriangleStrip: md = GL_TRIANGLE_STRIP; break;
				case TriangleFan: md = GL_TRIANGLE_FAN; break;
				case Triangles: md = GL_TRIANGLES; break;
				default: SPInvalidEnum("mode", mode);
			}
			vertCount += count;
			drawOps++;
			CheckExistence(glDrawElements);
			glDrawElements(md, count, parseType(type), indices);
			CheckError();
		}

		void SDLGLDevice::DrawArraysInstanced(Enum mode, Integer first, Sizei count,
		                                      Sizei instances) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum md;
			switch (mode) {
				case Points: md = GL_POINTS; break;
				case LineStrip: md = GL_LINE_STRIP; break;
				case LineLoop: md = GL_LINE_LOOP; break;
				case Lines: md = GL_LINES; break;
				case TriangleStrip: md = GL_TRIANGLE_STRIP; break;
				case TriangleFan: md = GL_TRIANGLE_FAN; break;
				case Triangles: md = GL_TRIANGLES; break;
				default: SPInvalidEnum("mode", mode);
			}
#if GLEW
			if (glDrawArraysInstanced)
				glDrawArraysInstanced(md, first, count, instances);
			else if (glDrawArraysInstancedARB)
				glDrawArraysInstancedARB(md, first, count, instances);
			else if (glDrawArraysInstancedEXT)
				glDrawArraysInstancedEXT(md, first, count, instances);
			else
				ReportMissingFunc("glDrawArraysInstanced");
#else
			glDrawArraysInstanced(md, first, count, instances);
#endif
			CheckError();
			vertCount += count * instances;
			drawOps++;
		}

		void SDLGLDevice::DrawElementsInstanced(Enum mode, Sizei count, Enum type,
		                                        const void *indices, Sizei instances) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum md;
			switch (mode) {
				case Points: md = GL_POINTS; break;
				case LineStrip: md = GL_LINE_STRIP; break;
				case LineLoop: md = GL_LINE_LOOP; break;
				case Lines: md = GL_LINES; break;
				case TriangleStrip: md = GL_TRIANGLE_STRIP; break;
				case TriangleFan: md = GL_TRIANGLE_FAN; break;
				case Triangles: md = GL_TRIANGLES; break;
				default: SPInvalidEnum("mode", mode);
			}
#if GLEW
			if (glDrawElementsInstanced)
				glDrawElementsInstanced(md, count, parseType(type), indices, instances);
			else if (glDrawElementsInstancedARB)
				glDrawElementsInstancedARB(md, count, parseType(type), indices, instances);
			else if (glDrawElementsInstancedEXT)
				glDrawElementsInstancedEXT(md, count, parseType(type), indices, instances);
			else
				ReportMissingFunc("glDrawElementsInstanced");
#else
			glDrawElementsInstanced(md, count, parseType(type), indices, instances);
#endif
			CheckError();
			vertCount += count * instances;
			drawOps++;
		}

		IGLDevice::UInteger SDLGLDevice::CreateShader(Enum type) {
			SPADES_MARK_FUNCTION();
			IGLDevice::UInteger ret = 0;
#if GLEW
			if (glCreateShader)
				switch (type) {
					case draw::IGLDevice::FragmentShader:
						ret = glCreateShader(GL_FRAGMENT_SHADER);
						break;
					case draw::IGLDevice::VertexShader:
						ret = glCreateShader(GL_VERTEX_SHADER);
						break;
					default: SPInvalidEnum("type", type);
				}
			else if (glCreateShaderObjectARB)
				switch (type) {
					case draw::IGLDevice::FragmentShader:
						ret = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
						break;
					case draw::IGLDevice::VertexShader:
						ret = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
						break;
					default: SPInvalidEnum("type", type);
				}
			else
				ReportMissingFunc("glCreateShader");
#else
			CheckExistence(glCreateShader);
			switch (type) {
				case draw::IGLDevice::FragmentShader:
					ret = glCreateShader(GL_FRAGMENT_SHADER);
					break;
				case draw::IGLDevice::VertexShader: ret = glCreateShader(GL_VERTEX_SHADER); break;
				case draw::IGLDevice::GeometryShader: ret = glCreateShader(GL_GEOMETRY_SHADER); break;
				default: SPInvalidEnum("type", type);
			}
#endif
			return ret;
		}

		void SDLGLDevice::ShaderSource(UInteger shader, Sizei count, const char **string,
		                               const int *len) {
#if GLEW
			if (glShaderSource)
				glShaderSource(shader, count, (const GLchar **)string, len);
			else if (glShaderSourceARB)
				glShaderSourceARB(shader, count, (const GLchar **)string, len);
			else
				ReportMissingFunc("glShaderSource");
#else
			CheckExistence(glShaderSource);
			glShaderSource(shader, count, (const GLchar **)string, len);
#endif
			CheckError();
		}

		void SDLGLDevice::CompileShader(UInteger i) {
#if GLEW
			if (glCompileShader)
				glCompileShader(i);
			else if (glCompileShaderARB)
				glCompileShaderARB(i);
			else
				ReportMissingFunc("glCompileShader");
#else
			CheckExistence(glCompileShader);
			glCompileShader(i);
#endif
			CheckError();
		}

		void SDLGLDevice::DeleteShader(UInteger i) {
#if GLEW
			if (glDeleteShader)
				glDeleteShader(i);
			else if (glDeleteObjectARB)
				glDeleteObjectARB(i);
			else
				ReportMissingFunc("glDeleteShader");
#else
			CheckExistence(glDeleteShader);
			glDeleteShader(i);
#endif
			CheckError();
		}

		IGLDevice::Integer SDLGLDevice::GetShaderInteger(UInteger shader, Enum param) {
			SPADES_MARK_FUNCTION();
			GLint ret = -1;
#if GLEW
			if (glGetShaderiv)
				switch (param) {
					case ShaderType: glGetShaderiv(shader, GL_SHADER_TYPE, &ret); break;
					case DeleteStatus: glGetShaderiv(shader, GL_DELETE_STATUS, &ret); break;
					case CompileStatus: glGetShaderiv(shader, GL_COMPILE_STATUS, &ret); break;
					case InfoLogLength: glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &ret); break;
					case ShaderSourceLength:
						glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &ret);
						break;
					default: SPInvalidEnum("param", param);
				}
			else if (glGetObjectParameterivARB)
				switch (param) {
					case ShaderType:
						SPRaise("GL_SHADER_TYPE not supported for GL_ARB_shader_objects");
					case DeleteStatus:
						glGetObjectParameterivARB(shader, GL_OBJECT_DELETE_STATUS_ARB, &ret);
						break;
					case CompileStatus:
						glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &ret);
						break;
					case InfoLogLength:
						glGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &ret);
						break;
					case ShaderSourceLength:
						glGetObjectParameterivARB(shader, GL_OBJECT_SHADER_SOURCE_LENGTH_ARB, &ret);
						break;
					default: SPInvalidEnum("param", param);
				}
			else
				ReportMissingFunc("glGetShaderiv");
#else
			CheckExistence(glGetShaderiv);
			switch (param) {
				case ShaderType: glGetShaderiv(shader, GL_SHADER_TYPE, &ret); break;
				case DeleteStatus: glGetShaderiv(shader, GL_DELETE_STATUS, &ret); break;
				case CompileStatus: glGetShaderiv(shader, GL_COMPILE_STATUS, &ret); break;
				case InfoLogLength: glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &ret); break;
				case ShaderSourceLength:
					glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &ret);
					break;
				default: SPInvalidEnum("param", param);
			}
#endif
			CheckError();
			return ret;
		}

		void SDLGLDevice::GetShaderInfoLog(UInteger shader, Sizei bufferSize, Sizei *length,
		                                   char *outString) {
#if GLEW
			if (glGetShaderInfoLog)
				glGetShaderInfoLog(shader, bufferSize, (GLsizei *)length, (GLchar *)outString);
			else if (glGetInfoLogARB)
				glGetInfoLogARB(shader, bufferSize, (GLsizei *)length, (GLchar *)outString);
			else
				ReportMissingFunc("glGetShaderInfoLog");
#else
			CheckExistence(glGetShaderInfoLog);
			glGetShaderInfoLog(shader, bufferSize, (GLsizei *)length, (GLchar *)outString);
#endif
			CheckError();
		}

		IGLDevice::Integer SDLGLDevice::GetProgramInteger(UInteger shader, Enum param) {
			SPADES_MARK_FUNCTION();
			GLint ret = -1;
#if GLEW
			if (glGetProgramiv)
				switch (param) {
					case DeleteStatus: glGetProgramiv(shader, GL_DELETE_STATUS, &ret); break;
					case LinkStatus: glGetProgramiv(shader, GL_LINK_STATUS, &ret); break;
					case ValidateStatus: glGetProgramiv(shader, GL_VALIDATE_STATUS, &ret); break;
					case InfoLogLength: glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &ret); break;
					default: SPInvalidEnum("param", param);
				}
			else if (glGetObjectParameterivARB)
				switch (param) {
					case DeleteStatus:
						glGetObjectParameterivARB(shader, GL_OBJECT_DELETE_STATUS_ARB, &ret);
						break;
					case LinkStatus:
						glGetObjectParameterivARB(shader, GL_OBJECT_LINK_STATUS_ARB, &ret);
						break;
					case ValidateStatus:
						glGetObjectParameterivARB(shader, GL_OBJECT_VALIDATE_STATUS_ARB, &ret);
						break;
					case InfoLogLength:
						glGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &ret);
						break;
					default: SPInvalidEnum("param", param);
				}
			else
				ReportMissingFunc("glGetProgramiv");
#else
			CheckExistence(glGetProgramiv);
			switch (param) {
				case DeleteStatus: glGetProgramiv(shader, GL_DELETE_STATUS, &ret); break;
				case LinkStatus: glGetProgramiv(shader, GL_LINK_STATUS, &ret); break;
				case ValidateStatus: glGetProgramiv(shader, GL_VALIDATE_STATUS, &ret); break;
				case InfoLogLength: glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &ret); break;
				default: SPInvalidEnum("param", param);
			}
#endif
			CheckError();
			return ret;
		}

		void SDLGLDevice::GetProgramInfoLog(UInteger p, Sizei bufferSize, Sizei *length,
		                                    char *outString) {
#if GLEW
			if (glGetProgramInfoLog)
				glGetProgramInfoLog(p, bufferSize, (GLsizei *)length, (GLchar *)outString);
			else if (glGetInfoLogARB)
				glGetInfoLogARB(p, bufferSize, (GLsizei *)length, (GLchar *)outString);
			else
				ReportMissingFunc("glGetShaderInfoLog");
#else
			CheckExistence(glGetProgramInfoLog);
			glGetProgramInfoLog(p, bufferSize, (GLsizei *)length, (GLchar *)outString);
#endif
			CheckError();
		}

		IGLDevice::UInteger SDLGLDevice::CreateProgram() {
#if GLEW
			if (glCreateProgram)
				return glCreateProgram();
			else if (glCreateProgramObjectARB)
				return glCreateProgramObjectARB();
			else
				ReportMissingFunc("glCreateProgram");
			return 0;
#else
			CheckExistence(glCreateProgram);
			return glCreateProgram();
#endif
		}

		void SDLGLDevice::AttachShader(UInteger program, UInteger shader) {
#if GLEW
			if (glAttachShader)
				glAttachShader(program, shader);
			else if (glAttachObjectARB)
				glAttachObjectARB(program, shader);
			else
				ReportMissingFunc("glAttachShader");

#else
			CheckExistence(glAttachShader);
			glAttachShader(program, shader);
#endif
			CheckError();
		}

		void SDLGLDevice::DetachShader(UInteger program, UInteger shader) {
#if GLEW
			if (glDetachShader)
				glDetachShader(program, shader);
			else if (glDetachObjectARB)
				glDetachObjectARB(program, shader);
			else
				ReportMissingFunc("glDetachShader");
#else
			CheckExistence(glDetachShader);
			glDetachShader(program, shader);
#endif
			CheckError();
		}

		void SDLGLDevice::LinkProgram(UInteger program) {
#if GLEW
			if (glLinkProgram)
				glLinkProgram(program);
			else if (glLinkProgramARB)
				glLinkProgramARB(program);
			else
				ReportMissingFunc("glLinkProgram");
#else
			CheckExistence(glLinkProgram);
			glLinkProgram(program);
#endif
			CheckError();
		}

		void SDLGLDevice::UseProgram(UInteger program) {
			if (currentProgramValid && currentProgram == program)
				return;

#if GLEW
			if (glUseProgram)
				glUseProgram(program);
			else if (glUseProgramObjectARB)
				glUseProgramObjectARB(program);
			else
				ReportMissingFunc("glUseProgram");
#else
			CheckExistence(glUseProgram);
			glUseProgram(program);
#endif
			CheckError();
			currentProgram = program;
			currentProgramValid = true;
		}

		void SDLGLDevice::DeleteProgram(UInteger program) {
#if GLEW
			if (glDeleteProgram)
				glDeleteProgram(program);
			else if (glDeleteObjectARB)
				glDeleteObjectARB(program);
			else
				ReportMissingFunc("glDeleteProgram");
#else
			CheckExistence(glDeleteProgram);
			glDeleteProgram(program);
#endif
			CheckError();
			if (currentProgramValid && currentProgram == program)
				currentProgramValid = false;
		}

		void SDLGLDevice::ValidateProgram(UInteger program) {
#if GLEW
			if (glValidateProgram)
				glValidateProgram(program);
			else if (glValidateProgramARB)
				glValidateProgramARB(program);
			else
				ReportMissingFunc("glValidateProgram");
#else
			CheckExistence(glValidateProgram);
			glValidateProgram(program);
#endif
			CheckError();
		}

		IGLDevice::Integer SDLGLDevice::GetAttribLocation(UInteger program, const char *name) {
#if GLEW
			if (glGetAttribLocation)
				return glGetAttribLocation(program, name);
			else if (glGetAttribLocationARB)
				return glGetAttribLocationARB(program, name);
			else
				ReportMissingFunc("glGetAttribLocation");
			return 0;
#else
			CheckExistence(glGetAttribLocation);
			return glGetAttribLocation(program, name);
#endif
		}

		void SDLGLDevice::BindAttribLocation(UInteger program, UInteger index, const char *name) {
#if GLEW
			if (glBindAttribLocation)
				glBindAttribLocation(program, index, name);
			else if (glBindAttribLocationARB)
				glBindAttribLocationARB(program, index, name);
			else
				ReportMissingFunc("glBindAttribLocation");
#else
			CheckExistence(glBindAttribLocation);
			glBindAttribLocation(program, index, name);
#endif
			CheckError();
		}

		IGLDevice::Integer SDLGLDevice::GetUniformLocation(UInteger program, const char *name) {
#if GLEW
			if (glGetUniformLocation)
				return glGetUniformLocation(program, name);
			else if (glGetUniformLocationARB)
				return glGetUniformLocationARB(program, name);
			else
				ReportMissingFunc("glGetUniformLocation");
			return 0;
#else
			CheckExistence(glGetUniformLocation);
			return glGetUniformLocation(program, name);
#endif
		}

		void SDLGLDevice::Uniform(Integer loc, Float x) {
#if GLEW
			if (glUniform1f)
				glUniform1f(loc, x);
			else if (glUniform1fARB)
				glUniform1fARB(loc, x);
			else
				ReportMissingFunc("glUniform1f");
#else
			CheckExistence(glUniform1f);
			glUniform1f(loc, x);
#endif
			CheckError();
		}
		void SDLGLDevice::Uniform(Integer loc, Float x, Float y) {
#if GLEW
			if (glUniform2f)
				glUniform2f(loc, x, y);
			else if (glUniform2fARB)
				glUniform2fARB(loc, x, y);
			else
				ReportMissingFunc("glUniform2f");
#else
			CheckExistence(glUniform2f);
			glUniform2f(loc, x, y);
#endif
			CheckError();
		}
		void SDLGLDevice::Uniform(Integer loc, Float x, Float y, Float z) {
#if GLEW
			if (glUniform3f)
				glUniform3f(loc, x, y, z);
			else if (glUniform3fARB)
				glUniform3fARB(loc, x, y, z);
			else
				ReportMissingFunc("glUniform3f");
#else
			CheckExistence(glUniform3f);
			glUniform3f(loc, x, y, z);
#endif
			CheckError();
		}
		void SDLGLDevice::Uniform(Integer loc, Float x, Float y, Float z, Float w) {
#if GLEW
			if (glUniform4f)
				glUniform4f(loc, x, y, z, w);
			else if (glUniform4fARB)
				glUniform4fARB(loc, x, y, z, w);
			else
				ReportMissingFunc("glUniform4f");
#else
			CheckExistence(glUniform4f);
			glUniform4f(loc, x, y, z, w);
#endif
			CheckError();
		}

		void SDLGLDevice::Uniform(Integer loc, Integer x) {
#if GLEW
			if (glUniform1i)
				glUniform1i(loc, x);
			else if (glUniform1iARB)
				glUniform1iARB(loc, x);
			else
				ReportMissingFunc("glUniform1i");
#else
			CheckExistence(glUniform1i);
			glUniform1i(loc, x);
#endif
			CheckError();
		}
		void SDLGLDevice::Uniform(Integer loc, Integer x, Integer y) {
#if GLEW
			if (glUniform2i)
				glUniform2i(loc, x, y);
			else if (glUniform2iARB)
				glUniform2iARB(loc, x, y);
			else
				ReportMissingFunc("glUniform2i");
#else
			CheckExistence(glUniform2i);
			glUniform2i(loc, x, y);
#endif
			CheckError();
		}
		void SDLGLDevice::Uniform(Integer loc, Integer x, Integer y, Integer z) {
#if GLEW
			if (glUniform3i)
				glUniform3i(loc, x, y, z);
			else if (glUniform3iARB)
				glUniform3iARB(loc, x, y, z);
			else
				ReportMissingFunc("glUniform3i");
#else
			CheckExistence(glUniform3i);
			glUniform3i(loc, x, y, z);
#endif
			CheckError();
		}
		void SDLGLDevice::Uniform(Integer loc, Integer x, Integer y, Integer z, Integer w) {
#if GLEW
			if (glUniform4i)
				glUniform4i(loc, x, y, z, w);
			else if (glUniform4iARB)
				glUniform4iARB(loc, x, y, z, w);
			else
				ReportMissingFunc("glUniform4i");
#else
			CheckExistence(glUniform4i);
			glUniform4i(loc, x, y, z, w);
#endif
			CheckError();
		}
		void SDLGLDevice::Uniform(Integer loc, bool transpose, const spades::Matrix4 &mat) {
#if GLEW
			if (glUniformMatrix4fv)
				glUniformMatrix4fv(loc, 1, transpose ? GL_TRUE : GL_FALSE, mat.m);
			else if (glUniformMatrix4fvARB)
				glUniformMatrix4fvARB(loc, 1, transpose ? GL_TRUE : GL_FALSE, mat.m);
			else
				ReportMissingFunc("glUniformMatrix4fv");
#else
			CheckExistence(glUniformMatrix4fv);
			glUniformMatrix4fv(loc, 1, transpose ? GL_TRUE : GL_FALSE, mat.m);
#endif
			CheckError();
		}

		GLenum SDLGLDevice::parseFramebufferTarget(Enum v) {
			SPADES_MARK_FUNCTION_DEBUG();
			switch (v) {
				case draw::IGLDevice::Framebuffer: return GL_FRAMEBUFFER;
				case draw::IGLDevice::ReadFramebuffer: return GL_READ_FRAMEBUFFER;
				case draw::IGLDevice::DrawFramebuffer: return GL_DRAW_FRAMEBUFFER;
				default: SPInvalidEnum("v", v);
			}
		}

		IGLDevice::UInteger SDLGLDevice::GenFramebuffer() {
			GLuint v = 0;
#if GLEW
			if (glGenFramebuffers)
				glGenFramebuffers(1, &v);
			else if (glGenFramebuffersEXT)
				glGenFramebuffersEXT(1, &v);
			else
				ReportMissingFunc("glGenFramebuffers");
#else
			CheckExistence(glGenFramebuffers);
			glGenFramebuffers(1, &v);
#endif
			CheckError();
			return (IGLDevice::UInteger)v;
		}
		void SDLGLDevice::BindFramebuffer(Enum target, UInteger framebuffer) {
			GLenum glTarget = parseFramebufferTarget(target);
			if ((target == Framebuffer && readFramebuffer.valid && drawFramebuffer.valid &&
			     readFramebuffer.value == framebuffer && drawFramebuffer.value == framebuffer) ||
			    (target == ReadFramebuffer && readFramebuffer.valid &&
			     readFramebuffer.value == framebuffer) ||
			    (target == DrawFramebuffer && drawFramebuffer.valid &&
			     drawFramebuffer.value == framebuffer))
				return;

#if GLEW
			if (glBindFramebuffer)
				glBindFramebuffer(glTarget, framebuffer);
			else if (glBindFramebufferEXT)
				glBindFramebufferEXT(glTarget, framebuffer);
			else
				ReportMissingFunc("glBindFramebuffer");
#else
			CheckExistence(glBindFramebuffer);
			glBindFramebuffer(glTarget, framebuffer);
#endif
			CheckError();
			if (target == Framebuffer || target == ReadFramebuffer) {
				readFramebuffer.value = framebuffer;
				readFramebuffer.valid = true;
			}
			if (target == Framebuffer || target == DrawFramebuffer) {
				drawFramebuffer.value = framebuffer;
				drawFramebuffer.valid = true;
			}
		}
		void SDLGLDevice::DeleteFramebuffer(UInteger fb) {
#if GLEW
			if (glDeleteFramebuffers)
				glDeleteFramebuffers(1, &fb);
			else if (glDeleteFramebuffersEXT)
				glDeleteFramebuffersEXT(1, &fb);
			else
				ReportMissingFunc("glDeleteFramebuffers");
#else
			CheckExistence(glDeleteFramebuffers);
			glDeleteFramebuffers(1, &fb);
#endif
			CheckError();
			if (readFramebuffer.valid && readFramebuffer.value == fb)
				readFramebuffer.value = 0;
			if (drawFramebuffer.valid && drawFramebuffer.value == fb)
				drawFramebuffer.value = 0;
		}
		IGLDevice::Enum SDLGLDevice::CheckFramebufferStatus(spades::draw::IGLDevice::Enum target) {
			GLenum ret = 0;
#if GLEW
			if (glCheckFramebufferStatus)
				ret = glCheckFramebufferStatus(parseFramebufferTarget(target));
			else if (glCheckFramebufferStatusEXT)
				ret = glCheckFramebufferStatusEXT(parseFramebufferTarget(target));
			else
				ReportMissingFunc("glCheckFramebufferStatus");
#else
			CheckExistence(glCheckFramebufferStatus);
			ret = glCheckFramebufferStatus(parseFramebufferTarget(target));
#endif
			CheckError();
			switch (ret) {
				case GL_FRAMEBUFFER_COMPLETE: return FramebufferComplete;
				case GL_FRAMEBUFFER_UNDEFINED: return FramebufferUndefined;
				case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return FramebufferIncompleteAttachment;
				case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
					return FramebufferIncompleteMissingAttachment;
				case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: return FramebufferIncompleteDrawBuffer;
				case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: return FramebufferIncompleteReadBuffer;
				case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: return FramebufferIncompleteMultisample;
				case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
					return FramebufferIncompleteLayerTargets;
				default: return FramebufferUnsupported;
			}
		}
		void SDLGLDevice::FramebufferTexture2D(Enum target, Enum attachment, Enum texTarget,
		                                       UInteger texture, Integer level) {
			SPADES_MARK_FUNCTION_DEBUG();

			GLenum a;
			switch (attachment) {
				case draw::IGLDevice::ColorAttachment0: a = GL_COLOR_ATTACHMENT0; break;
				case draw::IGLDevice::ColorAttachment1: a = GL_COLOR_ATTACHMENT1; break;
				case draw::IGLDevice::ColorAttachment2: a = GL_COLOR_ATTACHMENT2; break;
				case draw::IGLDevice::ColorAttachment3: a = GL_COLOR_ATTACHMENT3; break;
				case draw::IGLDevice::ColorAttachment4: a = GL_COLOR_ATTACHMENT4; break;
				case draw::IGLDevice::ColorAttachment5: a = GL_COLOR_ATTACHMENT5; break;
				case draw::IGLDevice::ColorAttachment6: a = GL_COLOR_ATTACHMENT6; break;
				case draw::IGLDevice::ColorAttachment7: a = GL_COLOR_ATTACHMENT7; break;
				case draw::IGLDevice::DepthAttachment: a = GL_DEPTH_ATTACHMENT; break;
				case draw::IGLDevice::StencilAttachment: a = GL_STENCIL_ATTACHMENT; break;
				default: SPInvalidEnum("attachment", attachment);
			}
#if GLEW
			if (glFramebufferTexture2D)
				glFramebufferTexture2D(parseFramebufferTarget(target), a,
				                       parseTextureTarget(texTarget), texture, level);
			else if (glFramebufferTexture2DEXT)
				glFramebufferTexture2DEXT(parseFramebufferTarget(target), a,
				                          parseTextureTarget(texTarget), texture, level);
			else
				ReportMissingFunc("glFramebufferTexture2D");
#else
			CheckExistence(glFramebufferTexture2D);
			glFramebufferTexture2D(parseFramebufferTarget(target), a, parseTextureTarget(texTarget),
			                       texture, level);
#endif
			CheckErrorAlways();
		}

		void SDLGLDevice::BlitFramebuffer(Integer srcX0, Integer srcY0, Integer srcX1,
		                                  Integer srcY1, Integer dstX0, Integer dstY0,
		                                  Integer dstX1, Integer dstY1, UInteger mask,
		                                  Enum filter) {
			SPADES_MARK_FUNCTION_DEBUG();

			GLenum flt;
			switch (filter) {
				case draw::IGLDevice::Linear: flt = GL_LINEAR; break;
				case draw::IGLDevice::Nearest: flt = GL_NEAREST; break;
				default: SPInvalidEnum("filter", filter);
			}

			GLbitfield m = 0;
			if (mask & ColorBufferBit)
				m |= GL_COLOR_BUFFER_BIT;
			if (mask & DepthBufferBit)
				m |= GL_DEPTH_BUFFER_BIT;
			if (mask & StencilBufferBit)
				m |= GL_STENCIL_BUFFER_BIT;
#if GLEW
			if (glBlitFramebuffer)
				glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, m, flt);
			else if (glBlitFramebufferEXT)
				glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, m,
				                     flt);
			else
				ReportMissingFunc("glBlitFramebuffer");
#else
			CheckExistence(glBlitFramebuffer);
			glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, m, flt);
#endif
			CheckError();
		}

		GLenum SDLGLDevice::parseRenderbufferTarget(Enum e) {
			SPADES_MARK_FUNCTION_DEBUG();
			switch (e) {
				case draw::IGLDevice::Renderbuffer: return GL_RENDERBUFFER;
				default: SPInvalidEnum("e", e);
			}
		}

		IGLDevice::UInteger SDLGLDevice::GenRenderbuffer() {
			GLuint v = 0;
#if GLEW
			if (glGenRenderbuffers)
				glGenRenderbuffers(1, &v);
			else if (glGenRenderbuffersEXT)
				glGenRenderbuffersEXT(1, &v);
			else
				ReportMissingFunc("glGenRenderbuffers");
#else
			CheckExistence(glGenRenderbuffers);
			glGenRenderbuffers(1, &v);
#endif
			CheckError();
			return v;
		}
		void SDLGLDevice::DeleteRenderbuffer(UInteger v) {
#if GLEW
			if (glDeleteRenderbuffers)
				glDeleteRenderbuffers(1, &v);
			else if (glDeleteRenderbuffersEXT)
				glDeleteRenderbuffersEXT(1, &v);
			else
				ReportMissingFunc("glDeleteRenderbuffers");
#else
			CheckExistence(glDeleteRenderbuffers);
			glDeleteRenderbuffers(1, &v);
#endif
			CheckError();
			if (renderbufferBinding.valid && renderbufferBinding.value == v)
				renderbufferBinding.value = 0;
		}
		void SDLGLDevice::BindRenderbuffer(Enum target, UInteger v) {
			SPADES_MARK_FUNCTION_DEBUG();
			GLenum glTarget = parseRenderbufferTarget(target);
			if (renderbufferBinding.valid && renderbufferBinding.value == v)
				return;

#if GLEW
			if (glBindRenderbuffer)
				glBindRenderbuffer(glTarget, v);
			else if (glBindRenderbufferEXT)
				glBindRenderbufferEXT(glTarget, v);
			else
				ReportMissingFunc("glBindRenderbuffer");
#else
			CheckExistence(glBindRenderbuffer);
			glBindRenderbuffer(glTarget, v);
#endif
			CheckError();
			renderbufferBinding.value = v;
			renderbufferBinding.valid = true;
		}
		void SDLGLDevice::RenderbufferStorage(Enum target, Enum intFormat, Sizei width,
		                                      Sizei height) {
#if GLEW
			if (glRenderbufferStorage)
				glRenderbufferStorage(parseRenderbufferTarget(target),
				                      parseTextureInternalFormat(intFormat), width, height);
			else if (glRenderbufferStorageEXT)
				glRenderbufferStorageEXT(parseRenderbufferTarget(target),
				                         parseTextureInternalFormat(intFormat), width, height);
			else
				ReportMissingFunc("glRenderbufferStorage");

#else
			CheckExistence(glRenderbufferStorage);
			glRenderbufferStorage(parseRenderbufferTarget(target),
			                      parseTextureInternalFormat(intFormat), width, height);
#endif
			CheckErrorAlways();
		}
		void SDLGLDevice::RenderbufferStorage(Enum target, Sizei samples, Enum intFormat,
		                                      Sizei width, Sizei height) {
#if GLEW
			if (glRenderbufferStorageMultisample)
				glRenderbufferStorageMultisample(parseRenderbufferTarget(target), samples,
				                                 parseTextureInternalFormat(intFormat), width,
				                                 height);
			else if (glRenderbufferStorageMultisampleEXT)
				glRenderbufferStorageMultisampleEXT(parseRenderbufferTarget(target), samples,
				                                    parseTextureInternalFormat(intFormat), width,
				                                    height);
			else
				ReportMissingFunc("glRenderbufferStorageMultisample");
#else
			CheckExistence(glRenderbufferStorageMultisample);
			glRenderbufferStorageMultisample(parseRenderbufferTarget(target), samples,
			                                 parseTextureInternalFormat(intFormat), width, height);
#endif
			CheckErrorAlways();
		}
		void SDLGLDevice::FramebufferRenderbuffer(Enum target, Enum attachment, Enum rbTarget,
		                                          UInteger rb) {

			GLenum a;
			switch (attachment) {
				case draw::IGLDevice::ColorAttachment0: a = GL_COLOR_ATTACHMENT0; break;
				case draw::IGLDevice::ColorAttachment1: a = GL_COLOR_ATTACHMENT1; break;
				case draw::IGLDevice::ColorAttachment2: a = GL_COLOR_ATTACHMENT2; break;
				case draw::IGLDevice::ColorAttachment3: a = GL_COLOR_ATTACHMENT3; break;
				case draw::IGLDevice::ColorAttachment4: a = GL_COLOR_ATTACHMENT4; break;
				case draw::IGLDevice::ColorAttachment5: a = GL_COLOR_ATTACHMENT5; break;
				case draw::IGLDevice::ColorAttachment6: a = GL_COLOR_ATTACHMENT6; break;
				case draw::IGLDevice::ColorAttachment7: a = GL_COLOR_ATTACHMENT7; break;
				case draw::IGLDevice::DepthAttachment: a = GL_DEPTH_ATTACHMENT; break;
				case draw::IGLDevice::StencilAttachment: a = GL_STENCIL_ATTACHMENT; break;
				default: SPInvalidEnum("attachment", attachment);
			}
#if GLEW
			if (glFramebufferRenderbuffer)
				glFramebufferRenderbuffer(parseFramebufferTarget(target), a,
				                          parseRenderbufferTarget(rbTarget), rb);
			else if (glFramebufferRenderbufferEXT)
				glFramebufferRenderbufferEXT(parseFramebufferTarget(target), a,
				                             parseRenderbufferTarget(rbTarget), rb);
			else
				ReportMissingFunc("glFramebufferRenderbuffer");
#else
			CheckExistence(glFramebufferRenderbuffer);
			glFramebufferRenderbuffer(parseFramebufferTarget(target), a,
			                          parseRenderbufferTarget(rbTarget), rb);
#endif
			CheckErrorAlways();
		}

		void SDLGLDevice::ReadPixels(Integer x, Integer y, Sizei width, Sizei height, Enum format,
		                             Enum type, void *data) {
			CheckExistence(glReadPixels);
			glReadPixels(x, y, width, height, parseTextureFormat(format), parseType(type), data);
			CheckErrorAlways();
		}

		IGLDevice::Integer SDLGLDevice::ScreenWidth() { return w; }

		IGLDevice::Integer SDLGLDevice::ScreenHeight() { return h; }
	}
}
