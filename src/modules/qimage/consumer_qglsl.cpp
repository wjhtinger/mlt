/*
 * consumer_qglsl.cpp
 * Copyright (C) 2012 Dan Dennedy <dan@dennedy.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <mlt++/MltConsumer.h>
#include <mlt++/MltFilter.h>
#include <mlt++/MltProfile.h>
#include <QtGui/QApplication>
#include <QtCore/QLocale>
#include <QtCore/QThread>
#include <QtOpenGL/QGLWidget>
#include <stdio.h>

class QGlslConsumer : public QThread, public Mlt::Consumer
{
public:
	bool isStopped;

	QGlslConsumer(mlt_profile profile)
		: QThread(0)
		, Mlt::Consumer(mlt_consumer_new(profile))
		, app(qApp)
		, renderContext(0)
	{
		mlt_filter filter = mlt_factory_filter(profile, "glsl.manager", 0);
		if (filter) {
			mlt_consumer consumer = get_consumer();
			consumer->child = this;
			glslManager = new Mlt::Filter(filter);
			mlt_filter_close(filter); // glslManager holds the reference
			set("mlt_image_format", "glsl");
			set("terminate_on_pause", 1);
			set("real_time", 0);
			glslManager->fire_event("init glsl");
		} else {
			mlt_consumer_close(get_consumer());
		}
	}

	~QGlslConsumer()
	{
		delete renderContext;
		delete glslManager;
	}

	void initContext()
	{
#ifdef linux
		if ( getenv("DISPLAY") == 0 ) {
			mlt_log_error( get_service(), "The qglsl consumer requires a X11 environment.\nPlease either run melt from an X session or use a fake X server like xvfb:\nxvfb-run -a melt (...)\n" );
		} else
#endif
		{
			int argc = 1;
			char* argv[1];
			argv[0] = (char*) "MLT qglsl consumer";
			app = new QApplication(argc, argv);
			const char *localename = mlt_properties_get_lcnumeric(get_properties());
			QLocale::setDefault(QLocale(localename));
		}
		renderContext = new QGLWidget;
		renderContext->resize(0, 0);
		renderContext->show();
		app->processEvents();
	}

	void startGlsl()
	{
		renderContext->makeCurrent();
		glslManager->fire_event("start glsl");
	}

protected:
	void run()
	{
		int terminate_on_pause = get_int("terminate_on_pause");
		bool terminated = false;
		mlt_frame frame = NULL;

		initContext();
		while (!terminated && isRunning()) {
			if ((frame = mlt_consumer_rt_frame(get_consumer()))) {
				terminated = terminate_on_pause &&
					mlt_properties_get_double(MLT_FRAME_PROPERTIES(frame), "_speed") == 0.0;
				if (mlt_properties_get_int(MLT_FRAME_PROPERTIES(frame), "rendered")) {
					mlt_events_fire(get_properties(), "consumer-frame-show", frame, NULL);
					mlt_frame_write_ppm(frame);
				}
				mlt_frame_close(frame);
			}
		}
		mlt_consumer_stopped(get_consumer());
	}

private:
	Mlt::Filter* glslManager;
	QApplication* app;
	QGLWidget* renderContext;
};

static int start(mlt_consumer consumer)
{
	QGlslConsumer* glsl = (QGlslConsumer*) consumer->child;
	glsl->QThread::start();
	return 0;
}

static int is_stopped(mlt_consumer consumer)
{
	QGlslConsumer* qglsl = (QGlslConsumer*) consumer->child;
	return !qglsl->isRunning();
}

static int stop(mlt_consumer consumer)
{
	QGlslConsumer* qglsl = (QGlslConsumer*) consumer->child;
	qglsl->quit();
	return 0;
}

static void close(mlt_consumer consumer)
{
	QGlslConsumer* c = (QGlslConsumer*) consumer->child;
	mlt_consumer_close(consumer);
	delete c;
}

static void onThreadStarted(mlt_properties owner, QGlslConsumer* qglsl)
{
	mlt_log_verbose(qglsl->get_service(), "%s\n", __FUNCTION__);
	qglsl->startGlsl();
}

static void onFrameRendered(mlt_properties owner, QGlslConsumer* qglsl)
{
	mlt_log_verbose(qglsl->get_service(), "%s\n", __FUNCTION__);
	glFinish();
}

extern "C" {

mlt_consumer consumer_qglsl_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg )
{
	QGlslConsumer* consumer = new QGlslConsumer(profile);
	if (consumer->get_consumer()) {
		mlt_consumer c = consumer->get_consumer();
		// These callbacks in C are so just so much cleaner to set in C.
		c->start = start;
		c->is_stopped = is_stopped;
		c->stop = stop;
		c->close = close;
		consumer->listen("consumer-thread-started", consumer, (mlt_listener) onThreadStarted);
		consumer->listen("consumer-frame-rendered", consumer, (mlt_listener) onFrameRendered);
		return c;
	} else {
		delete consumer;
		return NULL;
	}
}

}
