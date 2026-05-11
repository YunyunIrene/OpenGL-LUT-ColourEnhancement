#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>


using AVM_ULLong = unsigned long long;

namespace icelut_demo {

constexpr int kDim = 33;
constexpr int kNumBasis = 10;
constexpr int kFeatureCount = 10;
constexpr int kClassifierGroups = 5;
constexpr int kNibbleCount = 16 * 16 * 16;
constexpr int kAnalysisSize = 32;

class IcelutOpenGLProcessor {
public:
    explicit IcelutOpenGLProcessor();
    ~IcelutOpenGLProcessor();

    IcelutOpenGLProcessor(const IcelutOpenGLProcessor&) = delete;
    IcelutOpenGLProcessor& operator=(const IcelutOpenGLProcessor&) = delete;

    GLuint processTextureToNewTexture(GLuint sourceTexture, int width, int height);

    void processTextureToTexture(GLuint sourceTexture, GLuint outputTexture, int width, int height);

private:
    void loadAssets(const std::string& rawDir);
    void createGlObjects();
    void renderCopy(GLuint sourceTexture, GLuint framebuffer, int width, int height);
    std::array<float, kNumBasis> computeWeightsFromTexture(GLuint sourceTexture);
    void buildFinalLut(const std::array<float, kNumBasis>& weights);
    void uploadFinalLut();
    GLuint createOutputTexture(int width, int height) const;
    void drawWithLut(GLuint sourceTexture);

    std::vector<float> basisLuts_;
    std::vector<float> modelMsb_;
    std::vector<float> modelLsb_;
    std::vector<std::int8_t> classifier_;
    std::vector<float> finalLut_;

    GLuint vao_ = 0;
    GLuint copyProgram_ = 0;
    GLuint applyProgram_ = 0;
    GLuint analysisFbo_ = 0;
    GLuint outputFbo_ = 0;
    GLuint analysisTexture_ = 0;
    GLuint lutTexture_ = 0;
};

}  // namespace icelut_demo
