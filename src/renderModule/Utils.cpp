#include "Utils.h"

#include "mat4x4.hpp"
#include "Renderer.h"

using namespace Engine::RenderModule;

void Utils::CalculateEulerAnglesFromView(const glm::mat4& view, float& yaw, float& pitch, float& roll) {
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
	static unsigned quadVAO = 0;
	if (quadVAO == 0) {
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
			1.0f, -1.f, 0.0f, 1.0f, 0.0f,
		};
		// setup plane VAO
		unsigned quadVBO;
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	RenderModule::Renderer::drawArrays(GL_TRIANGLE_STRIP, 4);
	glBindVertexArray(0);
}

void Utils::renderQuad(float x1, float y1, float x2, float y2) {
	unsigned quadVAO;
	unsigned quadVBO;

	float quadVertices[] = {
		// positions        // texture Coords
		x1, y2, 0.0f, 0.0f, 1.0f,
		x1, y1, 0.0f, 0.0f, 0.0f,
		x2, y2, 0.0f, 1.0f, 1.0f,
		x2, y1, 0.0f, 1.0f, 0.0f,
	};

	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

	glBindVertexArray(quadVAO);
	RenderModule::Renderer::drawArrays(GL_TRIANGLE_STRIP, 4);
	glBindVertexArray(0);

	glDeleteVertexArrays(1, &quadVAO);
	glDeleteBuffers(1, &quadVBO);
}

void Utils::renderCube() {
	initCubeVAO();

	glBindVertexArray(cubeVAO);
	RenderModule::Renderer::drawArrays(GL_TRIANGLES, 36);
	glBindVertexArray(0);
}

void Utils::initCubeVAO() {
	if (cubeVAO == 0) {
		float vertices[] = {
			// back face
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
			// front face
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			// left face
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			// right face
			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
			 // bottom face
			 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			  1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
			  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			 -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			 // top face
			 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			  1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			  1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
			  1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			 -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
		};
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		// link vertex attributes
		glBindVertexArray(cubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

	}
}

void Utils::renderLine(glm::vec3& begin, glm::vec3& end) {
	static unsigned linesVAO = 0;

	float vertices[] = {
		begin.x,begin.y,begin.z,
		end.x,end.y,end.z
	};

	// setup plane VAO
	unsigned cubeVBO;
	glGenVertexArrays(1, &linesVAO);
	glGenBuffers(1, &cubeVBO);
	glBindVertexArray(linesVAO);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	glBindVertexArray(linesVAO);
	RenderModule::Renderer::drawArrays(GL_LINES, 2);
	glBindVertexArray(0);

	glDeleteVertexArrays(1, &linesVAO);
	glDeleteBuffers(1, &cubeVBO);
}

void Utils::renderCamera() {
	static unsigned linesVAO = 0;
	float w = 15.f;
	float h = 10.f;
	float l = 10.f;

	glm::vec3 A = { -w, -h, -l };
	glm::vec3 B = { w, -h, -l };
	glm::vec3 C = { -w, h, -l };
	glm::vec3 D = { w, h, -l };

	w = 5.f;
	h = 5.f;
	l = 0.f;

	glm::vec3 E = { -w, h, l };
	glm::vec3 F = { w, h, l };
	glm::vec3 G = { w, -h, l };
	glm::vec3 H = { -w, -h, l };

	float vertices[] = {
		A.x, A.y, A.z, //start
		B.x, B.y, B.z, //end

		B.x, B.y, B.z, //start
		D.x, D.y, D.z, //end

		C.x, C.y, C.z, //start
		D.x, D.y, D.z, //end

		A.x, A.y, A.z, //start
		C.x, C.y, C.z, //end

		C.x, C.y, C.z, //start
		E.x, E.y, E.z, //end

		A.x, A.y, A.z, //start
		H.x, H.y, H.z, //end

		D.x, D.y, D.z, //start
		F.x, F.y, F.z, //end

		B.x, B.y, B.z, //start
		G.x, G.y, G.z, //end

		H.x, H.y, H.z, //end
		E.x, E.y, E.z, //start

		H.x, H.y, H.z, //start
		G.x, G.y, G.z, //end

		F.x, F.y, F.z, //end
		E.x, E.y, E.z, //start

		G.x, G.y, G.z, //start
		F.x, F.y, F.z, //end

		0.f, C.y, C.z,
		0.f, C.y + 5.f, C.z
	};

	// setup plane VAO
	unsigned cubeVBO;
	glGenVertexArrays(1, &linesVAO);
	glGenBuffers(1, &cubeVBO);
	glBindVertexArray(linesVAO);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	glBindVertexArray(linesVAO);
	RenderModule::Renderer::drawArrays(GL_LINES, 26);
	glBindVertexArray(0);

	glDeleteVertexArrays(1, &linesVAO);
	glDeleteBuffers(1, &cubeVBO);
}

void Utils::renderXYZ(float length) {
	static unsigned linesVAO = 0;

	float vertices[] = {
		0.f,0.f,0.f,
		0.f,0.f,length, //+z

		0.f,0.f,0.f,
		0.f,length,0.f,//+y

		0.f,0.f,0.f,
		length,0.f,0.f,//+x
	};

	// setup plane VAO
	unsigned cubeVBO;
	glGenVertexArrays(1, &linesVAO);
	glGenBuffers(1, &cubeVBO);
	glBindVertexArray(linesVAO);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	glBindVertexArray(linesVAO);
	RenderModule::Renderer::drawArrays(GL_LINES, 6);
	glBindVertexArray(0);

	glDeleteVertexArrays(1, &linesVAO);
	glDeleteBuffers(1, &cubeVBO);
}
