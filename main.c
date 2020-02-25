#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define WINDOWS
#include <windows.h>
#endif
#include <SDL2/SDL.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <GL/gl.h>
#include <assert.h>
#include <time.h>

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER                0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER                  0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS                 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS                    0x8B82
#endif

static SDL_Window *window;
static void die(char const *format, ...) {
	char buf[256];
	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof buf, format, args);
	va_end(args);
	
	if (window) {
		if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "An error occured", buf, window) == 0) {
			exit(EXIT_FAILURE);
			return;
		}
	}
	fprintf(stderr, "%s\n", buf);
	exit(EXIT_FAILURE);
}
static void die_nogui(char const *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

typedef GLuint (*GLCreateShader)(GLenum shaderType);
typedef void (*GLShaderSource)(GLuint shader, GLsizei count, const char *const *string, const GLint *length);
typedef void (*GLCompileShader)(GLuint shader);
typedef void (*GLGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
typedef void (*GLGetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei *length, char *infoLog);
typedef GLuint (*GLCreateProgram)(void);
typedef void (*GLAttachShader)(GLuint program, GLuint shader);
typedef void (*GLLinkProgram)(GLuint program);
typedef void (*GLGetProgramiv)(GLuint program, GLenum pname, GLint *params);
typedef void (*GLGetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length, char *infoLog);
typedef void (*GLUseProgram)(GLuint program);
typedef void (*GLDeleteProgram)(GLuint program);
typedef void (*GLDeleteShader)(GLuint shader);
typedef void (*GLUniform1f)(GLint location, float v0);
typedef GLint (*GLGetUniformLocation)(GLuint program, char const *name);

static GLCreateShader glCreateShader;
static GLShaderSource glShaderSource;
static GLCompileShader glCompileShader;
static GLGetShaderiv glGetShaderiv;
static GLGetShaderInfoLog glGetShaderInfoLog;
static GLCreateProgram glCreateProgram;
static GLAttachShader glAttachShader;
static GLLinkProgram glLinkProgram;
static GLGetProgramiv glGetProgramiv;
static GLGetProgramInfoLog glGetProgramInfoLog;
static GLUseProgram glUseProgram;
static GLDeleteProgram glDeleteProgram;
static GLDeleteShader glDeleteShader;
static GLUniform1f glUniform1f;
static GLGetUniformLocation glGetUniformLocation;

static GLuint compile_shader(char const *code, GLenum shader_type) {
	GLuint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, &code, NULL);
	glCompileShader(shader);
	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		char log[256] = {0};
		glGetShaderInfoLog(shader, sizeof log - 1, NULL, log);
		die_nogui("Error compiling shader: %s.", log);
	}
	return shader;
}

static GLuint link_program(GLuint *shaders, size_t nshaders) {
	GLuint program = glCreateProgram();
	for (size_t i = 0; i < nshaders; ++i) {
		glAttachShader(program, shaders[i]);
	}
	glLinkProgram(program);
	GLint status = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		char log[256] = {0};
		glGetProgramInfoLog(program, sizeof log - 1, NULL, log);
		die_nogui("Error linking shader: %s.", log);
	}
	return program;
}

static inline int randnum(void) {
	return rand();
}
#define RANDNUM_MAX RAND_MAX

static void gen_expr(char *buf, size_t complexity) {
	assert(!*buf);
	int which = randnum() % 5;
	if (complexity == 0 || which == 0) {
		/* constant */
		which = randnum() % 4;
		switch (which) {
		case 0:
			*buf = 'x';
			break;
		case 1:
			*buf = 'y';
			break;
		case 2:
			*buf = 't';
			break;
		case 3:
			sprintf(buf, "%.2f", (float)randnum() / (1.0f+(float)RANDNUM_MAX));
			break;
		}
		return;
	}
	switch (which) {
	case 0: assert(0); break;
	case 1:
	case 2: {
		/* unary */
		char const *op = NULL;
		which = randnum() % 4;
		switch (which) {
		case 0:
			op = "sqrt(abs";
			break;
		case 1:
			op = "sin";
			break;
		case 2:
			op = "tan";
			break;
		case 3:
			op = "exp";
			break;
		}
		size_t len = strlen(op);
		memcpy(buf, op, len);
		buf += len;
		*buf++ = '(';
		gen_expr(buf, complexity-1); buf += strlen(buf);
		*buf++ = ')';
		if (which == 0) {
			*buf++ = ')';
		}
	} break;
	case 3:
	case 4: {
		/* binary */
	    char op = '\0';
		which = randnum() % 4;
		switch (which) {
		case 0:
			op = '+';
			break;
		case 1:
			op = '-';
			break;
		case 2:
			op = '/';
			break;
		case 3:
			op = '*';
			break;
		}

		*buf++ = '(';
	    gen_expr(buf, complexity-1); buf += strlen(buf);
		*buf++ = op;
		gen_expr(buf, complexity-1); buf += strlen(buf);
		*buf++ = ')';
	} break;
	}
}

static Uint32 start_time;
static GLint t_location;
static void generate_new_art(GLuint *vertex_shader, GLuint *fragment_shader, GLuint *program) {
	glDeleteShader(*vertex_shader);
	glDeleteShader(*fragment_shader);
	glDeleteProgram(*program);

	start_time = SDL_GetTicks();
	
	char const *const vert_code = "#version 110\n"
		"varying vec2 pos;\n"
		"void main() {\n"
		"   pos = gl_Vertex.xy;\n"
		"	gl_Position = gl_Vertex;\n"
		"}";
	char const *const frag_code_header = "#version 110\n"
		"varying vec2 pos;\n"
		"uniform float t;\n"
		"void main() {\n"
		"	float x = pos.x;\n"
		"	float y = pos.y;\n"
		"	gl_FragColor = vec4(mod(";

	char const *const frag_code_footer = ", 1.0), 1.0);\n"
		"}";

	static char frag_code[1L<<20] = {0};
	memset(frag_code, 0, sizeof frag_code);
	strcpy(frag_code, frag_code_header);
	char *formula = frag_code + strlen(frag_code);
	gen_expr(formula, 15);
    formula += strlen(formula);
	strcpy(formula, ", 1.0), mod(");
	formula += strlen(formula);
	
	gen_expr(formula, 15);
	formula += strlen(formula);
	strcpy(formula, ", 1.0), mod(");
	formula += strlen(formula);

	gen_expr(formula, 15);
    formula += strlen(formula);
	
	strcat(frag_code, frag_code_footer);
	*vertex_shader = compile_shader(vert_code, GL_VERTEX_SHADER);
	*fragment_shader = compile_shader(frag_code, GL_FRAGMENT_SHADER);
	GLuint shaders[] = {*vertex_shader, *fragment_shader};
	*program = link_program(shaders, 2);

	t_location = glGetUniformLocation(*program, "t");

}

int main(void) {
	unsigned seed = (unsigned)time(NULL);
	printf("SEED: %u\n",seed);
	srand(seed);
	GLuint vertex_shader = 0, fragment_shader = 0, program = 0;
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		die("Could not initialize SDL.");
	}
	window = SDL_CreateWindow("AutoArt", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
	if (!window) {
		die("Could not create window: %s.", SDL_GetError());
	}
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	if (!gl_context) {
		die("Could not create OpenGL context: %s.", SDL_GetError());
	}

	glCreateShader = (GLCreateShader)SDL_GL_GetProcAddress("glCreateShader");
	glCreateProgram = (GLCreateProgram)SDL_GL_GetProcAddress("glCreateProgram");
	glShaderSource = (GLShaderSource)SDL_GL_GetProcAddress("glShaderSource");
	glCompileShader = (GLCompileShader)SDL_GL_GetProcAddress("glCompileShader");
	glGetShaderiv = (GLGetShaderiv)SDL_GL_GetProcAddress("glGetShaderiv");
	glGetShaderInfoLog = (GLGetShaderInfoLog)SDL_GL_GetProcAddress("glGetShaderInfoLog");
	glCreateProgram = (GLCreateProgram)SDL_GL_GetProcAddress("glCreateProgram");
	glAttachShader = (GLAttachShader)SDL_GL_GetProcAddress("glAttachShader");
	glLinkProgram = (GLLinkProgram)SDL_GL_GetProcAddress("glLinkProgram");
	glGetProgramiv = (GLGetProgramiv)SDL_GL_GetProcAddress("glGetProgramiv");
	glGetProgramInfoLog = (GLGetProgramInfoLog)SDL_GL_GetProcAddress("glGetProgramInfoLog");
	glUseProgram = (GLUseProgram)SDL_GL_GetProcAddress("glUseProgram");
	glDeleteShader = (GLDeleteShader)SDL_GL_GetProcAddress("glDeleteShader");
	glDeleteProgram = (GLDeleteProgram)SDL_GL_GetProcAddress("glDeleteProgram");
	glUniform1f = (GLUniform1f)SDL_GL_GetProcAddress("glUniform1f");
	glGetUniformLocation = (GLGetUniformLocation)SDL_GL_GetProcAddress("glGetUniformLocation");

	if (!glCreateShader || !glCreateProgram || !glShaderSource || !glGetShaderiv || !glGetShaderInfoLog
		|| !glUseProgram || !glDeleteShader || !glDeleteProgram || !glUniform1f || !glGetUniformLocation) {
		die("Could not get necessary GL procedures. Your OpenGL is probably outdated.");
	}

	generate_new_art(&vertex_shader, &fragment_shader, &program);
	bool fullscreen = false;
	while (1) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				return 0;
			case SDL_KEYUP:
				switch (event.key.keysym.sym) {
				case SDLK_r:
					generate_new_art(&vertex_shader, &fragment_shader, &program);
					break;
				case SDLK_f:
					fullscreen = !fullscreen;
					SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP  : 0);
					break;
				}
			}
		}

		int width, height;
		SDL_GetWindowSize(window, &width, &height);

		glViewport(0, 0, width, height);
		
		glClearColor(0,0,0,1);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(program);
		glUniform1f(t_location, (float)(SDL_GetTicks() - start_time) / 100000.0f);
		glBegin(GL_QUADS);
		glColor3f(1.0f, 1.0f, 1.0f);
		glVertex2f(-1.0f, -1.0f);
		glVertex2f(-1.0f, +1.0f);
		glVertex2f(+1.0f, +1.0f);
		glVertex2f(+1.0f, -1.0f);
		glEnd();
		
		SDL_GL_SwapWindow(window);
	}
	
	return 0;
}

#ifdef WINDOWS
int WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR     lpCmdLine,
  int       nShowCmd) {
  main();
  return 0;
}
#endif
