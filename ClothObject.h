#pragma once

#include <webgpu/webgpu.hpp>
#include <glm/glm.hpp>

#include <vector>
#include <ResourceManager.h>

class ClothObject {
public:
	// (Just aliases to make notations lighter)
	using vec3 = glm::vec3;
	using vec2 = glm::vec2;
	using mat3x3 = glm::mat3x3;

    wgpu::Buffer m_inputParticles = nullptr;
	wgpu::Buffer m_outputParticles = nullptr;

	wgpu::Buffer m_vertexBuffer = nullptr;
    wgpu::Buffer m_uniformBuffer = nullptr; 

	struct ClothVertex {
		vec3 position;
		vec3 normal; // N = local Z axis
        //garbage is necessary for 32 byte blocks
        float garbage1;
        float garbage2;
	};
	struct ClothParticle {
		vec3 position;
		vec3 velocity; 
	};

    struct ClothParameters{
        int width = 100;
        int height = 100; 
        int particlesPerGroup = 64;

        float scale = 1.0f;
        float massScale = 25.0f;
        float maxStretch = 1.3f;
        float minStretch = 0.3f;

        float sphereRadius = 0.3f;
        float spherePeriod = 150.0f;
        float sphereRange = 2.0f;
    }

    struct ClothUniforms{
        float particleMass;
        float particleDist;
        float maxStretch;
        float minStretch;

        float sphereRadius;
        float spherePeriod;
        float sphereRange;
    }
    
    ClothParameters parameters = ClothParameters();
    ClothUniforms uniforms = ClothUniforms();

    int numParticles = parameters.width * parameters.height;
    int numVertices = 3 * 2 * (parameters.width - 1) * (parameters.height - 1);
    float totalMass = parameters.scale * parameters.massScale;
    float particleMass = totalMass / numParticles;
    float particleDist = parameters.scale / parameters.height;

    void updateParameters(ClothParameters& p, wgpu::Queue& queue);

    void initBuffers();

    void fillBuffer(wgpu::Queue& queue);

    void computePass();

    void renderPass();

    void terminate();

};
