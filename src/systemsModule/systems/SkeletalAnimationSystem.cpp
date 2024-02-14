#include "SkeletalAnimationSystem.h"

#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/ModelComponent.h"
#include "core/ECSHandler.h"

void SFE::SystemsModule::SkeletalAnimationSystem::update(float dt) {

    for (auto [entityId, animationComp,  modeComp] : ECSHandler::registry().getComponentsArray<ComponentsModule::AnimationComponent, ModelComponent>()) {
        animationComp->animator.updateAnimation(dt);
        
        auto ourShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer.vs", "shaders/g_buffer.fs");
      
        // don't forget to enable shader before setting uniforms
        ourShader->use();
        auto transforms = animationComp->animator.getFinalBoneMatrices();
        //auto transforms = animationComp->transforms;
        for (int i = 0; i < transforms.size(); ++i) {
            ourShader->setMat4(("finalBonesMatrices[" + std::to_string(i) + "]").c_str(), transforms[i]);
        }
    }

   
}