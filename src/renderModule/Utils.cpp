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

void Utils::renderCube(const glm::vec3& LTN, const glm::vec3& RBF) {
	static unsigned linesVAO = 0;

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


	//glm::vec3 LTN = { -far,  far, far };
	glm::vec3 RTN = { RBF.x, LTN.y, LTN.z };
	glm::vec3 LBN = { LTN.x, RBF.y, LTN.z };
	glm::vec3 RBN = { RBF.x, RBF.y, LTN.z };

	glm::vec3 LTF = { LTN.x, LTN.y, RBF.z };
	glm::vec3 RTF = { RBF.x, LTN.y, RBF.z };
	glm::vec3 LBF = { LTN.x, RBF.y, RBF.z };
	//glm::vec3 RBF = { far, -far, -far };
	

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
	RenderModule::Renderer::drawArrays(GL_LINES, sizeof(vertices) / 3);
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

void Utils::renderPointLight(float near, float far) {
	static unsigned linesVAO = 0;

	float w = 15.f;
	float h = 10.f;
	float l = 10.f;

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


	glm::vec3 LTN = { -far,  far, far };
	glm::vec3 RTN = { far,  far, far };
	glm::vec3 LBN = { -far, -far, far };
	glm::vec3 RBN = { far, -far, far };

	glm::vec3 LTF = { -far,  far, -far };
	glm::vec3 RTF = { far,  far, -far };
	glm::vec3 LBF = { -far, -far, -far };
	glm::vec3 RBF = { far, -far, -far };


	//small cube inside big
	glm::vec3 LTNs = { -near,  near, near };
	glm::vec3 RTNs = { near,  near, near };
	glm::vec3 LBNs = { -near, -near, near };
	glm::vec3 RBNs = { near, -near, near };

	glm::vec3 LTFs = { -near,  near, -near };
	glm::vec3 RTFs = { near,  near,-near };
	glm::vec3 LBFs = { -near, -near, -near };
	glm::vec3 RBFs = { near, -near,-near };



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
	RenderModule::Renderer::drawArrays(GL_LINES, sizeof(vertices) / 3);
	glBindVertexArray(0);

	glDeleteVertexArrays(1, &linesVAO);
	glDeleteBuffers(1, &cubeVBO);
}

void Utils::renderXYZ(float length) {
	static unsigned linesVAO = 0;
	static float prevLength = 0.f;

	static std::vector<float> vertices = {
		0.f,0.f,0.f,
		0.f,0.f,length, //+z

		0.f,0.f,0.f,
		0.f,length,0.f,//+y

		0.f,0.f,0.f,
		length,0.f,0.f,//+x
	};

	// setup plane VAO
	static unsigned cubeVBO;

	if (length != prevLength) {
		prevLength = length;

		glDeleteVertexArrays(1, &linesVAO);
		glDeleteBuffers(1, &cubeVBO);

		vertices = {
			0.f,0.f,0.f,
			0.f,0.f,length, //+z

			0.f,0.f,0.f,
			0.f,length,0.f,//+y

			0.f,0.f,0.f,
			length,0.f,0.f,//+x
		};

		glGenVertexArrays(1, &linesVAO);
		glGenBuffers(1, &cubeVBO);
		glBindVertexArray(linesVAO);
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);

		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices.front()) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	}
	
	glBindVertexArray(linesVAO);
	RenderModule::Renderer::drawArrays(GL_LINES, 6);
	glBindVertexArray(0);
}
