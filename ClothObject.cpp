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

    uniforms.maxStretch = parameters.maxStretch;
    uniforms.minStretch = parameters.minStretch;
    uniforms.particleDist = particleDist;
    uniforms.particleMass = particleMass;
    uniforms.spherePeriod = parameters.spherePeriod;
    uniforms.sphereRadius = parameters.sphereRadius;
    uniforms.sphereRange = parameters.sphereRange;

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
    queue.writeBuffer(m_uniformBuffer, 0, &uniforms, sizeof(ClothUniforms))
}

void ClothObject::initBuffers(){
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