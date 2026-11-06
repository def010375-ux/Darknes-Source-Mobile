#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_TAG "DarknesStrike"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define PI 3.14159265358979323846f
#define MAX_TOUCHES 10

typedef struct { float x,y,z; } V3;
typedef struct { int id; float x,y,px,py; bool down; int role; } Touch;
typedef struct {
 EGLDisplay display; EGLSurface surface; EGLContext context; int width,height; bool ready;
 GLuint program; GLint aPos,aNormal,uMvp,uModel,uColor,uLight;
 GLuint vbo; float projection[16];
 V3 player; float yaw,pitch; float moveX,moveY; Touch touches[MAX_TOUCHES];
 int ammo,health,kills; float fireCooldown; bool firing; float hitFlash; float elapsed;
} Game;

static const GLfloat cubeVerts[] = {
 /* front */ -0.5f,-0.5f,0.5f, 0,0,1,  0.5f,-0.5f,0.5f,0,0,1, 0.5f,0.5f,0.5f,0,0,1,
 -0.5f,-0.5f,0.5f,0,0,1, 0.5f,0.5f,0.5f,0,0,1, -0.5f,0.5f,0.5f,0,0,1,
 /* back */ 0.5f,-0.5f,-0.5f,0,0,-1, -0.5f,-0.5f,-0.5f,0,0,-1, -0.5f,0.5f,-0.5f,0,0,-1,
 0.5f,-0.5f,-0.5f,0,0,-1, -0.5f,0.5f,-0.5f,0,0,-1, 0.5f,0.5f,-0.5f,0,0,-1,
 /* left */ -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f,-0.5f,0.5f,-1,0,0, -0.5f,0.5f,0.5f,-1,0,0,
 -0.5f,-0.5f,-0.5f,-1,0,0, -0.5f,0.5f,0.5f,-1,0,0, -0.5f,0.5f,-0.5f,-1,0,0,
 /* right */ 0.5f,-0.5f,0.5f,1,0,0, 0.5f,-0.5f,-0.5f,1,0,0, 0.5f,0.5f,-0.5f,1,0,0,
 0.5f,-0.5f,0.5f,1,0,0, 0.5f,0.5f,-0.5f,1,0,0, 0.5f,0.5f,0.5f,1,0,0,
 /* top */ -0.5f,0.5f,0.5f,0,1,0, 0.5f,0.5f,0.5f,0,1,0, 0.5f,0.5f,-0.5f,0,1,0,
 -0.5f,0.5f,0.5f,0,1,0, 0.5f,0.5f,-0.5f,0,1,0, -0.5f,0.5f,-0.5f,0,1,0,
 /* bottom */ -0.5f,-0.5f,-0.5f,0,-1,0, 0.5f,-0.5f,-0.5f,0,-1,0, 0.5f,-0.5f,0.5f,0,-1,0,
 -0.5f,-0.5f,-0.5f,0,-1,0, 0.5f,-0.5f,0.5f,0,-1,0, -0.5f,-0.5f,0.5f,0,-1,0
};

static V3 v3(float x,float y,float z){ V3 r={x,y,z}; return r; }
static V3 add(V3 a,V3 b){return v3(a.x+b.x,a.y+b.y,a.z+b.z);}
static V3 mul(V3 a,float s){return v3(a.x*s,a.y*s,a.z*s);}
static V3 cross(V3 a,V3 b){return v3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static V3 norm(V3 a){float l=sqrtf(a.x*a.x+a.y*a.y+a.z*a.z);return l>0.0001f?mul(a,1.0f/l):a;}
static void ident(float*m){memset(m,0,16*sizeof(float));m[0]=m[5]=m[10]=m[15]=1;}
static void multiply(float*out,const float*a,const float*b){
 float t[16]; for(int c=0;c<4;c++)for(int r=0;r<4;r++)t[c*4+r]=a[0*4+r]*b[c*4+0]+a[1*4+r]*b[c*4+1]+a[2*4+r]*b[c*4+2]+a[3*4+r]*b[c*4+3]; memcpy(out,t,sizeof(t));
}
static void perspective(float*m,float fovy,float aspect,float nearZ,float farZ){
 memset(m,0,16*sizeof(float));float f=1.0f/tanf(fovy*0.5f);m[0]=f/aspect;m[5]=f;m[10]=(farZ+nearZ)/(nearZ-farZ);m[11]=-1;m[14]=(2*farZ*nearZ)/(nearZ-farZ);
}
static void lookAt(float*m,V3 eye,V3 center,V3 up){
 V3 f=norm(add(center,mul(eye,-1))),s=norm(cross(f,up)),u=cross(s,f);ident(m);
 m[0]=s.x;m[1]=u.x;m[2]=-f.x;m[4]=s.y;m[5]=u.y;m[6]=-f.y;m[8]=s.z;m[9]=u.z;m[10]=-f.z;
 m[12]=- (s.x*eye.x+s.y*eye.y+s.z*eye.z);m[13]=-(u.x*eye.x+u.y*eye.y+u.z*eye.z);m[14]=(f.x*eye.x+f.y*eye.y+f.z*eye.z);
}
static GLuint shader(GLenum type,const char*src){
 GLuint s=glCreateShader(type);glShaderSource(s,1,&src,0);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[1024];glGetShaderInfoLog(s,sizeof(log),0,log);LOGE("Shader compile: %s",log);glDeleteShader(s);return 0;}return s;
}
static bool init_renderer(Game*g){
 const char*vs="attribute vec3 aPosition; attribute vec3 aNormal; uniform mat4 uMVP; uniform mat4 uModel; varying vec3 vNormal; varying vec3 vWorld; void main(){vec4 w=uModel*vec4(aPosition,1.0);vWorld=w.xyz;vNormal=mat3(uModel)*aNormal;gl_Position=uMVP*vec4(aPosition,1.0);}";
 const char*fs="precision mediump float; uniform vec4 uColor; uniform vec3 uLight; varying vec3 vNormal; varying vec3 vWorld; void main(){float d=max(dot(normalize(vNormal),normalize(uLight)),0.0);float shade=0.34+0.66*d;float fog=clamp((length(vWorld.xz)-12.0)/32.0,0.0,0.35);vec3 c=uColor.rgb*shade*(1.0-fog)+vec3(0.035,0.045,0.055)*fog;gl_FragColor=vec4(c,uColor.a);}";
 GLuint a=shader(GL_VERTEX_SHADER,vs),b=shader(GL_FRAGMENT_SHADER,fs);if(!a||!b)return false;
 g->program=glCreateProgram();glAttachShader(g->program,a);glAttachShader(g->program,b);glBindAttribLocation(g->program,0,"aPosition");glBindAttribLocation(g->program,1,"aNormal");glLinkProgram(g->program);glDeleteShader(a);glDeleteShader(b);
 GLint ok=0;glGetProgramiv(g->program,GL_LINK_STATUS,&ok);if(!ok){char log[1024];glGetProgramInfoLog(g->program,sizeof(log),0,log);LOGE("Program link: %s",log);return false;}
 g->aPos=0;g->aNormal=1;g->uMvp=glGetUniformLocation(g->program,"uMVP");g->uModel=glGetUniformLocation(g->program,"uModel");g->uColor=glGetUniformLocation(g->program,"uColor");g->uLight=glGetUniformLocation(g->program,"uLight");
 glGenBuffers(1,&g->vbo);glBindBuffer(GL_ARRAY_BUFFER,g->vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(cubeVerts),cubeVerts,GL_STATIC_DRAW);
 glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);return true;
}
static void box(Game*g,const float*vp,V3 p,V3 s,float r,float green,float b){
 float model[16],mvp[16];ident(model);model[0]=s.x;model[5]=s.y;model[10]=s.z;model[12]=p.x;model[13]=p.y;model[14]=p.z;
 multiply(mvp,vp,model);glUniformMatrix4fv(g->uMvp,1,GL_FALSE,mvp);glUniformMatrix4fv(g->uModel,1,GL_FALSE,model);glUniform4f(g->uColor,r,green,b,1);
 glDrawArrays(GL_TRIANGLES,0,36);
}
static void drawBox(Game*g,const float*vp,V3 p,V3 s,float r,float gg,float b){box(g,vp,p,s,r,gg,b);}
static V3 forwardVec(Game*g){float cp=cosf(g->pitch);return norm(v3(sinf(g->yaw)*cp,sinf(g->pitch),-cosf(g->yaw)*cp));}
static V3 rightVec(Game*g){return norm(cross(forwardVec(g),v3(0,1,0)));}
static void drawWorld(Game*g,const float*vp){
 /* compact training arena: floor, perimeter walls, cover blocks, crates and target mannequins */
 drawBox(g,vp,v3(0,-0.22f,0),v3(34,0.4f,34),0.16f,0.19f,0.20f);
 for(int i=-16;i<=16;i+=2){drawBox(g,vp,v3((float)i,-0.005f,0),v3(0.018f,0.012f,32),0.24f,0.27f,0.28f);drawBox(g,vp,v3(0,-0.004f,(float)i),v3(32,0.012f,0.018f),0.24f,0.27f,0.28f);}
 drawBox(g,vp,v3(0,1.3f,-17),v3(34,2.8f,0.5f),0.19f,0.22f,0.24f);
 drawBox(g,vp,v3(-17,1.3f,0),v3(0.5f,2.8f,34),0.19f,0.22f,0.24f);
 drawBox(g,vp,v3(17,1.3f,0),v3(0.5f,2.8f,34),0.19f,0.22f,0.24f);
 drawBox(g,vp,v3(-5,0.65f,-5),v3(3.2f,1.3f,1.1f),0.32f,0.24f,0.16f);
 drawBox(g,vp,v3(4,0.55f,-7),v3(2.2f,1.1f,2.2f),0.28f,0.31f,0.28f);
 drawBox(g,vp,v3(7,0.8f,1),v3(4.2f,1.6f,1.0f),0.30f,0.25f,0.17f);
 drawBox(g,vp,v3(-7,0.45f,5),v3(2.5f,0.9f,2.5f),0.28f,0.31f,0.28f);
 drawBox(g,vp,v3(0,0.45f,-11),v3(5.0f,0.9f,1.4f),0.32f,0.24f,0.16f);
 drawBox(g,vp,v3(-11,0.8f,-1),v3(1.2f,1.6f,4.0f),0.24f,0.28f,0.30f);
 drawBox(g,vp,v3(11,0.8f,-9),v3(1.2f,1.6f,4.0f),0.24f,0.28f,0.30f);
 for(int i=0;i<3;i++){float x=-4.0f+i*4.0f,z=-13.0f;drawBox(g,vp,v3(x,0.9f,z),v3(0.48f,1.8f,0.42f),0.32f,0.34f,0.35f);drawBox(g,vp,v3(x,1.95f,z),v3(0.62f,0.62f,0.62f),0.72f,0.54f,0.38f);drawBox(g,vp,v3(x,0.45f,z),v3(0.8f,0.16f,0.55f),0.12f,0.14f,0.16f);}
}
static void drawWeapon(Game*g,const float*vp){
 V3 f=forwardVec(g),r=rightVec(g),u=norm(cross(r,f));V3 base=add(add(g->player,mul(f,0.82f)),add(mul(r,0.30f),mul(u,-0.27f)));
 float bob=sinf(g->elapsed*8.0f)*0.012f;base.y+=bob;
 drawBox(g,vp,add(base,mul(r,-0.12f)),v3(0.16f,0.20f,0.38f),0.52f,0.31f,0.20f);
 drawBox(g,vp,add(base,mul(r,0.04f)),v3(0.18f,0.15f,0.60f),0.11f,0.13f,0.15f);
 drawBox(g,vp,add(base,mul(r,0.04f)),v3(0.08f,0.08f,0.84f),0.23f,0.25f,0.27f);
 drawBox(g,vp,add(base,mul(r,0.13f)),v3(0.09f,0.25f,0.18f),0.10f,0.11f,0.12f);
 drawBox(g,vp,add(base,mul(r,-0.10f)),v3(0.13f,0.22f,0.24f),0.14f,0.16f,0.17f);
 /* sleeves and hands */
 drawBox(g,vp,add(base,add(mul(r,-0.38f),mul(f,-0.20f))),v3(0.22f,0.28f,0.48f),0.12f,0.16f,0.20f);
 drawBox(g,vp,add(base,add(mul(r,-0.22f),mul(f,0.03f))),v3(0.16f,0.13f,0.20f),0.67f,0.46f,0.32f);
 drawBox(g,vp,add(base,add(mul(r,0.26f),mul(f,0.10f))),v3(0.20f,0.27f,0.42f),0.12f,0.16f,0.20f);
 drawBox(g,vp,add(base,add(mul(r,0.16f),mul(f,0.23f))),v3(0.15f,0.13f,0.18f),0.67f,0.46f,0.32f);
}
static void hudRect(Game*g,float x,float y,float w,float h,float r,float gg,float b){
 float m[16],vp[16];ident(m);m[0]=w;m[5]=h;m[10]=1;m[12]=x;m[13]=y;ident(vp);multiply(vp,vp,m);
 glUniformMatrix4fv(g->uMvp,1,GL_FALSE,vp);glUniformMatrix4fv(g->uModel,1,GL_FALSE,m);glUniform4f(g->uColor,r,gg,b,1);glDrawArrays(GL_TRIANGLES,0,36);
}
static void drawHud(Game*g){
 glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glUseProgram(g->program);glBindBuffer(GL_ARRAY_BUFFER,g->vbo);glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));glUniform3f(g->uLight,0,1,0);
 /* dark transparent-style panels */
 hudRect(g,-0.78f,-0.82f,0.34f,0.20f,0.025f,0.035f,0.045f);
 hudRect(g,-0.78f,-0.78f,0.23f,0.018f,0.12f,0.14f,0.16f);
 hudRect(g,-0.78f,-0.78f,0.23f*(g->health/100.0f),0.018f,0.92f,0.25f,0.12f);
 hudRect(g,0.76f,-0.82f,0.30f,0.20f,0.025f,0.035f,0.045f);
 for(int i=0;i<g->ammo;i++)hudRect(g,0.65f+(i%15)*0.014f,-0.78f+(i/15)*0.025f,0.008f,0.018f,0.95f,0.58f,0.18f);
 /* crosshair */
 hudRect(g,0,0,0.004f,0.045f,0.95f,0.86f,0.68f);hudRect(g,0,0,0.035f,0.004f,0.95f,0.86f,0.68f);
 /* left virtual stick */
 hudRect(g,-0.72f,-0.35f,0.20f,0.20f,0.10f,0.12f,0.13f);
 hudRect(g,-0.72f+g->moveX*0.06f,-0.35f+g->moveY*0.06f,0.085f,0.085f,0.72f,0.40f,0.16f);
 /* fire button */
 hudRect(g,0.78f,-0.35f,0.17f,0.17f,g->firing?0.95f:0.65f,g->firing?0.28f:0.20f,0.12f);
 /* reload button */
 hudRect(g,0.54f,-0.62f,0.11f,0.075f,0.23f,0.27f,0.29f);
 /* status bars along top */
 hudRect(g,0,0.91f,0.58f,0.055f,0.02f,0.03f,0.04f);
 for(int i=0;i<g->kills&&i<8;i++)hudRect(g,-0.24f+i*0.065f,0.91f,0.045f,0.018f,0.95f,0.55f,0.16f);
 glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);
}
static void fire(Game*g){
 if(g->fireCooldown>0)return;
 if(g->ammo<=0){g->fireCooldown=0.25f;return;}
 g->ammo--;g->fireCooldown=0.16f;g->hitFlash=0.09f;
 /* range check against training targets, aim cone around forward */
 V3 f=forwardVec(g);V3 target=v3(0,1.0f,-13.0f);V3 d=norm(add(target,mul(g->player,-1)));
 if(f.x*d.x+f.y*d.y+f.z*d.z>0.94f){g->kills++;if(g->kills>8)g->kills=0;}
}
static int findTouch(Game*g,int id){for(int i=0;i<MAX_TOUCHES;i++)if(g->touches[i].down&&g->touches[i].id==id)return i;return -1;}
static void releaseTouch(Game*g,int id){int i=findTouch(g,id);if(i>=0){g->touches[i].down=false;if(g->touches[i].role==1){g->moveX=g->moveY=0;}if(g->touches[i].role==3)g->firing=false;}}
static int32_t handle_input(struct android_app*app,AInputEvent*e){
 Game*g=(Game*)app->userData;if(AInputEvent_getType(e)!=AINPUT_EVENT_TYPE_MOTION)return 0;
 int action=AMotionEvent_getAction(e),masked=action&AMOTION_EVENT_ACTION_MASK,index=(action&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
 int count=AMotionEvent_getPointerCount(e);
 if(masked==AMOTION_EVENT_ACTION_DOWN||masked==AMOTION_EVENT_ACTION_POINTER_DOWN){
  int id=AMotionEvent_getPointerId(e,index);float x=AMotionEvent_getX(e,index),y=AMotionEvent_getY(e,index);int slot=-1;
  for(int i=0;i<MAX_TOUCHES;i++)if(!g->touches[i].down){slot=i;break;}
  if(slot>=0){Touch*t=&g->touches[slot];t->id=id;t->x=t->px=x;t->y=t->py=y;t->down=true;t->role=0;
   if(x<g->width*0.42f){t->role=1;g->moveX=g->moveY=0;}
   else if(x>g->width*0.70f&&y>g->height*0.57f){t->role=3;g->firing=true;fire(g);}
   else if(x>g->width*0.48f&&y>g->height*0.70f&&x<g->width*0.72f){t->role=4;g->ammo=30;}
   else t->role=2;
  }return 1;
 }
 if(masked==AMOTION_EVENT_ACTION_MOVE){
  for(int p=0;p<count;p++){int id=AMotionEvent_getPointerId(e,p),i=findTouch(g,id);if(i<0)continue;Touch*t=&g->touches[i];float x=AMotionEvent_getX(e,p),y=AMotionEvent_getY(e,p),dx=x-t->px,dy=y-t->py;t->px=x;t->py=y;
   if(t->role==1){g->moveX=fmaxf(-1,fminf(1,(x-t->x)/85.0f));g->moveY=fmaxf(-1,fminf(1,(y-t->y)/85.0f));}
   else if(t->role==2){g->yaw-=dx*0.0045f;g->pitch-=dy*0.0045f;if(g->pitch>1.25f)g->pitch=1.25f;if(g->pitch< -1.25f)g->pitch=-1.25f;}
  }return 1;
 }
 if(masked==AMOTION_EVENT_ACTION_UP||masked==AMOTION_EVENT_ACTION_POINTER_UP||masked==AMOTION_EVENT_ACTION_CANCEL){if(masked==AMOTION_EVENT_ACTION_CANCEL){for(int i=0;i<MAX_TOUCHES;i++)releaseTouch(g,g->touches[i].id);}else releaseTouch(g,AMotionEvent_getPointerId(e,index));return 1;}
 return 1;
}
static void destroy_display(Game*g){
 if(g->display!=EGL_NO_DISPLAY){eglMakeCurrent(g->display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(g->context!=EGL_NO_CONTEXT)eglDestroyContext(g->display,g->context);if(g->surface!=EGL_NO_SURFACE)eglDestroySurface(g->display,g->surface);eglTerminate(g->display);}
 g->display=EGL_NO_DISPLAY;g->surface=EGL_NO_SURFACE;g->context=EGL_NO_CONTEXT;g->ready=false;
}
static bool init_display(Game*g,ANativeWindow*w){
 const EGLint ca[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,16,EGL_NONE};
 const EGLint xa[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};EGLConfig cfg;EGLint n=0;
 g->display=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(g->display==EGL_NO_DISPLAY||!eglInitialize(g->display,0,0)){LOGE("EGL init failed %x",eglGetError());destroy_display(g);return false;}
 if(!eglChooseConfig(g->display,ca,&cfg,1,&n)||n<1){LOGE("No depth EGL config %x",eglGetError());destroy_display(g);return false;}
 EGLint fmt;eglGetConfigAttrib(g->display,cfg,EGL_NATIVE_VISUAL_ID,&fmt);ANativeWindow_setBuffersGeometry(w,0,0,fmt);
 g->surface=eglCreateWindowSurface(g->display,cfg,w,0);g->context=eglCreateContext(g->display,cfg,EGL_NO_CONTEXT,xa);
 if(g->surface==EGL_NO_SURFACE||g->context==EGL_NO_CONTEXT||!eglMakeCurrent(g->display,g->surface,g->surface,g->context)){LOGE("EGL context failed %x",eglGetError());destroy_display(g);return false;}
 eglQuerySurface(g->display,g->surface,EGL_WIDTH,&g->width);eglQuerySurface(g->display,g->surface,EGL_HEIGHT,&g->height);
 glViewport(0,0,g->width,g->height);if(!init_renderer(g)){destroy_display(g);return false;}
 perspective(g->projection,PI/3.0f,(float)g->width/(float)g->height,0.08f,90.0f);g->ready=true;LOGI("Darknes Strike 3D arena initialized %dx%d",g->width,g->height);return true;
}
static void handle_cmd(struct android_app*app,int32_t cmd){
 Game*g=(Game*)app->userData;switch(cmd){case APP_CMD_INIT_WINDOW:if(app->window)init_display(g,app->window);break;case APP_CMD_TERM_WINDOW:destroy_display(g);break;case APP_CMD_WINDOW_RESIZED:case APP_CMD_CONFIG_CHANGED:if(g->ready){eglQuerySurface(g->display,g->surface,EGL_WIDTH,&g->width);eglQuerySurface(g->display,g->surface,EGL_HEIGHT,&g->height);glViewport(0,0,g->width,g->height);perspective(g->projection,PI/3.0f,(float)g->width/(float)g->height,0.08f,90.0f);}break;case APP_CMD_DESTROY:destroy_display(g);break;default:break;}
}
static void frame(Game*g,float dt){
 g->elapsed+=dt;if(g->fireCooldown>0)g->fireCooldown-=dt;if(g->hitFlash>0)g->hitFlash-=dt;
 V3 f=forwardVec(g),r=rightVec(g);V3 flat=norm(v3(f.x,0,f.z));V3 move=add(mul(flat,-g->moveY),mul(r,g->moveX));float speed=3.6f;if(move.x*move.x+move.z*move.z>0.001f)g->player=add(g->player,mul(norm(move),speed*dt));
 if(g->firing)fire(g);
 glViewport(0,0,g->width,g->height);glClearColor(0.035f,0.048f,0.060f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 V3 eye=add(g->player,v3(0,1.58f,0));float view[16],vp[16];lookAt(view,eye,add(eye,f),v3(0,1,0));multiply(vp,g->projection,view);
 glUseProgram(g->program);glBindBuffer(GL_ARRAY_BUFFER,g->vbo);glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));glUniform3f(g->uLight,-0.4f,0.9f,0.35f);
 drawWorld(g,vp);drawWeapon(g,vp);drawHud(g);
 if(!eglSwapBuffers(g->display,g->surface)){EGLint e=eglGetError();LOGE("Swap failed %x",e);if(e==EGL_CONTEXT_LOST||e==EGL_BAD_SURFACE)destroy_display(g);}
}
void android_main(struct android_app*app){
 app_dummy();Game g;memset(&g,0,sizeof(g));g.display=EGL_NO_DISPLAY;g.surface=EGL_NO_SURFACE;g.context=EGL_NO_CONTEXT;g.player=v3(0,0,7);g.yaw=0;g.pitch=0;g.ammo=30;g.health=100;app->userData=&g;app->onAppCmd=handle_cmd;app->onInputEvent=handle_input;
 LOGI("Darknes Strike playable 3D prototype starting");int64_t last=0;
 while(!app->destroyRequested){int events;struct android_poll_source*source;int timeout=g.ready?0:-1;while(ALooper_pollOnce(timeout,0,&events,(void**)&source)>=0){if(source)source->process(app,source);if(app->destroyRequested)break;timeout=0;}
  if(g.ready){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);int64_t now=(int64_t)ts.tv_sec*1000000000LL+ts.tv_nsec;if(!last)last=now;float dt=(float)(now-last)/1000000000.0f;last=now;if(dt>0.05f)dt=0.05f;frame(&g,dt);}
 }
 destroy_display(&g);LOGI("Darknes Strike stopped");
}