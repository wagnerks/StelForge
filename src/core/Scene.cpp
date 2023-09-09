#include "Scene.h"

#include "Camera.h"
#include "Engine.h"
#include "debugModule/ComponentsDebug.h"
#include "modelModule/MeshFactory.h"
#include "shaderModule/ShaderController.h"
#include "renderModule/TextureHandler.h"
#include "renderModule/Utils.h"

#include <ext/quaternion_trigonometric.hpp>
#include <gtx/quaternion.hpp>
#include <detail/type_quat.hpp>
#include <ext/matrix_transform.hpp>
#include <ext/quaternion_trigonometric.hpp>
#include <gtx/quaternion.hpp>
#include "mat4x4.hpp"
#include "ModelLoader.h"

using namespace GameModule::CoreModule;

void Scene::init() {

}

void Scene::updateScene(float dt) {

}

void Scene::drawScene() {

}

Scene::~Scene() {
}
