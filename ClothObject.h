#pragma once

#include <glm/glm.hpp>
#include <webgpu/webgpu.hpp>

#include <ResourceManager.h>
#include <array>
#include <vector>

class ClothObject {
public:
  // (Just aliases to make notations lighter)
  using vec3 = glm::vec3;
  using vec2 = glm::vec2;
  using mat3x3 = glm::mat3x3;

  std::array<wgpu::Buffer, 2> particleBuffers = {nullptr, nullptr};

  wgpu::Buffer m_vertexBuffer = nullptr;
  wgpu::Buffer m_uniformBuffer = nullptr;
  wgpu::ShaderModule m_shaderModule = nullptr;

  wgpu::BindGroupLayout m_bindGroupLayouts[2] = {nullptr, nullptr};

  wgpu::BindGroup m_bindGroup = nullptr;
  wgpu::BindGroup m_vertexBindGroup = nullptr;

  wgpu::PipelineLayout m_pipelineLayout = nullptr;
  wgpu::ComputePipeline m_pipeline = nullptr;

  wgpu::PipelineLayout m_vertexPipelineLayout = nullptr;
  wgpu::ComputePipeline m_vertexPipeline = nullptr;

  int m_bufferSize = 0;

  struct ClothVertex {
    // garbage is necessary for 32 byte blocks
    vec3 position;
    float garbage1;
    vec3 normal;
    float garbage2;
  };

  struct ClothParticle {
    vec3 position;
    float garbage1;
    vec3 velocity;
    float garbage2;
  };

  struct ClothParameters {
    int width = 100;
    int height = 100;
    int particlesPerGroup = 64;

    float scale = 1.0f;
    float massScale = 25.0f;
    float maxStretch = 1.3f;
    float minStretch = 0.1f;

    float sphereRadius = 0.3f;
    float spherePeriod = 150.0f;
    float sphereRange = 2.0f;
    float deltaT = 0.008f;
  };

  struct ClothUniforms {
    float width;
    float height;

    float particleDist;
    float particleMass;
    float particleScale;

    float maxStretch;
    float minStretch;

    float sphereRadius;
    float sphereX;
    float sphereY;
    float sphereZ;

    float deltaT;
    float currentT;
    vec3 garbage;
  };

  ClothParameters parameters = ClothParameters();
  ClothUniforms uniforms = ClothUniforms();

  int numParticles = parameters.width * parameters.height;
  int numVertices = 3 * 2 * (parameters.width - 1) * (parameters.height - 1);
  float totalMass = parameters.scale * parameters.massScale;
  float particleMass = totalMass / numParticles;
  float particleDist = parameters.scale / parameters.height;

  float currentT = 0.0f;
  int frame = 0;
  vec3 sphere_pos = vec3(0.0f, 0.0f, -1.0f);

  void updateParameters(ClothParameters &p);

  void processFrame(wgpu::Device &device);
  void computePass(wgpu::Device &device);

  void updateUniforms(wgpu::Device &device);
  void terminateUniforms();

  void fillBuffer(wgpu::Device &device);
  void initBuffers(wgpu::Device &device);
  void terminateBuffers();

  void initBindGroup(wgpu::Device &device);
  void terminateBindGroups();

  void initBindGroupLayout(wgpu::Device &device);
  void terminateBindGroupLayouts();

  void initComputePipeline(wgpu::Device &device);
  void terminateComputePipeline();

  void initiateNewCloth(ClothParameters &p, wgpu::Device &device);
  void terminateAll();
};
