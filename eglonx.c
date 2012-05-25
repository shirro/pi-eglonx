/* Created by exoticorn ( http://talk.maemo.org/showthread.php?t=37356 )
 * edited and commented by André Bergner [endboss]
 *
 * libraries needed: libx11-dev, libgles2-dev
 *
 */

#include <stdio.h>

#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#define EGL_EGLEXT_PROTOTYPES

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext_brcm.h>
#include <bcm_host.h>


const char vertex_src [] =
" \
attribute vec4 position; \
varying mediump vec2 pos; \
uniform vec4 offset; \
\
void main() \
{ \
	gl_Position = position + offset; \
	pos = position.xy; \
} \
";


const char fragment_src [] =
" \
varying mediump vec2 pos; \
uniform mediump float phase; \
\
void main() \
{ \
	gl_FragColor = vec4( 1., 0.9, 0.7, 1.0 ) * \
	cos( 30.*sqrt(pos.x*pos.x + 1.5*pos.y*pos.y) \
	+ atan(pos.y,pos.x) - phase ); \
} \
";
// some more formulas to play with...
// cos( 20.*(pos.x*pos.x + pos.y*pos.y) - phase );
// cos( 20.*sqrt(pos.x*pos.x + pos.y*pos.y) + atan(pos.y,pos.x) - phase );
// cos( 30.*sqrt(pos.x*pos.x + 1.5*pos.y*pos.y - 1.8*pos.x*pos.y*pos.y)
// + atan(pos.y,pos.x) - phase );


// handle to the shader
void
print_shader_info_log (GLuint shader)
{
	GLint length;

	glGetShaderiv ( shader , GL_INFO_LOG_LENGTH , &length );

	if ( length ) {
		char* buffer = malloc(sizeof(char) * length);
		glGetShaderInfoLog(shader, length, NULL, buffer);
		printf("shader info: %s\n");
		fflush(NULL);
		free(buffer);

		GLint success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (success != GL_TRUE) exit (1);
	}
}


GLuint
load_shader(const char *shader_source, GLenum type)
{
	GLuint shader = glCreateShader(type);

	glShaderSource(shader, 1 ,&shader_source , NULL);
	glCompileShader(shader);

	print_shader_info_log(shader);

	return shader;
}


Display *x_display;
Window win;
EGLDisplay egl_display;
EGLContext egl_context;

GLfloat
norm_x = 0.0,
norm_y = 0.0,
offset_x = 0.0,
offset_y = 0.0,
p1_pos_x = 0.0,
p1_pos_y = 0.0;

GLint
phase_loc,
offset_loc,
position_loc;


EGLSurface egl_surface;
bool update_pos = false;

const float vertexArray[] = {
	0.0, 0.5, 0.0,
	-0.5, 0.0, 0.0,
	0.0, -0.5, 0.0,
	0.5, 0.0, 0.0,
	0.0, 0.5, 0.0 
};


void render()
{
	static float phase = 0;
	static int donesetup = 0;

	static XWindowAttributes gwa;
	static XImage *image = 0;
	static GC gc; 
	static Pixmap pixmap;
	//// draw

	if (!donesetup) {
		XGetWindowAttributes(x_display, win ,&gwa);
		glViewport(0 ,0 , gwa.width, gwa.height);
		glClearColor(0.08, 0.06, 0.07, 1.0); // background color
		donesetup = 1;

		//gc = XCreateGC(x_display, win, 0, 0);
		//printf("d %d, w %d\n", x_display, win);
		//printf("w %d, h %d\n", gwa.width, gwa.height);
		image = XGetImage(x_display, win, 0, 0, gwa.width, gwa.height, AllPlanes, ZPixmap);
		//pixmap	= XCreatePixmap(x_display, win, gwa.width, gwa.height, 16);
		gc = DefaultGC(x_display, 0);
	}
	glClear (GL_COLOR_BUFFER_BIT);

	// memset((void*)(&(image->data[0])), 0, gwa.width * gwa.height);

	glUniform1f(phase_loc, phase); // write the value of phase to the shaders phase
	phase = fmodf( phase + 0.5f, 2.f * 3.141f); // and update the local variable

	if (update_pos) { // if the position of the texture has changed due to user action
		GLfloat old_offset_x = offset_x;
		GLfloat old_offset_y = offset_y;

		offset_x = norm_x - p1_pos_x;
		offset_y = norm_y - p1_pos_y;

		p1_pos_x = norm_x;
		p1_pos_y = norm_y;

		offset_x += old_offset_x;
		offset_y += old_offset_y;

		update_pos = false;
	}

	glUniform4f(offset_loc, offset_x, offset_y, 0.0, 0.0 );

	glVertexAttribPointer(position_loc, 3, GL_FLOAT, false, 0, vertexArray);
	glEnableVertexAttribArray(position_loc);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 5);

	glFinish();
	unsigned int *buffer = (unsigned int *)malloc(gwa.height * gwa.width * 4);
	glReadPixels(0, 0, gwa.width, gwa.height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

	int count;
	for (count = 0; count < gwa.width * gwa.height / 2; count++)
	{
		unsigned int *dest = (unsigned int*)(&(image->data[0]));
		unsigned int src0 = buffer[count * 2];
		unsigned int src1 = buffer[count * 2 + 1];

		unsigned char r0, g0, b0;
		unsigned char r1, g1, b1;

		r0 = src0 & 0xff;
		g0 = (src0 >> 8) & 0xff;
		b0 = (src0 >> 16) & 0xff;
		r1 = src1 & 0xff;
		g1 = (src1 >> 8) & 0xff;
		b1 = (src1 >> 16) & 0xff;
		dest[count] = ((r0 >> 3) << 27) | ((g0 >> 2) << 21) | ((b0 >> 3) << 16)
		| ((r1 >> 3) << 11) | ((g1 >> 2) << 5) | ((b1 >> 3) << 0);
	}


	free(buffer);
	// eglSwapBuffers ( egl_display, egl_surface ); // get the rendered buffer to the screen
	XPutImage(x_display, win, gc, image, 0, 0, 0, 0, gwa.width, gwa.height);
}


////////////////////////////////////////////////////////////////////////////////////////////


int main()
{
	/////// the X11 part //////////////////////////////////////////////////////////////////
	// in the first part the program opens a connection to the X11 window manager
	//

	bcm_host_init();

	x_display = XOpenDisplay(NULL); // open the standard display (the primary screen)
	if (x_display == NULL) {
		fputs("cannot connect to X server\n", stderr);
		return 1;
	}

	Window root = DefaultRootWindow(x_display); // get the root window (usually the whole screen)

	XSetWindowAttributes swa;
	swa.event_mask = ExposureMask | PointerMotionMask | KeyPressMask;

	const int width = 800;
	const int height = 480;
	// create a window with the provided parameters
	win = XCreateWindow (x_display, root, 0, 0, width, height, 0, CopyFromParent, InputOutput,
				CopyFromParent, CWEventMask, &swa );

	/* XSetWindowAttributes xattr;
	Atom atom;
	int one = 1;

	xattr.override_redirect = False;
	XChangeWindowAttributes ( x_display, win, CWOverrideRedirect, &xattr );

	atom = XInternAtom ( x_display, "_NET_WM_STATE_FULLSCREEN", True );
	XChangeProperty (
	x_display, win,
	XInternAtom ( x_display, "_NET_WM_STATE", True ),
	XA_ATOM, 32, PropModeReplace,
	(unsigned char*) &atom, 1 );

	XChangeProperty (
	x_display, win,
	XInternAtom ( x_display, "_HILDON_NON_COMPOSITED_WINDOW", True ),
	XA_INTEGER, 32, PropModeReplace,
	(unsigned char*) &one, 1);

	XWMHints hints;
	hints.input = True;
	hints.flags = InputHint;
	XSetWMHints(x_display, win, &hints);*/

	XMapWindow (x_display, win ); // make the window visible on the screen
	XStoreName (x_display, win, "GL test" ); // give the window a name
	/*
	//// get identifiers for the provided atom name strings
	Atom wm_state = XInternAtom ( x_display, "_NET_WM_STATE", False );
	Atom fullscreen = XInternAtom ( x_display, "_NET_WM_STATE_FULLSCREEN", False );

	XEvent xev;
	memset ( &xev, 0, sizeof(xev) );

	xev.type = ClientMessage;
	xev.xclient.window = win;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 1;
	xev.xclient.data.l[1] = fullscreen;
	XSendEvent ( // send an event mask to the X-server
	x_display,
	DefaultRootWindow ( x_display ),
	False,
	SubstructureNotifyMask,
	&xev );*/


	/////// the egl part //////////////////////////////////////////////////////////////////
	// egl provides an interface to connect the graphics related functionality of openGL ES
	// with the windowing interface and functionality of the native operation system (X11
	// in our case.

	//egl_display = eglGetDisplay( (EGLNativeDisplayType) x_display );
	egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (egl_display == EGL_NO_DISPLAY) {
		fputs("Got no EGL display.\n", stderr);
		return 1;
	}

	if (!eglInitialize( egl_display, NULL, NULL )) {
		fputs("Unable to initialize EGL\n", stderr);
		return 1;
	}

	EGLint attr[] = { // some attributes to set up our egl-interface
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SURFACE_TYPE,
		EGL_PIXMAP_BIT | EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLConfig ecfg;
	EGLint num_config;
	if (!eglChooseConfig(egl_display, attr, &ecfg, 1, &num_config)) {
		fprintf(stderr, "Failed to choose config (eglError: %s)\n");
		return 1;
	}

	if (num_config != 1) {
		fprintf(stderr, "Didn't get exactly one config, but %d\n", num_config);
		return 1;
	}

	//egl_surface = eglCreateWindowSurface ( egl_display, ecfg, win, NULL );

	EGLint pixel_format = EGL_PIXEL_FORMAT_ARGB_8888_BRCM;
	//EGLint pixel_format = EGL_PIXEL_FORMAT_RGB_565_BRCM;
	EGLint rt;
	eglGetConfigAttrib(egl_display, ecfg, EGL_RENDERABLE_TYPE, &rt);

	if (rt & EGL_OPENGL_ES_BIT) {
		pixel_format |= EGL_PIXEL_FORMAT_RENDER_GLES_BRCM;
		pixel_format |= EGL_PIXEL_FORMAT_GLES_TEXTURE_BRCM;
	}
	if (rt & EGL_OPENGL_ES2_BIT) {
		pixel_format |= EGL_PIXEL_FORMAT_RENDER_GLES2_BRCM;
		pixel_format |= EGL_PIXEL_FORMAT_GLES2_TEXTURE_BRCM;
	}
	if (rt & EGL_OPENVG_BIT) {
		pixel_format |= EGL_PIXEL_FORMAT_RENDER_VG_BRCM;
		pixel_format |= EGL_PIXEL_FORMAT_VG_IMAGE_BRCM;
	}
	if (rt & EGL_OPENGL_BIT) {
		pixel_format |= EGL_PIXEL_FORMAT_RENDER_GL_BRCM;
	}

	EGLint pixmap[5];
	pixmap[0] = 0;
	pixmap[1] = 0;
	pixmap[2] = width;
	pixmap[3] = height;
	pixmap[4] = pixel_format;

	eglCreateGlobalImageBRCM(width, height, pixmap[4], 0, width*4, pixmap);

	egl_surface = eglCreatePixmapSurface(egl_display, ecfg, pixmap, 0);
	if ( egl_surface == EGL_NO_SURFACE ) {
		fprintf(stderr, "Unable to create EGL surface (eglError: %s)\n", eglGetError());
		return 1;
	}

	//// egl-contexts collect all state descriptions needed required for operation
	EGLint ctxattr[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	egl_context = eglCreateContext ( egl_display, ecfg, EGL_NO_CONTEXT, ctxattr );
	if ( egl_context == EGL_NO_CONTEXT ) {
		fprintf(stderr, "Unable to create EGL context (eglError: %s)\n", eglGetError());
		return 1;
	}

	//// associate the egl-context with the egl-surface
	eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);


	/////// the openGL part ///////////////////////////////////////////////////////////////

	GLuint vertexShader = load_shader(vertex_src , GL_VERTEX_SHADER); // load vertex shader
	GLuint fragmentShader = load_shader(fragment_src , GL_FRAGMENT_SHADER); // load fragment shader

	GLuint shaderProgram = glCreateProgram (); // create program object
	glAttachShader(shaderProgram, vertexShader); // and attach both...
	glAttachShader(shaderProgram, fragmentShader); // ... shaders to it

	glLinkProgram(shaderProgram); // link the program
	glUseProgram(shaderProgram); // and select it for usage

	//// now get the locations (kind of handle) of the shaders variables
	position_loc = glGetAttribLocation(shaderProgram, "position");
	phase_loc = glGetUniformLocation(shaderProgram , "phase");
	offset_loc = glGetUniformLocation(shaderProgram , "offset");
	if (position_loc < 0 || phase_loc < 0 || offset_loc < 0) {
		fputs("Unable to get uniform location\n", stderr);
		return 1;
	}


	const float
	window_width = 800.0,
	window_height = 480.0;

	//// this is needed for time measuring --> frames per second
	struct timezone tz;
	struct timeval t1, t2;
	gettimeofday(&t1, &tz);
	int num_frames = 0;

	XWindowAttributes gwa;
	XGetWindowAttributes(x_display, win, &gwa);

	bool quit = false;
	while (!quit) { // the main loop

		while (XPending(x_display)) { // check for events from the x-server

			XEvent xev;
			XNextEvent(x_display, &xev);

			if (xev.type == MotionNotify) { // if mouse has moved
				// cout << "move to: << xev.xmotion.x << "," << xev.xmotion.y << endl;
				GLfloat window_y = (xev.xmotion.y) - window_height / 2.0;
				norm_y = window_y / (window_height / 2.0);
				GLfloat window_x = xev.xmotion.x - window_width / 2.0;
				norm_x = window_x / (window_width / 2.0);
				update_pos = true;
			}

			if (xev.type == KeyPress) quit = true;
		}

		render(); // now we finally put something on the screen

		if (++num_frames % 100 == 0) {
			gettimeofday( &t2, &tz );
			float dt = t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) * 1e-6;
			printf("fps: %d\n", num_frames / dt);
			num_frames = 0;
			t1 = t2;
		}
		// usleep( 1000*10 );
	}


	//// cleaning up...
	eglDestroyContext(egl_display, egl_context);
	eglDestroySurface(egl_display, egl_surface);
	eglTerminate(egl_display);
	XDestroyWindow(x_display, win);
	XCloseDisplay(x_display);

	return 0;
}
