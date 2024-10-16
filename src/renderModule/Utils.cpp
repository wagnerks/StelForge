#include "Utils.h"
#include "mathModule/Utils.h"

#include "glUtils.h"
#include "assetsModule/modelModule/Mesh.h"
#include "assetsModule/modelModule/Vertex.h"
#include "glWrapper/Buffer.h"
#include "mathModule/Quaternion.h"
#include "systemsModule/SystemManager.h"

using namespace SFE::Render;

std::vector<SFE::Math::Vec3>& Utils::getVerticesArray(const Math::Vec4& color, float thickness, GLW::RenderMode renderType) {
	auto it = std::find_if(renderVertices.begin(), renderVertices.end(), [color, thickness, renderType](std::pair<LineData, std::vector<Math::Vec3>>& a) {
		return a.first.color == color && std::fabs(a.first.thickness - thickness) <= std::numeric_limits<float>::epsilon() && a.first.renderType == renderType && renderType != GLW::LINE_LOOP;
	});

	if (it != renderVertices.end()) {
		return it->second;
	}

	renderVertices.push_back(std::make_pair(LineData{ color, thickness, renderType }, std::vector<Math::Vec3>()));
	return renderVertices.back().second;
}

std::vector<Utils::Triangle>& Utils::getTrianglesArray(const TriangleData& data) {
	auto it = std::find_if(renderTriangles.begin(), renderTriangles.end(), [&data](std::pair<TriangleData, std::vector<Triangle>>& a) {
		return a.first == data;
	});

	if (it != renderTriangles.end()) {
		return it->second;
	}

	renderTriangles.push_back(std::make_pair(data, std::vector<Triangle>()));
	return renderTriangles.back().second;
}

void Utils::CalculateEulerAnglesFromView(const Math::Mat4& view, float& yaw, float& pitch, float& roll) {
	if (view[0][0] == 1.0f || view[0][0] == -1.0f) {
		yaw = atan2f(view[0][2], view[2][3]);
		pitch = 0;
		roll = 0;
	}
	else {
		yaw = atan2(-view[2][0], view[0][0]);
		pitch = asin(view[1][0]);
		roll = atan2(-view[1][2], view[1][1]);
	}
}

void Utils::renderQuad() {
	static GLW::VertexArray quadVAO;
	if (quadVAO.getID() == 0) {

		const auto mesh = Mesh<Vertex2D>{
			{
				{ {-1.f,  1.f}, {0.f, 1.f} },
				{ {-1.f, -1.f}, {0.f, 0.f} },
				{ { 1.f,  1.f}, {1.f, 1.f} },
				{ { 1.f, -1.f}, {1.f, 0.f} },
			}
		};

		static GLW::Buffer<GLW::ARRAY_BUFFER, Vertex2D> quadVBO;

		quadVAO.generate();
		quadVAO.bind();
		quadVBO.generate();
		quadVBO.bind();

		quadVBO.allocateData(mesh.vertices);

		quadVAO.addAttribute(0, &Vertex2D::position, false);
		quadVAO.addAttribute(1, &Vertex2D::texCoords, false);
		quadVAO.bindDefault();
		quadVBO.unbind();
	}

	drawVertices(GLW::TRIANGLE_STRIP, quadVAO.getID(), 4);
}

void Utils::renderQuad(float x1 = -1.f, float y1 = -1.f, float x2 = 1.f, float y2 = 1.f) {
	Mesh<Vertex3D> mesh { {
			// positions        // texture Coords
		{{x1, y2, 0.0f}, { 0.0f, 1.0f }},
		{ {x1, y1, 0.0f}, {0.0f, 0.0f} },
		{ {x2, y2, 0.0f}, {1.0f, 1.0f} },
		{ {x2, y1, 0.0f}, {1.0f, 0.0f} },
	}};

	GLW::VertexArray quadVAO;
	GLW::Buffer<GLW::ARRAY_BUFFER, Vertex3D> quadVBO;
	quadVAO.generate();
	quadVAO.bind();
	quadVBO.generate();
	quadVBO.bind();
	quadVBO.allocateData(mesh.vertices);
	quadVAO.addAttribute(0, &Vertex3D::position, false);
	quadVAO.addAttribute(1, &Vertex3D::texCoords, false);

	drawMesh(GLW::TRIANGLE_STRIP, mesh);

	quadVAO.bindDefault();
	quadVBO.unbind();
}

void Utils::renderLine(const Math::Vec3& begin, const Math::Vec3& end, const Math::Vec4& color, float thickness) {
	getVerticesArray(color, thickness, GLW::LINES).emplace_back(begin);
	getVerticesArray(color, thickness, GLW::LINES).emplace_back(end);
}

std::pair<std::vector<SFE::Vertex3D>, std::vector<Utils::Triangle>> generateSphere(float radius, int segments) {
	std::vector<SFE::Vertex3D> vertices;
	std::vector<Utils::Triangle> triangles;

	// Generate vertices
	for (int i = 0; i <= segments; ++i) {
		for (int j = 0; j <= segments; ++j) {
			float theta = static_cast<float>(i) / segments * 2 * SFE::Math::pi<float>(); // Azimuthal angle
			float phi = static_cast<float>(j) / segments * SFE::Math::pi<float>(); // Polar angle

			float x = radius * std::sin(phi) * std::cos(theta);
			float y = radius * std::cos(phi);
			float z = radius * std::sin(phi) * std::sin(theta);

			vertices.emplace_back(SFE::Math::Vec3{ x, y, z });
		}
	}

	// Generate triangles
	for (int i = 0; i < segments; ++i) {
		for (int j = 0; j < segments; ++j) {
			int v1 = i * (segments + 1) + j;
			int v2 = v1 + 1;
			int v3 = (i + 1) * (segments + 1) + j;
			int v4 = v3 + 1;

			triangles.push_back({ vertices[v1].position, vertices[v3].position, vertices[v2].position, {1.f} });
			triangles.push_back({ vertices[v2].position, vertices[v3].position, vertices[v4].position, {1.f} });
		}
	}

	return { vertices, triangles };
}

void Utils::renderBone(const Math::Vec3& begin, const Math::Vec3& end, const Math::Vec4& color, const Math::Quaternion<float>& cameraRotation, const Math::Quaternion<float>& boneRotation) {
	static const auto boneScale = scale(Math::Mat4(1.f), Math::Vec3(1.f));

	auto directionVec = end - begin;
	const auto boneLength = (directionVec).length(); 
	directionVec = directionVec.normalize();
	
	const auto lengthToStart = boneLength / 10.f;
	const float radius = 0.02f * boneLength;
	const float boneRad = 0.05f * boneLength;

	auto rotation = boneRotation;
	rotation.changeAxis({ 0.f,1.f,0.f });

	const auto transform = Math::translate(Math::Mat4(1.f), begin) * Math::Quaternion(directionVec, Math::Vec3(0.f, 1.f, 0.f)).toMat4();
	const auto rotationMat = rotation.toMat4();

	const auto center = Math::Vec3(0.f, lengthToStart,0.f);
	auto A = center + Math::Vec3(-boneRad, 0.f, boneRad);
	auto B = center + Math::Vec3(boneRad, 0.f, boneRad);
	auto C = center + Math::Vec3(boneRad, 0.f, -boneRad);
	auto D = center + Math::Vec3(-boneRad, 0.f, -boneRad);

	A = transform * rotationMat * A;
	B = transform * rotationMat * B;
	C = transform * rotationMat * C;
	D = transform * rotationMat * D;
	
	auto& arr = getTrianglesArray({ false, false, true });
	arr.emplace_back( begin, B, A, color);
	arr.emplace_back( begin, C, B, color);
	arr.emplace_back( begin, D, C, color);
	arr.emplace_back( begin, A,D, color);

	arr.emplace_back( end, A, B, color);
	arr.emplace_back( end, D, A, color);
	arr.emplace_back( end, C, D, color);
	arr.emplace_back( end, B, C, color);

	renderCircleFilled(begin, cameraRotation, boneScale, radius, color, 32);
	renderCircleFilled(end, cameraRotation, boneScale, radius, color, 32);
}

void Utils::renderTriangle(const Triangle& triangle) {
	getTrianglesArray({}).emplace_back(triangle);
}

void Utils::renderTriangle(const Triangle& triangle, const Math::Mat4& transform, const Math::Vec4& color) {
	renderTriangle({
		transform * triangle.A.position,
		transform * triangle.B.position,
		transform * triangle.C.position,
		color
	});
}

void Utils::renderCube(const Math::Vec3& LTN, const Math::Vec3& RBF, const Math::Mat4& rotate, const Math::Vec3& pos, const Math::Vec4& color) {
	//		LTF*------------*RTF
	//		 / |           /|
	//      /  |          / |
	//     /   |         /  |
	// LTN*-----------*RTN  |
	//    |    |        |   |
	//    |    |        |   |
	//    |    *LBF-----|---*RBF
	//    |   /         |  /
	//    |  /          | /
	//    | /           |/
	// LBN*-------------*RBN   


	//Math::Vec3 LTN = { -far,  far, far };
	Math::Vec3 RTN = { RBF.x, LTN.y, LTN.z };
	Math::Vec3 LBN = { LTN.x, RBF.y, LTN.z };
	Math::Vec3 RBN = { RBF.x, RBF.y, LTN.z };

	Math::Vec3 LTF = { LTN.x, LTN.y, RBF.z };
	Math::Vec3 RTF = { RBF.x, LTN.y, RBF.z };
	Math::Vec3 LBF = { LTN.x, RBF.y, RBF.z };
	//Math::Vec3 RBF = { far, -far, -far };
	

	float vertices[] = {
		//far cube
		//backward
		LTN.x, LTN.y, LTN.z, //start
		RTN.x, RTN.y, RTN.z, //end

		LTN.x, LTN.y, LTN.z, //start
		LBN.x, LBN.y, LBN.z, //end

		LBN.x, LBN.y, LBN.z, //start
		RBN.x, RBN.y, RBN.z, //end

		RBN.x, RBN.y, RBN.z, //start
		RTN.x, RTN.y, RTN.z, //end
		//forward
		LTF.x, LTF.y, LTF.z, //start
		RTF.x, RTF.y, RTF.z, //end

		LTF.x, LTF.y, LTF.z, //start
		LBF.x, LBF.y, LBF.z, //end

		LBF.x, LBF.y, LBF.z, //start
		RBF.x, RBF.y, RBF.z, //end

		RBF.x, RBF.y, RBF.z, //start
		RTF.x, RTF.y, RTF.z, //end
		//right
		RTN.x, RTN.y, RTN.z, //start
		RTF.x, RTF.y, RTF.z, //end

		RBN.x, RBN.y, RBN.z, //start
		RBF.x, RBF.y, RBF.z, //end
		//left
		LTN.x, LTN.y, LTN.z, //start
		LTF.x, LTF.y, LTF.z, //end

		LBN.x, LBN.y, LBN.z, //start
		LBF.x, LBF.y, LBF.z, //end
	};

	// Create a rotation matrix (e.g., rotate 45 degrees around the Y-axis)
	const Math::Mat4 transform = Math::translate(Math::Mat4(1.0f), Math::Vec3(pos)) * Math::Mat4(rotate);
	auto& vertArray = getVerticesArray(color, 1.f, GLW::LINES);

	// Apply the rotation to the cube vertices
	for (int i = 0; i < sizeof(vertices) / sizeof(float); i += 3) {
		Math::Vec4 vertex(vertices[i], vertices[i + 1], vertices[i + 2], 1.0f);

		// Apply the model and rotation transformations
		vertex = transform * vertex;
		vertArray.emplace_back(vertex);
	}
}

void Utils::renderQuad(const Math::Vec3& min, const Math::Vec3& max, const Math::Mat4& rotate, const Math::Vec3& pos, const Math::Vec4& color) {
	//   A         max
	//    *---------*
	//    |         |
	//    |         |
	//    |         |
	//min *---------* B

	const Math::Vec3 A = { min.x, max.y, min.z };
	const Math::Vec3 B = { max.x, min.y, max.z };

	Triangle vertices[] = {
		{min, A, max, color},
		{min, max, B, color}
	};

	// Create a rotation matrix (e.g., rotate 45 degrees around the Y-axis)
	const Math::Mat4 transform = Math::translate(Math::Mat4(1.0f), Math::Vec3(pos)) * Math::Mat4(rotate);
	auto& vertArray = getTrianglesArray({});

	// Apply the rotation to the cube vertices
	for (int i = 0; i < sizeof(vertices) / sizeof(Triangle); i++) {
		auto& tr = vertices[i];
		tr.A.color = color;
		tr.B.color = color;
		tr.C.color = color;
		// Apply the model and rotation transformations
		tr.A.position = transform * tr.A.position;
		tr.B.position = transform * tr.B.position;
		tr.C.position = transform * tr.C.position;
		vertArray.emplace_back(tr);
	}
}

void Utils::renderCubeMesh(const Math::Vec3& LTN, const Math::Vec3& RBF, const Math::Mat4& rotate, const Math::Vec3& pos, const Math::Vec4& color) {
	//		LTF*------------*RTF
	//		 / |           /|
	//      /  |          / |
	//     /   |         /  |
	// LTN*-----------*RTN  |
	//    |    |        |   |
	//    |    |        |   |
	//    |    *LBF-----|---*RBF
	//    |   /         |  /
	//    |  /          | /
	//    | /           |/
	// LBN*-------------*RBN   


	//Math::Vec3 LTN = { -far,  far, far };
	Math::Vec3 RTN = { RBF.x, LTN.y, LTN.z };
	Math::Vec3 LBN = { LTN.x, RBF.y, LTN.z };
	Math::Vec3 RBN = { RBF.x, RBF.y, LTN.z };

	Math::Vec3 LTF = { LTN.x, LTN.y, RBF.z };
	Math::Vec3 RTF = { RBF.x, LTN.y, RBF.z };
	Math::Vec3 LBF = { LTN.x, RBF.y, RBF.z };
	//Math::Vec3 RBF = { far, -far, -far };


	Triangle vertices[] = {
		{RTN, LTN,  LBN, color},
		{RBN, RTN, LBN, color},

		{LBN, LTN, LBF, color},
		{LBF, LTN, LTF, color},

		{LTF, LTN,  RTN, color},
		{RTF, LTF,   RTN, color},

		{RBF, RTF, RBN, color},
		{RBN, RTF, RTN, color},

		{LTF, RTF, LBF, color},
		{LBF, RTF, RBF, color},

		{LBN, LBF,  RBN, color},
		{LBF, RBF,  RBN, color},
	};

	// Create a rotation matrix (e.g., rotate 45 degrees around the Y-axis)
	const Math::Mat4 transform = Math::translate(Math::Mat4(1.0f), Math::Vec3(pos)) * Math::Mat4(rotate);
	auto& vertArray = getTrianglesArray({false, true, true});

	// Apply the rotation to the cube vertices
	for (int i = 0; i < sizeof(vertices) / sizeof(Triangle); i++) {
		auto& tr = vertices[i];
		tr.A.color = color;
		tr.B.color = color;
		tr.C.color = color;

		// Apply the model and rotation transformations
		tr.A.position = transform * tr.A.position;
		tr.B.position = transform * tr.B.position;
		tr.C.position = transform * tr.C.position;
		vertArray.emplace_back(tr);
	}
}

void Utils::renderCapsule(const Math::Vec3& start, const Math::Vec3& end, float radius) {
	int segments = 100; // Adjust the number of segments as needed.

	// Calculate the capsule's axis and length
	Math::Vec3 axis = end - start;
	//float length = Math::distance(start, end);

	// Initialize the vertices array for the capsule.
	std::vector<float> vertices;

	for (int i = 0; i <= segments; ++i) {
		float theta = (Math::twoPi<float>() * i) / segments;
		for (int j = 0; j <= segments; ++j) {
			float phi = (Math::pi<float>() * j) / segments;

			Math::Vec3 vertex;
			vertex.x = radius * sinf(phi) * cosf(theta);
			vertex.y = radius * sinf(phi) * sinf(theta);
			vertex.z = radius * cosf(phi);

			// Apply the orientation and position to the vertex
			//vertex = Math::rotate(orientation, vertex);
			vertex += start + axis * 0.5f;

			vertices.push_back(vertex.x);
			vertices.push_back(vertex.y);
			vertices.push_back(vertex.z);
		}
	}

	// Generate and bind a VAO
	unsigned capsuleVAO;
	glGenVertexArrays(1, &capsuleVAO);
	glBindVertexArray(capsuleVAO);

	// Generate a VBO and buffer the vertex data
	unsigned capsuleVBO;
	glGenBuffers(1, &capsuleVBO);
	glBindBuffer(GL_ARRAY_BUFFER, capsuleVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

	// Configure the vertex attribute
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	// Draw the capsule
	GLW::drawVertices(GLW::LINES, capsuleVAO, vertices.size() / 3);


	// Deallocate resources (typically done at the end of your program, not immediately)
	glDeleteVertexArrays(1, &capsuleVAO);
	glDeleteBuffers(1, &capsuleVBO);
}

void Utils::renderSphere(const Math::Vec3& center, float radius) {
	int segments = 10; // Adjust the number of segments as needed.

	auto& vertArray = getVerticesArray(Math::Vec4(1.f, 1.f, 1.f, 1.f), 1.f, GLW::LINE_LOOP);

	// Initialize the vertices array for the sphere.
	for (int i = 0; i <= segments; ++i) {
		float theta = (Math::twoPi<float>() * i) / segments;
		for (int j = 0; j <= segments; ++j) {
			float phi = (Math::pi<float>() * j) / segments;

			Math::Vec3 vertex;
			vertex.x = center.x + radius * sinf(phi) * cosf(theta);
			vertex.y = center.y + radius * sinf(phi) * sinf(theta);
			vertex.z = center.z + radius * cosf(phi);

			vertArray.emplace_back(vertex);
		}
	}
}

void Utils::renderCircle(const Math::Vec3& pos, const Math::Quaternion<float>& quat, const Math::Mat4& scale, float radius, const Math::Vec4& color, int numSegments, float lineThicness, GLW::RenderMode renderType) {
	auto& vertArray = getVerticesArray(color, lineThicness, renderType);

	auto transform = Math::translate(Math::Mat4{1.f}, pos)* quat.toMat4() * scale;

	for (int i = 0; i < numSegments; ++i) {
		float theta = 2.0f * Math::pi<float>() * static_cast<float>(i) / static_cast<float>(numSegments);

		float x = radius * std::cos(theta);
		float y = radius * std::sin(theta);
		
		vertArray.push_back(transform * Math::Vec3{ x, y, 0.f});
	}

	//add GL_LINE_LOOP
}

void Utils::renderCircleFilled(const Math::Vec3& pos, const Math::Quaternion<float>& quat, const Math::Mat4& scale, float radius, const Math::Vec4& color, int numSegments, float startAngle, float delta) {
	auto& vertArray = getTrianglesArray({});

	auto transform = Math::translate(Math::Mat4{1.f}, pos) * quat.toMat4() * scale;
	for (int i = 0; i < numSegments; i++) {
		Triangle tr;
		tr.A.color = color;
		tr.B.color = color;
		tr.C.color = color;

		float theta = Math::radians(startAngle + delta * static_cast<float>(i) / static_cast<float>(numSegments));
		tr.A.position.x = radius * std::cos(theta);
		tr.A.position.y = radius * std::sin(theta);
		tr.A.position = transform * tr.A.position;

		theta = Math::radians(startAngle + delta * static_cast<float>(i + 1) / static_cast<float>(numSegments));
		tr.B.position.x = radius * std::cos(theta);
		tr.B.position.y = radius * std::sin(theta);
		tr.B.position = transform * tr.B.position;

		tr.C.position = pos;

		vertArray.push_back(tr);
	}
}

void Utils::renderCone(const Math::Vec3& pos, const Math::Quaternion<float>& quat, const Math::Mat4& scale,	float radius, float height, const Math::Vec4& color, int numSegments) {
	auto& vertArray = getTrianglesArray({});

	auto transform = Math::translate(Math::Mat4{1.f}, pos) * quat.toMat4() * scale;

	for (int i = 0; i < numSegments; i++) {
		Triangle tr;
		tr.A.color = color;
		tr.B.color = color;
		tr.C.color = color;

		float theta = Math::radians(360.f * static_cast<float>(i) / static_cast<float>(numSegments));
		tr.A.position.x = radius * std::cos(theta);
		tr.A.position.z = radius * std::sin(theta);
		tr.A.position = transform * tr.A.position;
		
		theta = Math::radians(360.f * static_cast<float>(i + 1) / static_cast<float>(numSegments));
		tr.B.position.x = radius * std::cos(theta);
		tr.B.position.z = radius * std::sin(theta);
		tr.B.position = transform * tr.B.position;

		tr.C.position = Math::Vec3{0.f};
		tr.C.position = transform * tr.C.position;
		tr.recalculateNormal();
		vertArray.push_back(tr);

		Triangle tr2;
		tr2.A = tr.A;
		tr2.B = tr.B;
		tr2.C.position = Math::Vec3{ 0.f };
		tr2.C.position.y += height;
		tr2.C.position = transform * tr2.C.position;

		tr2.A.color = color;
		tr2.B.color = color;
		tr2.C.color = color;
		vertArray.push_back(tr2);
	}
}

void Utils::renderCamera() {
	float w = 15.f;
	float h = 10.f;
	float l = 10.f;

	Math::Vec3 A = { -w, -h, -l };
	Math::Vec3 B = { w, -h, -l };
	Math::Vec3 C = { -w, h, -l };
	Math::Vec3 D = { w, h, -l };

	w = 5.f;
	h = 5.f;
	l = 0.f;

	Math::Vec3 E = { -w, h, l };
	Math::Vec3 F = { w, h, l };
	Math::Vec3 G = { w, -h, l };
	Math::Vec3 H = { -w, -h, l };

	auto mesh = Mesh<Vertex3D>{
	{
		{ A }, { B },
		{ B }, { D },
		{ C }, { D },
		{ A }, { C },

		{ C }, { E },

		{ A }, { H },
		{ D }, { F },

		{ B }, { G },
		{ H }, { E },

		{ H }, { G },
		{ F }, { E },
		{ G }, { F },
		{{0.f, C.y, C.z}} , {{0.f, C.y + 5.f, C.z}}
		}
	};

	GLW::VertexArray vao;
	GLW::Buffer<GLW::ARRAY_BUFFER, Vertex3D> vbo;

	vao.generate();
	vao.bind();
	vbo.generate();
	vbo.bind();
	vbo.allocateData(mesh.vertices);
	vao.addAttribute(0, &Vertex3D::position, false);

	drawMesh(GLW::LINES, mesh);

	vao.bindDefault();
	vbo.unbind();
}

void Utils::renderPointLight(float near, float far, const Math::Vec3& pos) {
	//    LTF*-----------*RTF
	//      /|          /|
	//     / |         / |
	// LTN*-----------*RTN
	//    |  |        |  |
	//    |  |        |  |
	//    |  *LBF-----|--*RBF
	//    | /         | /
	//    |/          |/
	// LBN*-----------*RBN   


	Math::Vec3 LTN = { -far,  far, far };
	Math::Vec3 RTN = { far,  far, far };
	Math::Vec3 LBN = { -far, -far, far };
	Math::Vec3 RBN = { far, -far, far };

	Math::Vec3 LTF = { -far,  far, -far };
	Math::Vec3 RTF = { far,  far, -far };
	Math::Vec3 LBF = { -far, -far, -far };
	Math::Vec3 RBF = { far, -far, -far };


	//small cube inside big
	Math::Vec3 LTNs = { -near,  near, near };
	Math::Vec3 RTNs = { near,  near, near };
	Math::Vec3 LBNs = { -near, -near, near };
	Math::Vec3 RBNs = { near, -near, near };

	Math::Vec3 LTFs = { -near,  near, -near };
	Math::Vec3 RTFs = { near,  near,-near };
	Math::Vec3 LBFs = { -near, -near, -near };
	Math::Vec3 RBFs = { near, -near,-near };



	float vertices[] = {
		//near cube
		//backward
		LTNs.x, LTNs.y, LTNs.z, //start
		RTNs.x, RTNs.y, RTNs.z, //end

		LTNs.x, LTNs.y, LTNs.z, //start
		LBNs.x, LBNs.y, LBNs.z, //end

		LBNs.x, LBNs.y, LBNs.z, //start
		RBNs.x, RBNs.y, RBNs.z, //end

		RBNs.x, RBNs.y, RBNs.z, //start
		RTNs.x, RTNs.y, RTNs.z, //end
		//forward
		LTFs.x, LTFs.y, LTFs.z, //start
		RTFs.x, RTFs.y, RTFs.z, //end

		LTFs.x, LTFs.y, LTFs.z, //start
		LBFs.x, LBFs.y, LBFs.z, //end

		LBFs.x, LBFs.y, LBFs.z, //start
		RBFs.x, RBFs.y, RBFs.z, //end

		RBFs.x, RBFs.y, RBFs.z, //start
		RTFs.x, RTFs.y, RTFs.z, //end
		//right
		RTNs.x, RTNs.y, RTNs.z, //start
		RTFs.x, RTFs.y, RTFs.z, //end

		RBNs.x, RBNs.y, RBNs.z, //start
		RBFs.x, RBFs.y, RBFs.z, //end
		//left
		LTNs.x, LTNs.y, LTNs.z, //start
		LTFs.x, LTFs.y, LTFs.z, //end

		LBNs.x, LBNs.y, LBNs.z, //start
		LBFs.x, LBFs.y, LBFs.z, //end

		//far cube
		//backward
		LTN.x, LTN.y, LTN.z, //start
		RTN.x, RTN.y, RTN.z, //end

		LTN.x, LTN.y, LTN.z, //start
		LBN.x, LBN.y, LBN.z, //end

		LBN.x, LBN.y, LBN.z, //start
		RBN.x, RBN.y, RBN.z, //end

		RBN.x, RBN.y, RBN.z, //start
		RTN.x, RTN.y, RTN.z, //end
		//forward
		LTF.x, LTF.y, LTF.z, //start
		RTF.x, RTF.y, RTF.z, //end

		LTF.x, LTF.y, LTF.z, //start
		LBF.x, LBF.y, LBF.z, //end

		LBF.x, LBF.y, LBF.z, //start
		RBF.x, RBF.y, RBF.z, //end

		RBF.x, RBF.y, RBF.z, //start
		RTF.x, RTF.y, RTF.z, //end
		//right
		RTN.x, RTN.y, RTN.z, //start
		RTF.x, RTF.y, RTF.z, //end

		RBN.x, RBN.y, RBN.z, //start
		RBF.x, RBF.y, RBF.z, //end
		//left
		LTN.x, LTN.y, LTN.z, //start
		LTF.x, LTF.y, LTF.z, //end

		LBN.x, LBN.y, LBN.z, //start
		LBF.x, LBF.y, LBF.z, //end

		//diagonals
		LTNs.x, LTNs.y, LTNs.z, //start
		LTN.x, LTN.y, LTN.z, //end
		LTFs.x, LTFs.y, LTFs.z, //start
		LTF.x, LTF.y, LTF.z, //end

		RTNs.x, RTNs.y, RTNs.z, //start
		RTN.x, RTN.y, RTN.z, //end
		RTFs.x, RTFs.y, RTFs.z, //start
		RTF.x, RTF.y, RTF.z, //end

		RBNs.x, RBNs.y, RBNs.z, //start
		RBN.x, RBN.y, RBN.z, //end
		RBFs.x, RBFs.y, RBFs.z, //start
		RBF.x, RBF.y, RBF.z, //end

		LBNs.x, LBNs.y, LBNs.z, //start
		LBN.x, LBN.y, LBN.z, //end
		LBFs.x, LBFs.y, LBFs.z, //start
		LBF.x, LBF.y, LBF.z, //end
	};

	const Math::Mat4 transform = Math::translate(Math::Mat4(1.0f), Math::Vec3(pos));
	auto& vertArray = getVerticesArray(Math::Vec4(1.f, 1.f, 1.f, 1.f), 1.f, GLW::LINES);
	for (int i = 0; i < sizeof(vertices) / sizeof(float); i += 3) {
		Math::Vec3 vertex(vertices[i], vertices[i + 1], vertices[i + 2]);
		
		vertex = transform * vertex;
		vertArray.emplace_back(vertex);
	}
}
