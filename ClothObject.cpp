#include "ClothObject.h"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/polar_coordinates.hpp>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>
#include <imgui.h>

#include <array>
#include <cassert>

using namespace wgpu;
using ClothVertex = ClothObject::ClothVertex;
using ClothParticle = ClothObject::ClothParticle;
using ClothUniforms = ClothObject::ClothUniforms;

constexpr float PI = 3.14159265358979323846f;

void ClothObject::initiateNewCloth(ClothParameters &p, wgpu::Device &device) {
  updateParameters(p);
  initBuffers(device);
  initBindGroupLayout(device);
  initComputePipeline(device);
  initBindGroup(device);

  updateUniforms(device);
  fillBuffer(device);
}

void ClothObject::processFrame(wgpu::Device &device) {
  frame += 1;
  currentT += parameters.deltaT;

  updateUniforms(device);
  // repeat bindgroup inititation to switch which buffer is input and output
  initBindGroup(device);

  computePass(device);
}

void ClothObject::updateParameters(ClothParameters &p) {
  parameters = p;

  numParticles = parameters.width * parameters.height;
  m_bufferSize = numParticles * sizeof(ClothParticle);
  numVertices = 3 * 2 * (parameters.width - 1) * (parameters.height - 1);
  totalMass = parameters.scale * parameters.massScale;
  particleMass = totalMass / numParticles;
  particleDist = parameters.scale / parameters.height;

  currentT = 0;
  frame = 0;

  // update
  uniforms.width = (float)parameters.width;
  uniforms.height = (float)parameters.height;

  uniforms.particleDist = particleDist;
  uniforms.particleMass = particleMass;
  uniforms.particleScale = parameters.scale;

  uniforms.maxStretch = parameters.maxStretch;
  uniforms.minStretch = parameters.minStretch;

  uniforms.sphereRadius = parameters.sphereRadius;
  uniforms.sphereX = 0;
  uniforms.sphereY = 0;
  uniforms.sphereZ = -5;

  uniforms.deltaT = parameters.deltaT;
  uniforms.currentT = currentT;
}

void ClothObject::fillBuffer(wgpu::Device &device) {
  std::vector<ClothParticle> particleData;
  for (int x = 0; x < parameters.width; x++) {
    for (int y = 0; y < parameters.height; y++) {
      ClothParticle particle;
      particle.position = vec3(x * particleDist, y * particleDist, 0.0f);
      particle.velocity = vec3(0.0f, 0.0f, 0.0f);

      particleData.push_back(particle);
    }
  }
  device.getQueue().writeBuffer(particleBuffers[0], 0, particleData.data(),
                                numParticles * sizeof(ClothParticle));
}

void ClothObject::initBuffers(wgpu::Device &device) {
  // Create input/output buffers
  BufferDescriptor bufferDesc;
  bufferDesc.mappedAtCreation = false;
  bufferDesc.size = numParticles * sizeof(ClothParticle);
  bufferDesc.usage = BufferUsage::Storage | BufferUsage::CopyDst;
  particleBuffers[0] = device.createBuffer(bufferDesc);
  particleBuffers[1] = device.createBuffer(bufferDesc);

  // Create vertex buffer
  BufferDescriptor vbufferDesc;
  vbufferDesc.size = numVertices * sizeof(ClothVertex);
  vbufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Storage;
  vbufferDesc.mappedAtCreation = false;
  m_vertexBuffer = device.createBuffer(vbufferDesc);

  BufferDescriptor ubufferDesc;
  ubufferDesc.size = sizeof(ClothUniforms);
  ubufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
  ubufferDesc.mappedAtCreation = false;
  m_uniformBuffer = device.createBuffer(ubufferDesc);
}

void ClothObject::initBindGroupLayout(wgpu::Device &device) {

  // compute group
  std::vector<BindGroupLayoutEntry> bindings(3, Default);

  // The uniform buffer binding
  bindings[0].binding = 0;
  bindings[0].visibility = ShaderStage::Compute;
  bindings[0].buffer.type = BufferBindingType::Uniform;
  bindings[0].buffer.minBindingSize = sizeof(ClothUniforms);

  // Input buffer
  bindings[1].binding = 1;
  bindings[1].buffer.type = BufferBindingType::ReadOnlyStorage;
  bindings[1].visibility = ShaderStage::Compute;

  // Output buffer
  bindings[2].binding = 2;
  bindings[2].buffer.type = BufferBindingType::Storage;
  bindings[2].visibility = ShaderStage::Compute;

  BindGroupLayoutDescriptor bindGroupLayoutDesc;
  bindGroupLayoutDesc.entryCount = (uint32_t)bindings.size();
  bindGroupLayoutDesc.entries = bindings.data();
  m_bindGroupLayouts[0] = device.createBindGroupLayout(bindGroupLayoutDesc);

  // vertex output group

  std::vector<BindGroupLayoutEntry> vBindings(1, Default);
  vBindings[0].binding = 0;
  vBindings[0].visibility = ShaderStage::Compute;
  vBindings[0].buffer.type = BufferBindingType::Storage;

  BindGroupLayoutDescriptor vertexBindGroupLayoutDesc;
  vertexBindGroupLayoutDesc.entryCount = (uint32_t)vBindings.size();
  vertexBindGroupLayoutDesc.entries = vBindings.data();
  m_bindGroupLayouts[1] =
      device.createBindGroupLayout(vertexBindGroupLayoutDesc);
}

void ClothObject::updateUniforms(wgpu::Device &device) {
  uniforms.currentT = currentT;

  // uniforms.sphereX = 0.0f;
  // uniforms.sphereY = 0.0f;
  float sphere_period = (fmod(currentT, parameters.spherePeriod * 2.0f) -
                         parameters.spherePeriod) /
                        parameters.spherePeriod;
  float sphere_sign = sphere_period < 0.0f ? -1.0f : 1.0f;
  uniforms.sphereZ = parameters.sphereRange *
                     (1.0f + sphere_sign * (sphere_period * 2.0f) - 2.0f);

  device.getQueue().writeBuffer(m_uniformBuffer, 0, &uniforms,
                                sizeof(ClothUniforms));
}

void ClothObject::initComputePipeline(wgpu::Device &device) {

  ShaderModule computeShaderModule =
      ResourceManager::loadShaderModule(RESOURCE_DIR "/compute.wgsl", device);

  // Create compute pipeline layout
  PipelineLayoutDescriptor pipelineLayoutDesc;
  pipelineLayoutDesc.bindGroupLayoutCount = 2;
  pipelineLayoutDesc.bindGroupLayouts =
      (WGPUBindGroupLayout *)&m_bindGroupLayouts;
  m_pipelineLayout = device.createPipelineLayout(pipelineLayoutDesc);

  // Create compute pipeline
  ComputePipelineDescriptor computePass1;
  computePass1.compute.constantCount = 0;
  computePass1.compute.constants = nullptr;
  computePass1.compute.entryPoint = "main";
  computePass1.compute.module = computeShaderModule;
  computePass1.layout = m_pipelineLayout;
  m_pipeline = device.createComputePipeline(computePass1);

  ComputePipelineDescriptor computePass2;
  computePass2.compute.constantCount = 0;
  computePass2.compute.constants = nullptr;
  computePass2.compute.entryPoint = "particle_to_vertex";
  computePass2.compute.module = computeShaderModule;
  computePass2.layout = m_pipelineLayout;
  m_vertexPipeline = device.createComputePipeline(computePass2);
}

void ClothObject::initBindGroup(wgpu::Device &device) {
  // Create compute bind group
  std::vector<BindGroupEntry> entries(3, Default);

  entries[0].binding = 0;
  entries[0].buffer = m_uniformBuffer;
  entries[0].offset = 0;
  entries[0].size = sizeof(ClothUniforms);

  // Input buffer
  entries[1].binding = 1;
  entries[1].buffer = particleBuffers[frame % 2];
  entries[1].offset = 0;
  entries[1].size = numParticles * sizeof(ClothParticle);

  // Output buffer
  entries[2].binding = 2;
  entries[2].buffer = particleBuffers[1 - (frame % 2)];
  entries[2].offset = 0;
  entries[2].size = numParticles * sizeof(ClothParticle);

  BindGroupDescriptor bindGroupDesc;
  bindGroupDesc.layout = m_bindGroupLayouts[0];
  bindGroupDesc.entryCount = (uint32_t)entries.size();
  bindGroupDesc.entries = (WGPUBindGroupEntry *)entries.data();
  m_bindGroup = device.createBindGroup(bindGroupDesc);

  std::vector<BindGroupEntry> ventries(1, Default);

  void updateVertexBuffer(wgpu::Device & device);
  ventries[0].binding = 0;
  ventries[0].buffer = m_vertexBuffer;
  ventries[0].offset = 0;
  ventries[0].size = numVertices * sizeof(ClothVertex);

  BindGroupDescriptor vbindGroupDesc;
  bindGroupDesc.layout = m_bindGroupLayouts[1];
  bindGroupDesc.entryCount = (uint32_t)ventries.size();
  bindGroupDesc.entries = (WGPUBindGroupEntry *)ventries.data();
  m_vertexBindGroup = device.createBindGroup(bindGroupDesc);
}

void ClothObject::computePass(wgpu::Device &device) {
  Queue queue = device.getQueue();

  CommandEncoderDescriptor encoderDesc = Default;
  CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

  ComputePassDescriptor computePassDesc;
  computePassDesc.timestampWrites = nullptr;
  ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);

  computePass.setPipeline(m_pipeline);
  computePass.setBindGroup(0, m_bindGroup, 0, nullptr);
  computePass.setBindGroup(1, m_vertexBindGroup, 0, nullptr);

  /*uint32_t invocationCount = m_bufferSize / sizeof(float);
  uint32_t workgroupSize = 64;
  uint32_t workgroupCount =
      (invocationCount + workgroupSize - 1) / workgroupSize;*/
  computePass.dispatchWorkgroups(64, 1, 1);
  computePass.end();

  ComputePassDescriptor computePassDesc2;
  computePassDesc2.timestampWrites = nullptr;
  ComputePassEncoder computePass2 = encoder.beginComputePass(computePassDesc2);

  computePass2.setPipeline(m_vertexPipeline);
  computePass2.setBindGroup(0, m_bindGroup, 0, nullptr);
  computePass2.setBindGroup(1, m_vertexBindGroup, 0, nullptr);

  /*uint32_t invocationCount2 = m_bufferSize / sizeof(float);
  uint32_t workgroupSize2 = 64;
  uint32_t workgroupCount2 =
      (invocationCount2 + workgroupSize2 - 1) / workgroupSize2;*/
  computePass2.dispatchWorkgroups(64, 1, 1);
  computePass2.end();

  CommandBuffer commands = encoder.finish(CommandBufferDescriptor{});
  queue.submit(commands);
}

// -------------- MEMORY TERMINATION ----------------------

void ClothObject::terminateAll() {
  terminateBindGroups();
  terminateUniforms();
  terminateComputePipeline();
  terminateBindGroupLayouts();
  terminateBuffers();
}

void ClothObject::terminateComputePipeline() {
  m_pipeline.release();
  m_shaderModule.release();
}

void ClothObject::terminateBindGroups() {
  m_bindGroup.release();
  m_vertexBindGroup.release();
}

void ClothObject::terminateBindGroupLayouts() {
  for (wgpu::BindGroupLayout &layout : m_bindGroupLayouts) {
    layout.release();
  }
}

void ClothObject::terminateUniforms() {
  m_uniformBuffer.destroy();
  m_uniformBuffer.release();
}

void ClothObject::terminateBuffers() {
  for (wgpu::Buffer &pbuffer : particleBuffers) {
    pbuffer.destroy();
    pbuffer.release();
  }

  m_vertexBuffer.release();
}

// ---------------------------------------------------------------------------------------------------
