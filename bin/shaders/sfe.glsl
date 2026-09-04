#ifndef SFE_GLSL
#define SFE_GLSL

#ifdef VULKAN
#define SFE_SET(setIdx, bindingIdx) set = setIdx, binding = bindingIdx
#define SFE_PUSH_BLOCK layout(push_constant) uniform
#define SFE_INSTANCE_ID gl_InstanceIndex
#define SFE_VERTEX_ID gl_VertexIndex
#else
#define SFE_SET(setIdx, bindingIdx) binding = bindingIdx
#define SFE_PUSH_BLOCK layout(std140, binding = 15) uniform
#define SFE_INSTANCE_ID gl_InstanceID
#define SFE_VERTEX_ID gl_VertexID
#endif

#endif
