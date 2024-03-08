#pragma once
#include "assetsModule/modelModule/Mesh.h"
#include "glWrapper/Draw.h"

namespace SFE::Render {

	template<typename VertexType>
	void drawMesh(GLW::RenderMode mode, Mesh<VertexType> mesh, unsigned vao) {
		GLW::drawVertices(mode, vao, mesh.vertices.size(), mesh.indices.size());
	}

	template<typename VertexType>
	void drawMesh(GLW::RenderMode mode, Mesh<VertexType> mesh) {
		GLW::drawVerticesW(mode, mesh.vertices.size(), mesh.indices.size());
	}
}
