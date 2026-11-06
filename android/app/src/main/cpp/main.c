#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <stdint.h>

#define LOG_TAG "DarknesMobile"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef struct EngineShell {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
    bool ready;
    float phase;
} EngineShell;

static void destroy_display(EngineShell *engine) {
    if (engine->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) eglDestroyContext(engine->display, engine->context);
        if (engine->surface != EGL_NO_SURFACE) eglDestroySurface(engine->display, engine->surface);
        eglTerminate(engine->display);
    }
    engine->display = EGL_NO_DISPLAY;
    engine->surface = EGL_NO_SURFACE;
    engine->context = EGL_NO_CONTEXT;
    engine->ready = false;
}

static bool init_display(EngineShell *engine, ANativeWindow *window) {
    const EGLint config_attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    const EGLint context_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLConfig config;
    EGLint count = 0;

    engine->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (engine->display == EGL_NO_DISPLAY || !eglInitialize(engine->display, 0, 0)) {
        LOGE("eglInitialize failed: 0x%x", eglGetError());
        destroy_display(engine);
        return false;
    }
    if (!eglChooseConfig(engine->display, config_attrs, &config, 1, &count) || count < 1) {
        LOGE("No compatible EGL config: 0x%x", eglGetError());
        destroy_display(engine);
        return false;
    }
    EGLint format = 0;
    eglGetConfigAttrib(engine->display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(window, 0, 0, format);

    engine->surface = eglCreateWindowSurface(engine->display, config, window, 0);
    engine->context = eglCreateContext(engine->display, config, EGL_NO_CONTEXT, context_attrs);
    if (engine->surface == EGL_NO_SURFACE || engine->context == EGL_NO_CONTEXT ||
        !eglMakeCurrent(engine->display, engine->surface, engine->surface, engine->context)) {
        LOGE("EGL surface/context creation failed: 0x%x", eglGetError());
        destroy_display(engine);
        return false;
    }

    eglQuerySurface(engine->display, engine->surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(engine->display, engine->surface, EGL_HEIGHT, &engine->height);
    glViewport(0, 0, engine->width, engine->height);
    engine->ready = true;
    LOGI("Android GLES2 shell ready: %dx%d", engine->width, engine->height);
    return true;
}

static void handle_cmd(struct android_app *app, int32_t cmd) {
    EngineShell *engine = (EngineShell *)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) init_display(engine, app->window);
            break;
        case APP_CMD_TERM_WINDOW:
            destroy_display(engine);
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            if (engine->ready && engine->display != EGL_NO_DISPLAY) {
                eglQuerySurface(engine->display, engine->surface, EGL_WIDTH, &engine->width);
                eglQuerySurface(engine->display, engine->surface, EGL_HEIGHT, &engine->height);
                glViewport(0, 0, engine->width, engine->height);
            }
            break;
        case APP_CMD_PAUSE:
            LOGI("Activity paused");
            break;
        case APP_CMD_RESUME:
            LOGI("Activity resumed");
            break;
        case APP_CMD_DESTROY:
            destroy_display(engine);
            break;
        default:
            break;
    }
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    (void)app;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        const int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        if (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            LOGI("Touch input received");
            return 1;
        }
    }
    return 0;
}

void android_main(struct android_app *app) {
    app_dummy();
    EngineShell engine = {
        .display = EGL_NO_DISPLAY,
        .surface = EGL_NO_SURFACE,
        .context = EGL_NO_CONTEXT,
        .width = 0,
        .height = 0,
        .ready = false,
        .phase = 0.0f
    };
    app->userData = &engine;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    LOGI("Darknes Android native shell started (ARM64 target)");
    while (!app->destroyRequested) {
        int events = 0;
        struct android_poll_source *source = 0;
        const int timeout = engine.ready ? 0 : -1;
        if (ALooper_pollOnce(timeout, 0, &events, (void **)&source) >= 0 && source) {
            source->process(app, source);
        }

        if (engine.ready) {
            engine.phase += 0.0025f;
            if (engine.phase > 1.0f) engine.phase = 0.0f;
            const float pulse = 0.025f * engine.phase;
            glClearColor(0.018f + pulse, 0.012f, 0.025f + pulse, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!eglSwapBuffers(engine.display, engine.surface)) {
                const EGLint error = eglGetError();
                LOGE("eglSwapBuffers failed: 0x%x", error);
                if (error == EGL_CONTEXT_LOST || error == EGL_BAD_SURFACE) {
                    destroy_display(&engine);
                }
            }
        }
    }
    destroy_display(&engine);
    LOGI("Darknes Android native shell stopped");
}
