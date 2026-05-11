#include "icelut_opengl_demo.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace icelut_demo {
namespace {

const char* shaderTypeName(GLenum type) {
    switch (type) {
        case GL_VERTEX_SHADER:
            return "vertex";
        case GL_FRAGMENT_SHADER:
            return "fragment";
        default:
            return "unknown";
    }
}

std::vector<std::uint8_t> readBytes(const std::string& path, std::size_t expectedBytes) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open: " + path);
    }

    std::vector<std::uint8_t> data(expectedBytes);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (file.gcount() != static_cast<std::streamsize>(data.size())) {
        throw std::runtime_error("Unexpected file size: " + path);
    }
    return data;
}

std::vector<float> readFloatRaw(const std::string& path, std::size_t count) {
    std::vector<std::uint8_t> bytes = readBytes(path, count * sizeof(float));
    std::vector<float> values(count);
    std::copy(bytes.begin(), bytes.end(), reinterpret_cast<std::uint8_t*>(values.data()));
    return values;
}

std::vector<std::int8_t> readInt8Raw(const std::string& path, std::size_t count) {
    std::vector<std::uint8_t> bytes = readBytes(path, count);
    std::vector<std::int8_t> values(count);
    std::copy(bytes.begin(), bytes.end(), reinterpret_cast<std::uint8_t*>(values.data()));
    return values;
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error(std::string("Failed to compile ") + shaderTypeName(type) + " shader: " + log);
    }
    return shader;
}

GLuint linkProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        glDeleteProgram(program);
        throw std::runtime_error(log);
    }
    return program;
}

const char* fullscreenVertexShader() {
    return R"GLSL(
        #version 300 es
        out vec2 vUv;

        const vec2 positions[3] = vec2[3](
            vec2(-1.0, -1.0),
            vec2( 3.0, -1.0),
            vec2(-1.0,  3.0)
        );

        void main() {
            vec2 p = positions[gl_VertexID];
            vUv = p * 0.5 + 0.5;
            gl_Position = vec4(p, 0.0, 1.0);
        }
    )GLSL";
}

const char* copyFragmentShader() {
    return R"GLSL(
        #version 300 es
        precision mediump float;
        uniform sampler2D uInput;
        in vec2 vUv;
        out vec4 fragColor;

        void main() {
            fragColor = texture(uInput, vUv);
        }
    )GLSL";
}

const char* applyLutFragmentShader() {
    return R"GLSL(
        #version 300 es
        precision mediump float;
        precision mediump sampler3D;
        uniform sampler2D uInput;
        uniform sampler3D uLut;
        in vec2 vUv;
        out vec4 fragColor;

        vec3 lutCoord(vec3 rgb) {
            float dim = 33.0;
            return rgb * ((dim - 1.0) / dim) + vec3(0.5 / dim);
        }

        void main() {
            vec4 src = texture(uInput, vUv);
            vec3 residual = texture(uLut, lutCoord(clamp(src.rgb, 0.0, 1.0))).rgb;
            vec3 outRgb = clamp(src.rgb + 0.5 * residual, 0.0, 1.0);
            fragColor = vec4(outRgb, src.a);
        }
    )GLSL";
}

class ScopedGLState {
public:
    ScopedGLState() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray_);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer_);
        glGetIntegerv(GL_VIEWPORT, viewport_);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment_);

        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2D0_);
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &texture3D0_);

        glActiveTexture(GL_TEXTURE1);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2D1_);
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &texture3D1_);

        glActiveTexture(static_cast<GLenum>(activeTexture_));
    }

    ~ScopedGLState() {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer_));
        glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
        glUseProgram(static_cast<GLuint>(program_));
        glBindVertexArray(static_cast<GLuint>(vertexArray_));
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment_);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture2D0_));
        glBindTexture(GL_TEXTURE_3D, static_cast<GLuint>(texture3D0_));

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture2D1_));
        glBindTexture(GL_TEXTURE_3D, static_cast<GLuint>(texture3D1_));

        glActiveTexture(static_cast<GLenum>(activeTexture_));
    }

private:
    GLint program_ = 0;
    GLint vertexArray_ = 0;
    GLint framebuffer_ = 0;
    GLint viewport_[4] = {};
    GLint activeTexture_ = GL_TEXTURE0;
    GLint unpackAlignment_ = 4;
    GLint texture2D0_ = 0;
    GLint texture3D0_ = 0;
    GLint texture2D1_ = 0;
    GLint texture3D1_ = 0;
};

}  // namespace

IcelutOpenGLProcessor::IcelutOpenGLProcessor() {
    std::string filepath = "../LUT/exported_raw";
    loadAssets(filepath);
    createGlObjects();
}

IcelutOpenGLProcessor::~IcelutOpenGLProcessor() {
    if (lutTexture_ != 0) glDeleteTextures(1, &lutTexture_);
    if (analysisTexture_ != 0) glDeleteTextures(1, &analysisTexture_);
    if (analysisFbo_ != 0) glDeleteFramebuffers(1, &analysisFbo_);
    if (outputFbo_ != 0) glDeleteFramebuffers(1, &outputFbo_);
    if (copyProgram_ != 0) glDeleteProgram(copyProgram_);
    if (applyProgram_ != 0) glDeleteProgram(applyProgram_);
    if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
}


GLuint IcelutOpenGLProcessor::processTextureToNewTexture(GLuint sourceTexture, int width, int height) {
    GLuint outputTexture = createOutputTexture(width, height);
    processTextureToTexture(sourceTexture, outputTexture, width, height);
    return outputTexture;
}

void IcelutOpenGLProcessor::processTextureToTexture(GLuint sourceTexture,
                                                    GLuint outputTexture,
                                                    int width,
                                                    int height) {
    if (sourceTexture == outputTexture) {
        throw std::runtime_error("sourceTexture and outputTexture must be different textures");
    }

    ScopedGLState scopedState;
    GLint previousFramebuffer = 0;
    GLint previousViewport[4] = {};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    std::array<float, kNumBasis> weights = computeWeightsFromTexture(sourceTexture);
    buildFinalLut(weights);
    uploadFinalLut();

    glBindFramebuffer(GL_FRAMEBUFFER, outputFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Output framebuffer is incomplete");
    }

    glViewport(0, 0, width, height);
    drawWithLut(sourceTexture);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
}

void IcelutOpenGLProcessor::loadAssets(const std::string& rawDir) {
    basisLuts_ = readFloatRaw(rawDir + "/basis_luts_gl_float32.raw",
                              kNumBasis * kDim * kDim * kDim * 3);
    modelMsb_ = readFloatRaw(rawDir + "/model_msb_fp32.raw", kNibbleCount * kFeatureCount);
    modelLsb_ = readFloatRaw(rawDir + "/model_lsb_fp32.raw", kNibbleCount * kFeatureCount);
    classifier_ = readInt8Raw(rawDir + "/classifier_int8.raw",
                              kClassifierGroups * 64 * 64 * kNumBasis);
    finalLut_.resize(kDim * kDim * kDim * 3);
}

void IcelutOpenGLProcessor::createGlObjects() {
    glGenVertexArrays(1, &vao_);

    copyProgram_ = linkProgram(fullscreenVertexShader(), copyFragmentShader());
    applyProgram_ = linkProgram(fullscreenVertexShader(), applyLutFragmentShader());

    glGenFramebuffers(1, &analysisFbo_);
    glGenFramebuffers(1, &outputFbo_);

    glGenTextures(1, &analysisTexture_);
    glBindTexture(GL_TEXTURE_2D, analysisTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kAnalysisSize, kAnalysisSize, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenTextures(1, &lutTexture_);
    glBindTexture(GL_TEXTURE_3D, lutTexture_);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F, kDim, kDim, kDim, 0,
                 GL_RGB, GL_FLOAT, nullptr);
}

void IcelutOpenGLProcessor::renderCopy(GLuint sourceTexture, GLuint framebuffer, int width, int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
    glUseProgram(copyProgram_);
    glBindVertexArray(vao_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glUniform1i(glGetUniformLocation(copyProgram_, "uInput"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

std::array<float, kNumBasis> IcelutOpenGLProcessor::computeWeightsFromTexture(GLuint sourceTexture) {
    glBindFramebuffer(GL_FRAMEBUFFER, analysisFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, analysisTexture_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Analysis framebuffer is incomplete");
    }

    renderCopy(sourceTexture, analysisFbo_, kAnalysisSize, kAnalysisSize);

    std::vector<std::uint8_t> pixels(kAnalysisSize * kAnalysisSize * 4);
    glReadPixels(0, 0, kAnalysisSize, kAnalysisSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    std::array<float, kFeatureCount> features = {};
    for (int i = 0; i < kAnalysisSize * kAnalysisSize; ++i) {
        int r = pixels[i * 4 + 0];
        int g = pixels[i * 4 + 1];
        int b = pixels[i * 4 + 2];

        int msbIndex = ((r >> 4) * 16 + (g >> 4)) * 16 + (b >> 4);
        int lsbIndex = ((r & 15) * 16 + (g & 15)) * 16 + (b & 15);

        for (int k = 0; k < kFeatureCount; ++k) {
            features[k] += modelMsb_[msbIndex * kFeatureCount + k];
            features[k] += modelLsb_[lsbIndex * kFeatureCount + k];
        }
    }

    float invCount = 1.0f / static_cast<float>(kAnalysisSize * kAnalysisSize);
    for (float& value : features) {
        value *= invCount;
        value = std::round(value * 2.0f) / 2.0f;
        value = std::max(-16.0f, std::min(15.5f, value));
    }

    std::array<float, kNumBasis> weights = {};
    for (int group = 0; group < kClassifierGroups; ++group) {
        int a = static_cast<int>(features[group * 2 + 0] * 2.0f) + 32;
        int b = static_cast<int>(features[group * 2 + 1] * 2.0f) + 32;
        a = std::max(0, std::min(63, a));
        b = std::max(0, std::min(63, b));
        int clsIndex = a * 64 + b;

        for (int basis = 0; basis < kNumBasis; ++basis) {
            int offset = (group * 64 * 64 + clsIndex) * kNumBasis + basis;
            weights[basis] += (static_cast<float>(classifier_[offset]) - 32.0f) / 4.0f;
        }
    }
    return weights;
}

void IcelutOpenGLProcessor::buildFinalLut(const std::array<float, kNumBasis>& weights) {
    std::fill(finalLut_.begin(), finalLut_.end(), 0.0f);

    const int basisStride = kDim * kDim * kDim * 3;
    for (int basis = 0; basis < kNumBasis; ++basis) {
        const float weight = weights[basis];
        const float* basisData = basisLuts_.data() + basis * basisStride;
        for (int i = 0; i < basisStride; ++i) {
            finalLut_[i] += weight * basisData[i];
        }
    }
}

void IcelutOpenGLProcessor::uploadFinalLut() {
    glBindTexture(GL_TEXTURE_3D, lutTexture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, kDim, kDim, kDim,
                    GL_RGB, GL_FLOAT, finalLut_.data());
}

GLuint IcelutOpenGLProcessor::createOutputTexture(int width, int height) const {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    return texture;
}

void IcelutOpenGLProcessor::drawWithLut(GLuint sourceTexture) {
    glUseProgram(applyProgram_);
    glBindVertexArray(vao_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glUniform1i(glGetUniformLocation(applyProgram_, "uInput"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, lutTexture_);
    glUniform1i(glGetUniformLocation(applyProgram_, "uLut"), 1);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}

}  // namespace icelut_demo
