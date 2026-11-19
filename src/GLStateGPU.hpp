#pragma once
#include "GLState.hpp"
#include <array>
// Additional state changed by the pinned ThorVG GL backend. Base guard restores adapter state.
struct GLStateGPU : GLState {
    GLint drawFbo, readFbo, renderbuffer, element, uniform, viewport[4], scissorBox[4], srcRGB,
        dstRGB, srcA, dstA, eqRGB, eqA, depthFunc, front, cullMode, clearStencil;
    GLfloat clearColor[4], attribute[4];
    GLdouble clearDepth;
    GLboolean depthWrite, stencil, srgb, multisample, discard;
    struct Unit {
        GLint texture, sampler;
    };
    std::array<Unit, 16> units;
    struct Binding {
        GLint buffer;
        GLint64 start, size;
    };
    std::array<Binding, 8> ubos;
    struct Stencil {
        GLint func, ref, valueMask, writeMask, fail, zfail, zpass;
    };
    Stencil faces[2];
    GLStateGPU() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element);
        glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &uniform);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
        glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
        glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcA);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &dstA);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &eqRGB);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eqA);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        glGetIntegerv(GL_FRONT_FACE, &front);
        glGetIntegerv(GL_CULL_FACE_MODE, &cullMode);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        glGetDoublev(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &clearStencil);
        glGetVertexAttribfv(1, GL_CURRENT_VERTEX_ATTRIB, attribute);
        stencil = glIsEnabled(GL_STENCIL_TEST);
        srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
        multisample = glIsEnabled(GL_MULTISAMPLE);
        discard = glIsEnabled(GL_RASTERIZER_DISCARD);
        const GLenum names[2][7] = {
            {GL_STENCIL_FUNC, GL_STENCIL_REF, GL_STENCIL_VALUE_MASK, GL_STENCIL_WRITEMASK,
             GL_STENCIL_FAIL, GL_STENCIL_PASS_DEPTH_FAIL, GL_STENCIL_PASS_DEPTH_PASS},
            {GL_STENCIL_BACK_FUNC, GL_STENCIL_BACK_REF, GL_STENCIL_BACK_VALUE_MASK,
             GL_STENCIL_BACK_WRITEMASK, GL_STENCIL_BACK_FAIL, GL_STENCIL_BACK_PASS_DEPTH_FAIL,
             GL_STENCIL_BACK_PASS_DEPTH_PASS}};
        for (int i = 0; i < 2; ++i) {
            auto &f = faces[i];
            GLint *values[] = {&f.func, &f.ref,   &f.valueMask, &f.writeMask,
                               &f.fail, &f.zfail, &f.zpass};
            for (int j = 0; j < 7; ++j)
                glGetIntegerv(names[i][j], values[j]);
        }
        for (unsigned i = 0; i < units.size(); ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &units[i].texture);
            glGetIntegerv(GL_SAMPLER_BINDING, &units[i].sampler);
        }
        for (unsigned i = 0; i < ubos.size(); ++i) {
            glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, i, &ubos[i].buffer);
            glGetInteger64i_v(GL_UNIFORM_BUFFER_START, i, &ubos[i].start);
            glGetInteger64i_v(GL_UNIFORM_BUFFER_SIZE, i, &ubos[i].size);
        }
        glActiveTexture(GL_TEXTURE0);
    }
    void prepare() {
        glActiveTexture(GL_TEXTURE0);
        GLState::prepare();
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glDisable(GL_RASTERIZER_DISCARD);
        glEnable(GL_MULTISAMPLE);
        glStencilMask(~0u);
        glDepthMask(true);
        glBlendEquation(GL_FUNC_ADD);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        for (unsigned i = 0; i < units.size(); ++i)
            glBindSampler(i, 0);
    }
    ~GLStateGPU() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glBindVertexArray(vao);
        if (vao)
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element);
        for (unsigned i = 0; i < ubos.size(); ++i) {
            auto &u = ubos[i];
            if (u.buffer && u.size > 0)
                glBindBufferRange(GL_UNIFORM_BUFFER, i, u.buffer, GLintptr(u.start),
                                  GLsizeiptr(u.size));
            else
                glBindBufferBase(GL_UNIFORM_BUFFER, i, u.buffer);
        }
        glBindBuffer(GL_UNIFORM_BUFFER, uniform);
        for (unsigned i = 0; i < units.size(); ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, units[i].texture);
            glBindSampler(i, units[i].sampler);
        }
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
        glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA);
        glBlendEquationSeparate(eqRGB, eqA);
        glDepthFunc(depthFunc);
        glDepthMask(depthWrite);
        glFrontFace(front);
        glCullFace(cullMode);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glClearDepth(clearDepth);
        glClearStencil(clearStencil);
        glVertexAttrib4fv(1, attribute);
        for (int i = 0; i < 2; ++i) {
            GLenum side = i ? GL_BACK : GL_FRONT;
            auto &f = faces[i];
            glStencilFuncSeparate(side, f.func, f.ref, f.valueMask);
            glStencilMaskSeparate(side, f.writeMask);
            glStencilOpSeparate(side, f.fail, f.zfail, f.zpass);
        }
        enable(GL_STENCIL_TEST, stencil);
        enable(GL_FRAMEBUFFER_SRGB, srgb);
        enable(GL_MULTISAMPLE, multisample);
        enable(GL_RASTERIZER_DISCARD, discard);
    }
};
