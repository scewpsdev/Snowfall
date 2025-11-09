#include "ScreenQuad.h"

#include "math/Vector.h"


extern SDL_Window* window;
extern SDL_GPUDevice* device;
extern SDL_GPUTexture* swapchain;


static const vec2 vertices[] = {
	vec2(-3, -1),
	vec2(1, -1),
	vec2(1, 3)
};


void InitScreenQuad(ScreenQuad* quad, SDL_GPUCommandBuffer* cmdBuffer)
{
	VertexBufferLayout bufferLayout = {};
	bufferLayout.numAttributes = 1;
	bufferLayout.attributes[0].location = 0;
	bufferLayout.attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	quad->vertexBuffer = CreateVertexBuffer(3, &bufferLayout, (uint8_t*)vertices, sizeof(vertices), cmdBuffer);

	quad->shader = LoadGraphicsShader("res/shaders/screenquad.vs.glsl.bin", "res/shaders/screenquad.fs.glsl.bin");

	SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.vertex_shader = quad->shader->vertex;
	pipelineInfo.fragment_shader = quad->shader->fragment;
	pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

	SDL_GPUColorTargetDescription targetDescriptions[1];
	targetDescriptions[0] = {};
	targetDescriptions[0].format = SDL_GetGPUSwapchainTextureFormat(device, window);
	targetDescriptions[0].blend_state.enable_blend = true;
	targetDescriptions[0].blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
	targetDescriptions[0].blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
	targetDescriptions[0].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
	targetDescriptions[0].blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	targetDescriptions[0].blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
	targetDescriptions[0].blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

	pipelineInfo.target_info.num_color_targets = 1;
	pipelineInfo.target_info.color_target_descriptions = targetDescriptions;

	SDL_GPUVertexAttribute attributes[1];
	attributes[0].buffer_slot = 0;
	attributes[0].location = 0;
	attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	attributes[0].offset = 0;

	pipelineInfo.vertex_input_state.num_vertex_attributes = 1;
	pipelineInfo.vertex_input_state.vertex_attributes = attributes;

	SDL_GPUVertexBufferDescription bufferDescriptions[1];
	bufferDescriptions[0].slot = 0;
	bufferDescriptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	bufferDescriptions[0].instance_step_rate = 0;
	bufferDescriptions[0].pitch = sizeof(vec3);

	pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
	pipelineInfo.vertex_input_state.vertex_buffer_descriptions = bufferDescriptions;

	quad->pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);

	SDL_GPUSamplerCreateInfo samplerInfo = {};
	quad->sampler = SDL_CreateGPUSampler(device, &samplerInfo);
}

void DestroyScreenQuad(ScreenQuad* quad)
{
	DestroyVertexBuffer(quad->vertexBuffer);
	DestroyShader(quad->shader);

	SDL_ReleaseGPUSampler(device, quad->sampler);

	SDL_ReleaseGPUGraphicsPipeline(device, quad->pipeline);
}

void RenderScreenQuad(ScreenQuad* quad, SDL_GPUTexture* texture, SDL_GPUCommandBuffer* cmdBuffer)
{
	SDL_assert(quad->pipeline);

	SDL_GPUColorTargetInfo colorTargetInfo = {};
	colorTargetInfo.clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
	colorTargetInfo.load_op = SDL_GPU_LOADOP_LOAD;
	colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
	colorTargetInfo.texture = swapchain;

	SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuffer, &colorTargetInfo, 1, nullptr);

	SDL_BindGPUGraphicsPipeline(renderPass, quad->pipeline);

	SDL_GPUBufferBinding bufferBindings[1];
	bufferBindings[0].buffer = quad->vertexBuffer->buffer;
	bufferBindings[0].offset = 0;

	SDL_BindGPUVertexBuffers(renderPass, 0, bufferBindings, 1);

	SDL_GPUTextureSamplerBinding binding = {};
	binding.texture = texture;
	binding.sampler = quad->sampler;

	SDL_BindGPUFragmentSamplers(renderPass, 0, &binding, 1);

	SDL_DrawGPUPrimitives(renderPass, 6, 1, 0, 0);

	SDL_EndGPURenderPass(renderPass);
}
