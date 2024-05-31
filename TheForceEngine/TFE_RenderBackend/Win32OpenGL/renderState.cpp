#include <TFE_RenderBackend/renderState.h>
#include <TFE_System/system.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <GL/gl.h>
#include <vector>

namespace TFE_RenderState
{
	static const GLenum c_blendFuncGL[] =
	{
		GL_FUNC_ADD, // BLEND_FUNC_ADD
	};

	static const GLenum c_blendFactor[] =
	{
		GL_ONE,  // BLEND_ONE = 0,
		GL_ZERO, // BLEND_ZERO,
		GL_SRC_ALPHA, // BLEND_SRC_ALPHA,
		GL_ONE_MINUS_SRC_ALPHA// BLEND_ONE_MINUS_SRC_ALPHA,
	};

	static const GLenum c_comparisionFunc[] =
	{
		GL_NEVER,    //CMP_NEVER
		GL_LESS,     //CMP_LESS
		GL_EQUAL,    //CMP_EQUAL
		GL_LEQUAL,   //CMP_LEQUAL
		GL_GREATER,  //CMP_GREATER
		GL_NOTEQUAL, //CMP_NOTEQUAL
		GL_GEQUAL,   //CMP_GEQUAL
		GL_ALWAYS,   //CMP_ALWAYS
	};

	static const GLenum c_stencilOp[] =
	{
		GL_KEEP,      // OP_KEEP
		GL_ZERO,      // OP_ZERO
		GL_INCR,      // OP_INC
		GL_DECR,      // OP_DEC
		GL_INVERT,    // OP_INVERT
		GL_REPLACE,   // OP_REPLACE
		GL_INCR_WRAP, // OP_INC_WRAP
		GL_DECR_WRAP, // OP_DEC_WRAP
	};

	static u32 s_currentState;
	static u32 s_depthFunc;
	static u32 s_colorMask;
	static s32 s_clipPlaneCount = 0;

	struct StencilFuncState
	{
		ComparisonFunction func;
		s32 ref;
		u32 mask;
	};
	struct StencilOpState
	{
		StencilOp stencilFail;
		StencilOp depthFail;
		StencilOp depthStencilPass;
	};
	StencilFuncState s_stencilFunc;
	StencilOpState s_stencilOp;

	void clear()
	{
		s_currentState = 0u;
		s_colorMask = CMASK_ALL;
		s_depthFunc = CMP_LEQUAL;
		s_stencilFunc = { CMP_ALWAYS, 0, 0xffffffff };
		s_stencilOp = { OP_KEEP, OP_KEEP, OP_KEEP };
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(false);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		setDepthBias(0.0f, 0.0f);
		glStencilMask(0);
		glStencilFunc(GL_ALWAYS, 0, 0xffffffff);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		enableClipPlanes();
	}

	void setStateEnable(bool enable, u32 stateFlags)
	{
		if (enable)
		{
			const u32 stateToChange = stateFlags & (~s_currentState);
			if (stateToChange & STATE_CULLING)
			{
				glEnable(GL_CULL_FACE);
			}
			if (stateToChange & STATE_BLEND)
			{
				glEnable(GL_BLEND);
			}
			if (stateToChange & STATE_DEPTH_TEST)
			{
				glEnable(GL_DEPTH_TEST);
			}
			if (stateToChange & STATE_DEPTH_WRITE)
			{
				glDepthMask(true);
			}
			if (stateToChange & STATE_STENCIL_WRITE)
			{
				glStencilMask(0xffffffff);
			}
			if (stateToChange & STATE_STENCIL_TEST)
			{
				glEnable(GL_STENCIL_TEST);
			}
			if (stateToChange & STATE_WIREFRAME)
			{
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			}
			s_currentState |= stateFlags;
		}
		else
		{
			const u32 stateToChange = stateFlags & s_currentState;
			if (stateToChange & STATE_CULLING)
			{
				glDisable(GL_CULL_FACE);
			}
			if (stateToChange & STATE_BLEND)
			{
				glDisable(GL_BLEND);
			}
			if (stateToChange & STATE_DEPTH_TEST)
			{
				glDisable(GL_DEPTH_TEST);
			}
			if (stateToChange & STATE_DEPTH_WRITE)
			{
				glDepthMask(false);
			}
			if (stateToChange & STATE_STENCIL_WRITE)
			{
				glStencilMask(0);
			}
			if (stateToChange & STATE_STENCIL_TEST)
			{
				glDisable(GL_STENCIL_TEST);
			}
			if (stateToChange & STATE_WIREFRAME)
			{
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
			s_currentState &= ~stateFlags;
		}
	}
		
	void setBlendMode(StateBlendFactor srcFactor, StateBlendFactor dstFactor, StateBlendFunc func)
	{
		glBlendEquation(c_blendFuncGL[func]);
		glBlendFunc(c_blendFactor[srcFactor], c_blendFactor[dstFactor]);
	}

	void setDepthFunction(ComparisonFunction func)
	{
		if (func != s_depthFunc)
		{
			glDepthFunc(c_comparisionFunc[func]);
			s_depthFunc = func;
		}
	}
	
	void setStencilFunction(ComparisonFunction func, s32 ref, u32 mask)
	{
		if (func != s_stencilFunc.func || ref != s_stencilFunc.ref || mask != s_stencilFunc.mask)
		{
			s_stencilFunc.func = func;
			s_stencilFunc.ref = ref;
			s_stencilFunc.mask = mask;
			glStencilFunc(c_comparisionFunc[func], ref, mask);
		}
	}

	void setStencilOp(StencilOp stencilFail, StencilOp depthFail, StencilOp depthStencilPass)
	{
		if (stencilFail != s_stencilOp.stencilFail || depthFail != s_stencilOp.depthFail || depthStencilPass != s_stencilOp.depthStencilPass)
		{
			s_stencilOp.stencilFail = stencilFail;
			s_stencilOp.depthFail = depthFail;
			s_stencilOp.depthStencilPass = depthStencilPass;
			glStencilOp(c_stencilOp[stencilFail], c_stencilOp[depthFail], c_stencilOp[depthStencilPass]);
		}
	}

	void setColorMask(u32 colorMask)
	{
		if (colorMask != s_colorMask)
		{
			glColorMask((colorMask&CMASK_RED)!=0 ? GL_TRUE : GL_FALSE,  (colorMask&CMASK_GREEN)!=0 ? GL_TRUE : GL_FALSE,
				        (colorMask&CMASK_BLUE)!=0 ? GL_TRUE : GL_FALSE, (colorMask&CMASK_ALPHA)!=0 ? GL_TRUE : GL_FALSE);
			s_colorMask = colorMask;
		}
	}

	void setDepthBias(f32 factor, f32 bias)
	{
		if (factor != 0.0f || bias != 0.0f)
		{
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(factor, bias);
		}
		else
		{
			glDisable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(0.0f, 0.0f);
		}
	}
	
	void enableClipPlanes(s32 count)
	{
		if (s_clipPlaneCount != count)
		{
			// Disable unused planes.
			for (s32 p = count; p < s_clipPlaneCount; p++)
			{
				glDisable(GL_CLIP_DISTANCE0 + p);
			}
			// Enable new planes.
			for (s32 p = 0; p < count; p++)
			{
				glEnable(GL_CLIP_DISTANCE0 + p);
			}

			s_clipPlaneCount = count;
		}
	}
}
