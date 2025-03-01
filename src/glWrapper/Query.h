﻿#pragma once
#include <cstdint>

#include "glad/glad.h"

namespace SFE::GLW {
	enum class QueryType {
		SAMPLES_PASSED = GL_SAMPLES_PASSED, //glGetQueryObjectuiv for 32-bit, glGetQueryObjectui64v for 64-bit,
		//Returns the number of samples that pass the depth test. The exact size may depend on the maximum number of samples that can be represented, which might require 64-bit integers on systems with very high sample counts.

		ANY_SAMPLES_PASSED = GL_ANY_SAMPLES_PASSED, //glGetQueryObjectuiv
		//Returns whether any samples passed the depth test.This is typically used for occlusion queries to determine if an object is visible.

		ANY_SAMPLES_PASSED_CONSERVATIVE = GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
		PRIMITIVES_GENERATED = GL_PRIMITIVES_GENERATED,  //glGetQueryObjectuiv or glGetQueryObjectui64v for higher counts
		//Returns the number of primitives generated by geometry shaders.This is useful for understanding the workload generated by geometry processing.

		TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN = GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, //glGetQueryObjectuiv or glGetQueryObjectui64v for higher counts
		//Returns the number of primitives generated by geometry shaders.This is useful for understanding the workload generated by geometry processing.

		TIME_ELAPSED =  GL_TIME_ELAPSED, //glGetQueryObjectui64v
		//Measures the time taken to complete all OpenGL commands between the glBeginQuery and glEndQuery calls.The time is measured in nanoseconds.

		TIMESTAMP = GL_TIMESTAMP //glGetQueryObjectui64v
		//Used to query the GPU timestamp before or after a set of commands.The timestamp is useful for measuring the time between events or commands in the GPU pipeline.
	};

	enum class QueryResult {
		QUERY_RESULT = GL_QUERY_RESULT, //params or buffer returns the value of the query object's passed samples counter. The initial value is 0.

		QUERY_RESULT_NO_WAIT = GL_QUERY_RESULT_NO_WAIT, //If the result of the query is available(that is, a query of GL_QUERY_RESULT_AVAILABLE would return non - zero), then params or buffer returns the value of the query object's passed samples counter, otherwise, the data referred to by params or buffer is not modified. The initial value is 0.

		QUERY_RESULT_AVAILABLE = GL_QUERY_RESULT_AVAILABLE, //params or buffer returns whether the passed samples counter is immediately available.If a delay would occur waiting for the query result, GL_FALSE is returned.Otherwise, GL_TRUE is returned, which also indicates that the results of all previous queries are available as well.

		QUERY_TARGET = GL_QUERY_TARGET, //params or buffer returns the query object's target.
	};

	template<size_t Count = 1, QueryType Type = QueryType::SAMPLES_PASSED>
	struct Queries {
		~Queries() {
			glDeleteQueries(Count, ids);
		}

		bool isGenerated(size_t idx = 0) {
			return glIsQuery(ids[idx]);
		}
		void generate() {
			glGenQueries(1, ids);
		}
		
		void begin(size_t idx = 0) {
			glBeginQuery(static_cast<unsigned int>(type), ids[idx]);
		}

		void end() {
			glEndQuery(static_cast<unsigned int>(type));
		}

		void getResult(unsigned int& res, size_t idx = 0, QueryResult resultType = QueryResult::QUERY_RESULT) {
			glGetQueryObjectuiv(ids[idx], static_cast<GLenum>(resultType), &res);
		}

		void getResult(int& res, size_t idx = 0, QueryResult resultType = QueryResult::QUERY_RESULT) {
			glGetQueryObjectiv(ids[idx], static_cast<GLenum>(resultType), &res);
		}

		void getResult(uint64_t& res, size_t idx = 0, QueryResult resultType = QueryResult::QUERY_RESULT) {
			glGetQueryObjectui64v(ids[idx], static_cast<GLenum>(resultType), &res);
		}

		void getResult(int64_t& res, size_t idx = 0, QueryResult resultType = QueryResult::QUERY_RESULT) {
			glGetQueryObjecti64v(ids[idx], static_cast<GLenum>(resultType), &res);
		}

		const QueryType type = Type;
		unsigned int ids[Count]{0};
	};

	template<QueryType Type>
	using Query = Queries<1, Type>;
}