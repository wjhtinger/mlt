/*
 * qgl_wrapper.cpp -- a Qt OpenGL based consumer
 * Copyright (C) 2011 Christophe Thommeret
 * Author: Christophe Thommeret <hftom@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "qgl_wrapper.h"
#include "mlt_glsl.h"

#include <QtGui>
#include <QtOpenGL>

#include <sys/syscall.h>



class VideoThread : public QThread
{
	Q_OBJECT
public:
	VideoThread( consumer_qgl consumer );
	~VideoThread();

protected:
	void run();

private:
	consumer_qgl qgl;

signals:
	void newFrame( glsl_texture t, int w, int h, double ar );

};



class VideoWidget : public QGLWidget
{
	Q_OBJECT
public:
	VideoWidget( consumer_qgl consumer );
	~VideoWidget();

	static void hiddenMakeCurrent( void *userData );
	static void hiddenDoneCurrent( void *userData );

	typedef int (*GLXSWAPINTERVALSGI) ( int );
	GLXSWAPINTERVALSGI mglXSwapInterval;

protected :
	void initializeGL();
	void resizeGL( int, int );
	void paintGL();

private:
	consumer_qgl qgl;
	VideoThread *thread;
	bool threadStarted;
	double aspectRatio;

	QGLFramebufferObject *fb;

	QGLWidget *hiddenctx;

private slots:
	void showFrame( glsl_texture gt, int w, int h, double ar );
};



VideoThread::VideoThread( consumer_qgl consumer )
{
	qgl = consumer;
	//start();
}

VideoThread::~VideoThread()
{
}

void VideoThread::run()
{
	mlt_frame frame = NULL;
	mlt_frame next = NULL;
	mlt_consumer consumer = &qgl->parent;
	mlt_properties consumer_props = MLT_CONSUMER_PROPERTIES( consumer );
	glsl_env g = (glsl_env)mlt_properties_get_data( mlt_global_properties(), "glsl_env", 0 );
	int skipped = 0;
	double duration = 0;
	QTime time;

	fprintf(stderr, "videoThread started\n");

	while ( qgl->running )
	{
		time.start();
		// Get a frame from the attached producer
		frame = mlt_consumer_rt_frame( consumer );

		// Ensure that we have a frame
		if ( frame )
		{
			fprintf(stderr,"VideoThread threadID : %ld\n", syscall(SYS_gettid));

			//while ( qgl->running && mlt_deque_count( qgl->queue ) > 15 )
				//usleep( 100 );

			next = (mlt_frame)mlt_deque_pop_front( qgl->queue );

			if ( next ) {
				fprintf(stderr,"VideoThread threadID : %ld\n", syscall(SYS_gettid));

				mlt_properties properties =  MLT_FRAME_PROPERTIES( next );
				// Get the image, width and height
				if ( duration >= 0 || skipped > 4 ) {
					skipped = 0;
					mlt_image_format vfmt = mlt_image_glsl;
					int width = 0, height = 0;
					uint8_t *image;
					mlt_frame_get_image( next, &image, &vfmt, &width, &height, 0 );
					g->finish( g );
					glsl_texture gt = (glsl_texture)image;
					if ( gt && (vfmt == mlt_image_glsl) ) {
						fprintf( stderr, "VideoThread glsl_texture=%u, tex=%u [frame:%d], w=%d h=%d\n", (unsigned int)gt, gt->texture, mlt_properties_get_int( properties, "_position"), width, height );
						double ar = mlt_properties_get_double( properties, "aspect_ratio" );
						emit newFrame( gt, width, height, ar );
					}
				}				                                      
				else
					++skipped;
				duration = 1000.0 / mlt_properties_get_double( consumer_props, "fps" );
				duration -= time.elapsed();
				fprintf(stderr, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ %f ms @@@@@@@@@@@@@@@@@@ \n", duration);
				//if ( duration > 0 )
					//msleep( (int)duration );

				// This frame can now be closed
				fprintf(stderr, ".........................mlt_frame_close\n");
				mlt_frame_close( next );
			}

			// Push this frame to the back of the queue
			mlt_deque_push_back( qgl->queue, frame );
			next = frame = NULL;
			//usleep( 25000 );
		}
		else
			usleep( 1000 );
	}

	if ( next != NULL )
		mlt_frame_close( next );

	mlt_consumer_stopped( consumer );
}



VideoWidget::VideoWidget( consumer_qgl consumer ) : QGLWidget()
{
	setAttribute( Qt::WA_DeleteOnClose );
	setWindowTitle( "QtOpenGL consumer" );
	qgl = consumer;

	thread = 0;
	threadStarted = false;

	aspectRatio = 16.0 / 9.0;
	
	qRegisterMetaType<glsl_texture>("glsl_texture");
}

VideoWidget::~VideoWidget()
{
	if ( thread ) {
		qgl->running = 0;
		thread->wait();
	}
}

void VideoWidget::hiddenMakeCurrent( void *userData )
{
	QGLWidget *w = (QGLWidget*)userData;
	w->makeCurrent();
	fprintf(stderr, "hiddenMakeCurrent\n");
}

void VideoWidget::hiddenDoneCurrent( void *userData )
{
	QGLWidget *w = (QGLWidget*)userData;
	w->doneCurrent();
	fprintf(stderr, "hiddenDoneCurrent\n");
}

void VideoWidget::showFrame( glsl_texture gt, int w, int h, double ar )
{
	#include <sys/syscall.h>
	fprintf(stderr,"showFrame threadID : %ld\n", syscall(SYS_gettid));
	makeCurrent ();
	if ( (fb->width() != w) || (fb->height() != h) ) {
		delete fb;
		fb = new QGLFramebufferObject( w, h, GL_TEXTURE_RECTANGLE_ARB );
		glBindTexture( GL_TEXTURE_RECTANGLE_ARB, fb->texture() );
		glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	glPushAttrib(GL_VIEWPORT_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
	
	fb->bind();
	
	glViewport( 0, 0, w, h );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0.0, w, 0.0, h, -1.0, 1.0 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, gt->texture );

	glBegin( GL_QUADS );
		glTexCoord2f( 0.0, 0.0 );	glVertex3f( 0.0, 0.0, 0.);
		glTexCoord2f( 0.0, h );		glVertex3f( 0.0, h, 0.);
		glTexCoord2f( w, h );		glVertex3f( w, h, 0.);
		glTexCoord2f( w, 0.0 );		glVertex3f( w, 0.0, 0.);
	glEnd();

	fb->release();

	glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
	
	aspectRatio = ((double)w / (double)h) * ar;
	updateGL();
}

void VideoWidget::initializeGL()
{
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClearDepth( 1.0f );
	glDepthFunc( GL_LEQUAL );
	glEnable( GL_DEPTH_TEST );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_BLEND );
	glShadeModel( GL_SMOOTH );
	glEnable( GL_TEXTURE_RECTANGLE_ARB );
	glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

	typedef int (*MLT_GLSL_SUPPORTED)();
	MLT_GLSL_SUPPORTED mlt_glsl_supported;
	mlt_properties prop = mlt_global_properties();
	mlt_glsl_supported = (MLT_GLSL_SUPPORTED)mlt_properties_get_data( prop, "mlt_glsl_supported", NULL );
	if ( mlt_glsl_supported ) {
		hiddenctx = new QGLWidget( 0, this );
		if ( hiddenctx->isSharing() ) {
			fprintf(stderr, "Shared context created.\n");
			doneCurrent();
			hiddenctx->makeCurrent();
			if ( !mlt_glsl_supported() ) {
				hiddenctx->doneCurrent();
				delete hiddenctx;
			}
			else {
				hiddenctx->doneCurrent();
				glsl_env g = (glsl_env)mlt_properties_get_data( prop, "glsl_env", 0 );
				if ( g ) {
					g->context_make_current = hiddenMakeCurrent;
					g->context_done_current = hiddenDoneCurrent;
					g->user_data = (void*)hiddenctx;
				}
			}
		}
		else {
			delete hiddenctx;
		}
	}
	makeCurrent();

	mglXSwapInterval = (GLXSWAPINTERVALSGI)context()->getProcAddress( "glXSwapIntervalSGI" );
	if ( mglXSwapInterval )
		mglXSwapInterval( 1 );

	fb = new QGLFramebufferObject( 640, 480, GL_TEXTURE_RECTANGLE_ARB );
	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, fb->texture() );
	glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	thread = new VideoThread( qgl );
	connect( thread, SIGNAL(newFrame(glsl_texture,int,int,double)), this, SLOT(showFrame(glsl_texture,int,int,double)), Qt::BlockingQueuedConnection );

}

void VideoWidget::resizeGL( int width, int height )
{
	glViewport( 0, 0, width, height );
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, width, 0.0, height, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	updateGL();
}

void VideoWidget::paintGL()
{
	int _width = width();
	int _height = height();
	GLfloat left, right, top, bottom, u, u1, v, v1;

	u = 0.0;
	v = 0.0;
	u1 = fb->width();
	v1 = fb->height();

	GLfloat war = (GLfloat)_width/(GLfloat)_height;

	if ( war < aspectRatio ) {
		left = -1.0;
		right = 1.0;
		top = war / aspectRatio;
		bottom = -war / aspectRatio;
	}
	else {
		top = 1.0;
		bottom = -1.0;
		left = -aspectRatio / war;
		right = aspectRatio / war;
	}

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glLoadIdentity();

	glPushMatrix();

	glTranslatef( _width/2, _height/2, 0 );
	glScalef( _width/2, _height/2, 1.0 );

	glBindTexture( GL_TEXTURE_RECTANGLE_ARB, fb->texture() );

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

	if ( !threadStarted ) {
		thread->start();
		threadStarted = true;
	}
}

#include "qgl_wrapper.moc"



extern "C" {

void start_qgl( consumer_qgl consumer )
{
	int argc = 1;
	char *argv = new char(5);
	strcpy( argv, "none");
	
	QApplication app( argc, &argv );

	VideoWidget *vw = new VideoWidget( consumer );
	vw->show();

	app.exec();
}

} // extern "C"
