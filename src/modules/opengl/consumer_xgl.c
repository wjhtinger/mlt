#define GL_GLEXT_PROTOTYPES

#include <GL/gl.h>
#include <GL/glext.h>

#include <GL/glx.h>

#include <X11/keysym.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include <framework/mlt.h>

#include "mlt_glsl.h"

#define STARTWIDTH 640
#define STARTHEIGHT 480


extern int XInitThreads();

 
enum KEYCODES { WK_1, WK_2, WK_3, WK_4, WK_UP, WK_DOWN, WK_LEFT, WK_RIGHT,
	WK_SPACE, WK_PRIOR, WK_NEXT, WK_Q, WK_Z, WK_W, WK_S, WK_D, WK_A };


typedef struct consumer_xgl_s *consumer_xgl;

struct consumer_xgl_s
{
	struct mlt_consumer_s parent;
	mlt_properties properties;
	mlt_deque queue;
	pthread_t thread;
	int joined;
	int running;
	int playing;
	int xgl_started;
};


typedef struct
{
	pthread_t thread;
	int running;
} thread_video;


typedef struct
{
	int width;
	int height;
	double aspect_ratio;
	GLuint texture;
	pthread_mutex_t mutex;
	int new;
} frame_new;


typedef struct
{
	int width;
	int height;
	GLuint fbo;
	GLuint texture;
} fbo;


typedef struct
{
	Display *dpy;
    int screen;
    Window win;
    GLXContext ctx;
} HiddenContext;


typedef struct
{
    Display *dpy;
    int screen;
    Window win;
    GLXContext ctx;
    XSetWindowAttributes attr;
    int x, y;
    unsigned int width, height;
    unsigned int depth;
} GLWindow;


typedef unsigned int (*MLT_GLSL_TEXTURE)( void* );
MLT_GLSL_TEXTURE mlt_glsl_texture;
	
static GLWindow GLWin;
static HiddenContext hiddenctx;
static char keys[256];
static int keyBindings[20];

static frame_new new_frame;
static fbo fb;
static int mlt_has_glsl;
static thread_video vthread;
static consumer_xgl xgl;



static void* video_thread( void *arg );



static void initKeys()
{
	keyBindings[WK_1] =  	XKeysymToKeycode(GLWin.dpy, XK_1);
	keyBindings[WK_2] =  	XKeysymToKeycode(GLWin.dpy, XK_2);
	keyBindings[WK_3] =  	XKeysymToKeycode(GLWin.dpy, XK_3);
	keyBindings[WK_4] =  	XKeysymToKeycode(GLWin.dpy, XK_4);
	keyBindings[WK_UP] = 	XKeysymToKeycode(GLWin.dpy, XK_Up);
	keyBindings[WK_DOWN] =  XKeysymToKeycode(GLWin.dpy, XK_Down);
	keyBindings[WK_LEFT] =  XKeysymToKeycode(GLWin.dpy, XK_Left);
	keyBindings[WK_RIGHT] = XKeysymToKeycode(GLWin.dpy, XK_Right);
	keyBindings[WK_SPACE] = XKeysymToKeycode(GLWin.dpy, XK_space);
	keyBindings[WK_Z] =  	XKeysymToKeycode(GLWin.dpy, XK_Z);
	keyBindings[WK_Q] = 	XKeysymToKeycode(GLWin.dpy, XK_Q);
	keyBindings[WK_W] = 	XKeysymToKeycode(GLWin.dpy, XK_W);
	keyBindings[WK_S] = 	XKeysymToKeycode(GLWin.dpy, XK_S);
	keyBindings[WK_D] =  	XKeysymToKeycode(GLWin.dpy, XK_D);
	keyBindings[WK_A] = 	XKeysymToKeycode(GLWin.dpy, XK_A);
	keyBindings[WK_PRIOR] = XKeysymToKeycode(GLWin.dpy,XK_Prior);
	keyBindings[WK_NEXT] = 	XKeysymToKeycode(GLWin.dpy, XK_Next);
}



static void hidden_make_current( void *user_data )
{
	HiddenContext *h = (HiddenContext*)user_data;
	glXMakeCurrent( h->dpy, h->win, h->ctx );
	fprintf(stderr, "hidden_make_current\n");
}

static void hidden_done_current( void *user_data )
{
	HiddenContext *h = (HiddenContext*)user_data;
	glXMakeCurrent( h->dpy, None, NULL );
	fprintf(stderr, "hiddenDoneCurrent\n");
}



static void update()
{
	int _width = GLWin.width;
	int _height = GLWin.height;
	GLfloat left, right, top, bottom, u, u1, v, v1;

	u = 0.0;
	v = 0.0;
	u1 = fb.width;
	v1 = fb.height;

	GLfloat war = (GLfloat)_width/(GLfloat)_height;

	if ( war < new_frame.aspect_ratio ) {
		left = -1.0;
		right = 1.0;
		top = war / new_frame.aspect_ratio;
		bottom = -war / new_frame.aspect_ratio;
	}
	else {
		top = 1.0;
		bottom = -1.0;
		left = -new_frame.aspect_ratio / war;
		right = new_frame.aspect_ratio / war;
	}

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glLoadIdentity();

	glPushMatrix();

	glTranslatef( _width/2, _height/2, 0 );
	glScalef( _width/2, _height/2, 1.0 );

	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, fb.texture );

	glBegin( GL_QUADS );
		glTexCoord2f( u, v );
		glVertex3f( left, top, 0.);
		glTexCoord2f( u, v1 );
		glVertex3f( left, bottom, 0.);
		glTexCoord2f( u1, v1 );
		glVertex3f( right, bottom, 0.);
		glTexCoord2f( u1, v );
		glVertex3f( right, top, 0.);
	glEnd();

	glPopMatrix();
	
	glXSwapBuffers( GLWin.dpy, GLWin.win );
	
	if ( !vthread.running && mlt_has_glsl ) {
		pthread_create( &vthread.thread, NULL, video_thread, NULL );
		vthread.running = 1;
	}
}



static void show_frame()
{
	fprintf(stderr,"show_frame threadID : %ld\n", syscall(SYS_gettid));
	
	glXMakeCurrent( GLWin.dpy, GLWin.win, GLWin.ctx );
	
	if ( (fb.width != new_frame.width) || (fb.height != new_frame.height) ) {
		glDeleteFramebuffers( 1, &fb.fbo );
		glDeleteTextures( 1, &fb.texture );
		fb.fbo = 0;
		fb.width = new_frame.width;
		fb.height = new_frame.height;
		glGenFramebuffers( 1, &fb.fbo );
		glGenTextures( 1, &fb.texture );
		glBindTexture( GL_TEXTURE_RECTANGLE_ARB, fb.texture );
		glTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, fb.width, fb.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		glTexParameterf( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameterf( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glBindFramebuffer( GL_FRAMEBUFFER, fb.fbo );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, fb.texture, 0 );
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	glPushAttrib(GL_VIEWPORT_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
	
	glBindFramebuffer( GL_FRAMEBUFFER, fb.fbo );
	
	glViewport( 0, 0, new_frame.width, new_frame.height );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0.0, new_frame.width, 0.0, new_frame.height, -1.0, 1.0 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, new_frame.texture );

	glBegin( GL_QUADS );
		glTexCoord2f( 0.0, 0.0 );							glVertex3f( 0.0, 0.0, 0.);
		glTexCoord2f( 0.0, new_frame.height );				glVertex3f( 0.0, new_frame.height, 0.);
		glTexCoord2f( new_frame.width, new_frame.height );	glVertex3f( new_frame.width, new_frame.height, 0.);
		glTexCoord2f( new_frame.width, 0.0 );				glVertex3f( new_frame.width, 0.0, 0.);
	glEnd();

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
	
	update();
	
	new_frame.new = 0;
}



void* video_thread( void *arg )
{
	mlt_frame next = NULL;
	mlt_consumer consumer = &xgl->parent;

	while ( vthread.running )
	{
		// Get a frame from the attached producer
		next = mlt_consumer_rt_frame( consumer );

		// Ensure that we have a frame
		if ( next )
		{
			if ( next ) {
				mlt_properties properties =  MLT_FRAME_PROPERTIES( next );
				// Get the image, width and height
				mlt_image_format vfmt = mlt_image_glsl;
				int width = 0, height = 0;
				uint8_t *image = 0;
				int error = mlt_frame_get_image( next, &image, &vfmt, &width, &height, 0 );
				if ( !error && image && width && height && !new_frame.new ) {
					GLuint tex = mlt_glsl_texture( (void*)image );
					new_frame.width = width;
					new_frame.height = height;
					new_frame.texture = tex;
					new_frame.aspect_ratio = ((double)width / (double)height) * mlt_properties_get_double( properties, "aspect_ratio" );
					new_frame.new = 1;
					
					int loop = 200;
					while ( new_frame.new && --loop )
						usleep( 500 );
				}
				new_frame.new = 0;
				// This frame can now be closed
				mlt_frame_close( next );
			}

			next = NULL;
		}
		else
			usleep( 1000 );
	}

	if ( next != NULL )
		mlt_frame_close( next );

	mlt_consumer_stopped( consumer );
	
	return NULL;
}



static void resizeGLScene()
{
	glXMakeCurrent( GLWin.dpy, GLWin.win, GLWin.ctx );
	
	if ( GLWin.height == 0 )
		GLWin.height = 1;
	if ( GLWin.width == 0 )
		GLWin.width = 1;
	glViewport( 0, 0, GLWin.width, GLWin.height );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0.0, GLWin.width, 0.0, GLWin.height, -1.0, 1.0 );
	glMatrixMode( GL_MODELVIEW );

	update();
}



static void initGL( void )
{
	glXMakeCurrent( GLWin.dpy, GLWin.win, GLWin.ctx );
	
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClearDepth( 1.0f );
	glDepthFunc( GL_LEQUAL );
	glEnable( GL_DEPTH_TEST );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_BLEND );
	glShadeModel( GL_SMOOTH );
	glEnable( GL_TEXTURE_RECTANGLE_ARB );
	glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

	typedef int (*GLXSWAPINTERVALSGI) ( int );
	GLXSWAPINTERVALSGI mglXSwapInterval = (GLXSWAPINTERVALSGI)glXGetProcAddressARB( (const GLubyte*)"glXSwapIntervalSGI" );
	if ( mglXSwapInterval )
		mglXSwapInterval( 1 );

	fb.fbo = 0;
	fb.width = STARTWIDTH;
	fb.height = STARTHEIGHT;
	glGenFramebuffers( 1, &fb.fbo );
	glGenTextures( 1, &fb.texture );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, fb.texture );
	glTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, fb.width, fb.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
	glTexParameterf( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameterf( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glBindFramebuffer( GL_FRAMEBUFFER, fb.fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, fb.texture, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	
	resizeGLScene();
}



static void createGLWindow()
{
	const char* title = "OpenGL consumer";
	int width = STARTWIDTH;
	int height = STARTHEIGHT;
	
	int attrListSgl[] = { GLX_RGBA, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 16, None };

	int attrListDbl[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 16, None };

	XVisualInfo *vi;
	Colormap cmap;
	Atom wmDelete;
	Window winDummy;
	unsigned int borderDummy;

	GLWin.dpy = XOpenDisplay( 0 );
	GLWin.screen = DefaultScreen( GLWin.dpy );

	vi = glXChooseVisual( GLWin.dpy, GLWin.screen, attrListDbl );
	if ( !vi )
		vi = glXChooseVisual( GLWin.dpy, GLWin.screen, attrListSgl );

	GLWin.ctx = glXCreateContext( GLWin.dpy, vi, 0, GL_TRUE );

	cmap = XCreateColormap( GLWin.dpy, RootWindow( GLWin.dpy, vi->screen ), vi->visual, AllocNone );
	GLWin.attr.colormap = cmap;
	GLWin.attr.border_pixel = 0;

	GLWin.attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask;
	GLWin.win = XCreateWindow( GLWin.dpy, RootWindow(GLWin.dpy, vi->screen), 0, 0, width, height,
		0, vi->depth, InputOutput, vi->visual, CWBorderPixel | CWColormap | CWEventMask, &GLWin.attr );
	wmDelete = XInternAtom( GLWin.dpy, "WM_DELETE_WINDOW", True );
	XSetWMProtocols( GLWin.dpy, GLWin.win, &wmDelete, 1 );
	XSetStandardProperties( GLWin.dpy, GLWin.win, title, title, None, NULL, 0, NULL );
	XMapRaised( GLWin.dpy, GLWin.win );

	glXMakeCurrent( GLWin.dpy, GLWin.win, GLWin.ctx );
	XGetGeometry( GLWin.dpy, GLWin.win, &winDummy, &GLWin.x, &GLWin.y,
		&GLWin.width, &GLWin.height, &borderDummy, &GLWin.depth );

	fprintf(stderr, "Direct Rendering: %s\n",glXIsDirect( GLWin.dpy, GLWin.ctx ) ? "true" : "false" );
	
	mlt_has_glsl = 0;
	typedef int (*MLT_GLSL_SUPPORTED)();
	MLT_GLSL_SUPPORTED mlt_glsl_supported;
	typedef int (*MLT_GLSL_INIT)( void*, void*, void* );
	MLT_GLSL_INIT mlt_glsl_init;
	mlt_properties prop = mlt_global_properties();
	mlt_glsl_supported = (MLT_GLSL_SUPPORTED)mlt_properties_get_data( prop, "mlt_glsl_supported", NULL );
	mlt_glsl_init = (MLT_GLSL_INIT)mlt_properties_get_data( prop, "mlt_glsl_init", NULL );
	mlt_glsl_texture = (MLT_GLSL_TEXTURE)mlt_properties_get_data( prop, "mlt_glsl_get_texture", NULL );
	if ( mlt_glsl_supported && mlt_glsl_init && mlt_glsl_texture) {
		hiddenctx.ctx = glXCreateContext( GLWin.dpy, vi, GLWin.ctx, GL_TRUE );
		if ( hiddenctx.ctx ) {
			hiddenctx.dpy = GLWin.dpy;
			hiddenctx.screen = GLWin.screen;
			hiddenctx.win = RootWindow( hiddenctx.dpy, hiddenctx.screen );
			if ( mlt_glsl_supported() ) {
				if ( mlt_glsl_init( (void*)hidden_make_current, (void*)hidden_done_current, (void*)&hiddenctx ) )
					mlt_has_glsl = 1;
			}
		}
	}
	
	if ( mlt_has_glsl )
		fprintf(stderr, "Shared context created.\n");
	else
		fprintf(stderr, "No shared context :-(\n");

	initGL();
	initKeys();
}



static void killGLWindow()
{
	if ( vthread.running ) {
		vthread.running = 0;
		pthread_join( vthread.thread, NULL );
	}
		
	if ( GLWin.ctx ) {
		if ( !glXMakeCurrent( GLWin.dpy, None, NULL ) ) {
			printf("Error releasing drawing context : killGLWindow\n");
		}
		glXDestroyContext( GLWin.dpy, GLWin.ctx );
		GLWin.ctx = NULL;
	}
	
	if ( hiddenctx.ctx )
		glXDestroyContext( hiddenctx.dpy, hiddenctx.ctx );

	XCloseDisplay( GLWin.dpy );
}



static void run()
{
	XEvent event;
	int running = 1;

	while ( running ) {
		while ( XPending( GLWin.dpy ) > 0 ) {
			XNextEvent( GLWin.dpy, &event );
			switch ( event.type ) {
				case Expose:
					if ( event.xexpose.count != 0 )
						break;
					break;
				case ConfigureNotify:
					if ( (event.xconfigure.width != GLWin.width) || (event.xconfigure.height != GLWin.height) ) {
						GLWin.width = event.xconfigure.width;
						GLWin.height = event.xconfigure.height;
						resizeGLScene();
					}
					break;
				case KeyPress:
					keys[event.xkey.keycode] = True;
					switch ( XLookupKeysym( &event.xkey, 0 ) ) {
						case XK_Escape:									
							running = 0;
							break;
					}
					break;
				case KeyRelease:
					keys[event.xkey.keycode] = False;
					break;
				case ClientMessage:
					if ( *XGetAtomName( GLWin.dpy, event.xclient.message_type ) == *"WM_PROTOCOLS" )
						running = 0;
					break;
				default:
					break;
			}
		}
		
		if ( new_frame.new )
			show_frame();
		else
			usleep( 1000 );
	}
}



void start_xgl( consumer_xgl consumer )
{
	xgl = consumer;
	
	pthread_mutex_init( &new_frame.mutex, NULL );
	new_frame.aspect_ratio = 16.0 / 9.0;
	new_frame.new = 0;
	new_frame.width = STARTWIDTH;
	new_frame.height = STARTHEIGHT;
	
	vthread.running = 0;
	
	createGLWindow();
	run();
	killGLWindow();
	xgl->running = 0;
}



/** Forward references to static functions.
*/

static int consumer_start( mlt_consumer parent );
static int consumer_stop( mlt_consumer parent );
static int consumer_is_stopped( mlt_consumer parent );
static void consumer_close( mlt_consumer parent );
static void *consumer_thread( void * );



/** This is what will be called by the factory - anything can be passed in
	via the argument, but keep it simple.
*/

mlt_consumer consumer_xgl_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	fprintf(stderr, "consumer_xgl_init\n");
	// Create the consumer object
	consumer_xgl this = calloc( sizeof( struct consumer_xgl_s ), 1 );

	// If no malloc'd and consumer init ok
	if ( this != NULL && mlt_consumer_init( &this->parent, this, profile ) == 0 )
	{
		// Create the queue
		this->queue = mlt_deque_init( );

		// Get the parent consumer object
		mlt_consumer parent = &this->parent;

		// We have stuff to clean up, so override the close method
		parent->close = consumer_close;

		// get a handle on properties
		mlt_service service = MLT_CONSUMER_SERVICE( parent );
		this->properties = MLT_SERVICE_PROPERTIES( service );

		// Default scaler
		mlt_properties_set( this->properties, "rescale", "bilinear" );
		mlt_properties_set( this->properties, "deinterlace_method", "onefield" );

		// default image format
		mlt_properties_set( this->properties, "mlt_image_format", "glsl" );

		// not more than 1
		mlt_properties_set_int( this->properties, "real_time", 1 );

		// Default buffer for low latency
		mlt_properties_set_int( this->properties, "buffer", 1 );

		// Ensure we don't join on a non-running object
		this->joined = 1;
		this->xgl_started = 0;

		// Allow thread to be started/stopped
		parent->start = consumer_start;
		parent->stop = consumer_stop;
		parent->is_stopped = consumer_is_stopped;

		glsl_env g = glsl_env_create();
		if ( g )
		{
			mlt_properties prop = mlt_global_properties();
			if ( prop )
			{
				mlt_properties_set_data( prop, "glsl_env", (void*)g, 0, NULL, NULL );
			}
		}

		// Return the consumer produced
		return parent;
	}

	// malloc or consumer init failed
	free( this );

	// Indicate failure
	return NULL;
}



int consumer_start( mlt_consumer parent )
{
	consumer_xgl this = parent->child;

	if ( !this->running )
	{
		consumer_stop( parent );

		this->running = 1;
		this->joined = 0;

		pthread_create( &this->thread, NULL, consumer_thread, this );
	}

	return 0;
}



int consumer_stop( mlt_consumer parent )
{
	// Get the actual object
	consumer_xgl this = parent->child;
	
	if ( this->running && this->joined == 0 )
	{
		// Kill the thread and clean up
		this->joined = 1;
		this->running = 0;

		if ( this->thread )
			pthread_join( this->thread, NULL );
	}

	return 0;
}



int consumer_is_stopped( mlt_consumer parent )
{
	consumer_xgl this = parent->child;
	return !this->running;
}



static void *consumer_thread( void *arg )
{
	// Identify the arg
	consumer_xgl this = arg;

	XInitThreads();
	start_xgl( this );

	return NULL;
}



/** Callback to allow override of the close method.
*/

static void consumer_close( mlt_consumer parent )
{
	// Get the actual object
	consumer_xgl this = parent->child;

	// Stop the consumer
	///mlt_consumer_stop( parent );

	// Now clean up the rest
	mlt_consumer_close( parent );

	// Close the queue
	mlt_deque_close( this->queue );

	// Finally clean up this
	free( this );
}
