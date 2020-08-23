#ifndef ROBOTGL_H
#define ROBOTGL_H

// Include GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

namespace samsRobot{


	//////////////////////////////////////////////////
	static const char* vertex_shader_text =
		"#version 330\n"
		"layout(location=0) in vec3 vertexPosition_modelspace;\n"
		"layout(location=1) in vec3 vertexColor;\n"
		"out vec3 fragmentColor;\n"
		"uniform mat4 MVP;\n"
		"void main()\n"
		"{\n"
		"    gl_Position = MVP * vec4(vertexPosition_modelspace, 1);\n"
		"    fragmentColor = vertexColor;\n"
		"}\n";

	static const char* fragment_shader_text =
		"#version 330\n"
		"in vec3 fragmentColor;\n"
		"out vec3 color;\n"
		"void main()\n"
		"{\n"
		"	color = fragmentColor;"
		"}\n";


	class robotGL{
		private:
			GLFWwindow* window;
			GLuint programID;
			GLuint fragment_shader;
			GLuint vertex_shader;
			GLuint colorbuffer;
			GLuint vertexbuffer;
			GLuint matrixID;
			GLuint vertexArrayID;

			// create the projection and view matrices
			glm::mat4 proj;
			glm::mat4 view;
			glm::mat4 model;
			glm::mat4 MVP;

			// buffer data
			GLfloat* g_vertex_buffer_data;
			GLfloat* g_color_buffer_data;
			int numVertices;

			bool prog_finished;
			float mouse_wh_speed, keys_speed;
			float fps; // track how fast we're updating screen
			
			// use these to compute user interaction
			// Initial position : on +Z
			glm::vec3 position = glm::vec3( 0, 0, 5 ); 
			// Initial horizontal angle : toward -Z
			float horizontalAngle = 3.14f;
			// Initial vertical angle : none
			float verticalAngle = 0.0f;
			// Initial Field of View
			float initialFoV = 45.0f;


		public:
			robotGL();
			~robotGL();

			GLFWwindow* getWindow (void) const;
			int init(void);
			void stop(void);
			void update(void);

			void set_mat(const GLfloat* , const int );
			void set_col(const GLfloat* , const int );
			void set_view(const glm::vec3 cam_pos, const glm::vec3 look_at_dir);
			void set_proj(const float fov, const float asp_ratio);
			void set_bg(const float r, const float g, const float b, const float a);

			GLfloat* get_mat(int &size) const;
			GLfloat* get_col(int &size) const;

			inline float get_keys_speed(void) const {return this->keys_speed;}
			inline float get_mouse_wh_speed(void) const { return this->mouse_wh_speed;}
			glm::mat4 get_view() const;
			inline float get_fps(void){return this->fps;}
			inline bool get_progFinished(void){return this->prog_finished;}

			// from tutorial 06
			void computeMatricesFromInputs(void);

	};
}

#endif