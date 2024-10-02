#include "ClothObject.h"
#include "ResourceManager.h"

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/polar_coordinates.hpp>

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include <iostream>
#include <cassert>
#include <filesystem>
#include <sstream>
#include <string>
#include <array>

using namespace wgpu;
using VertexAttributes = ResourceManager::VertexAttributes;
using ClothVertex = ClothObject::ClothVertex;
using ClothParticle = ClothObject::ClothParticle;
using ClothUniforms = ClothObject::ClothUniforms;

constexpr float PI = 3.14159265358979323846f;

void ClothObject::updateParameters(ClothParameters& p, wgpu::Queue& queue){
    parameters = p;

    numParticles = parameters.width * parameters.height;
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

    initBuffers();
    fillBuffer(queue);
}

void ClothObject::fillBuffer(wgpu::Queue& queue){

    initBuffers();

    std::vector<ClothParticle> particleData;
    for(int x = 0; x < width; x++){
        for(int y = 0; y < height; y++){
            particleData.push_back(ClothParticle(vec3(x * particleDist, y * particleDist, 0.0f), vec3(0.0f,0.0f,0.0f)));
        }
    }
	queue.writeBuffer(m_inputParticles, 0, particleData.data(), numParticles * sizeof(ClothParticle));
}

void ClothObject::initBuffers(wgpu::Device m_device){
    // Create input/output buffers
	BufferDescriptor bufferDesc;
	bufferDesc.mappedAtCreation = false;
	bufferDesc.size = numParticles * sizeof(ClothParticle);

	bufferDesc.usage = BufferUsage::Storage | BufferUsage::CopyDst;
	m_inputParticles = m_device.createBuffer(bufferDesc);

	bufferDesc.usage = BufferUsage::Storage | BufferUsage::CopySrc;
	m_outputParticles = m_device.createBuffer(bufferDesc);

	// Create vertex buffer
	BufferDescriptor vbufferDesc;
	vbufferDesc.size = numVertices * sizeof(ClothVertex);
	vbufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
	vbufferDesc.mappedAtCreation = false;
	m_vertexBuffer = m_device.createBuffer(vbufferDesc);

	BufferDescriptor ubufferDesc;
	ubufferDesc.size = sizeof(ClothUniforms);
	ubufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	ubufferDesc.mappedAtCreation = false;
	m_uniformBuffer = m_device.createBuffer(ubufferDesc);
}

void ClothObject::computePass(){

}

bool ClothObject::initBindGroupLayout() {

    //compute group
	std::vector<BindGroupLayoutEntry> bindings(3, Default);

	// The uniform buffer binding
	bindings[0].binding = 0;
	bindings[0].visibility = ShaderStage::Compute;
	bindings[0].buffer.type = BufferBindingType::Uniform;
	bindings[0].buffer.minBindingSize = sizeof(ClothUniforms);

    // Input buffer
    bindings[1].binding = 0;
    bindings[1].buffer.type = BufferBindingType::ReadOnlyStorage;
    bindings[1].visibility = ShaderStage::Compute;

    // Output buffer
    bindings[2].binding = 1;
    bindings[2].buffer.type = BufferBindingType::Storage;
    bindings[2].visibility = ShaderStage::Compute;

    BindGroupLayoutDescriptor bindGroupLayoutDesc;
    bindGroupLayoutDesc.entryCount = (uint32_t)bindings.size();
    bindGroupLayoutDesc.entries = bindings.data();
    m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);

    //vertex output group

	std::vector<BindGroupLayoutEntry> vBindings(1, Default);
	vBindings[0].binding = 0;
	vBindings[0].visibility = ShaderStage::Compute;
	vBindings[0].buffer.type = BufferBindingType::Vertex;

    BindGroupLayoutDescriptor vertexBindGroupLayoutDesc;
    VertexBindGroupLayoutDesc.entryCount = (uint32_t)vBindings.size();
    VertexBindGroupLayoutDesc.entries = vBindings.data();
    m_vertexBindGroupLayout = m_device.createBindGroupLayout(vertexBindGroupLayoutDesc);
}

void updateVertexBuffer(wgpu::device m_device){
    frame += 1;
    currentT += deltaT;
    updateUniforms();
    
    queue.writeBuffer(m_uniformBuffer, 0, &uniforms, sizeof(ClothUniforms));

	CommandEncoderDescriptor encoderDesc = Default;
	CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);

	// Create compute pass
	ComputePassDescriptor computePassDesc;
	computePassDesc.timestampWrites = nullptr;
	ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);

}

void updateUniforms(){
    uniforms.currentT = currentT;
    
    //uniforms.sphereX = 0.0f;
    //uniforms.sphereY = 0.0f;
    float sphere_period = (currentT % parameters.spherePeriod * 2.0f32) - parameters.spherePeriod) / parameters.spherePeriod;
    float sphere_sign = sphere_period < 0.0f32 ? -1.0f32 : 1.0f32;
    uniforms.sphereZ = parameters.sphereRange * (1.0f32 + sphere_sign * (sphere_period * 2.0f32) - 2.0f32);
}

void initComputePipeline(){

	ShaderModule computeShaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/compute-shader.wgsl", m_device);

	// Create compute pipeline layout
	PipelineLayoutDescriptor pipelineLayoutDesc;
	pipelineLayoutDesc.bindGroupLayoutCount = 1;
	pipelineLayoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&m_bindGroupLayout;
	m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutDesc);

	// Create compute pipeline
	ComputePipelineDescriptor computePipelineDesc;
	computePipelineDesc.compute.constantCount = 0;
	computePipelineDesc.compute.constants = nullptr;
	computePipelineDesc.compute.entryPoint = "computeStuff";
	computePipelineDesc.compute.module = computeShaderModule;
	computePipelineDesc.layout = m_pipelineLayout;
	m_pipeline = m_device.createComputePipeline(computePipelineDesc);
}

void Application::initBindGroup(wgpu::Device m_device) {
	// Create compute bind group
	std::vector<BindGroupEntry> entries(2, Default);

	// Input buffer
	entries[0].binding = 0;
	entries[0].buffer = m_inputBuffer;
	entries[0].offset = 0;
	entries[0].size = m_bufferSize;

	// Output buffer
	entries[1].binding = 1;
	entries[1].buffer = m_outputBuffer;
	entries[1].offset = 0;
	entries[1].size = m_bufferSize;

	BindGroupDescriptor bindGroupDesc;
	bindGroupDesc.layout = m_bindGroupLayout;
	bindGroupDesc.entryCount = (uint32_t)entries.size();
	bindGroupDesc.entries = (WGPUBindGroupEntry*)entries.data();
	m_bindGroup = m_device.createBindGroup(bindGroupDesc);
}