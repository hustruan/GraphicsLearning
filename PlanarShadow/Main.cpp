#include <iostream>
#include <gl/glew.h>
#include <gl/glut.h>
#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include <nvImage.h>
#include "Math.h"

using namespace std;

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glut32.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "cg.lib")
#pragma comment(lib, "cgGL.lib")
#pragma comment(lib, "nvImaged.lib")

#define BUFFER_OFFSET(i) ((char*)NULL + (i))

const char* myProgramName = "GlutCgfx";
unsigned int appWidth = 640;
unsigned int appHeight = 480;
float		 appFiewOfView = 60.0f;


GLuint myPlanTex, myCubeTex;
GLuint myCubeVBO, myCubeIBO;

CGcontext myCgContext;
CGeffect myCgEffect;
CGtechnique myCgTechnique;
CGparameter myCgParam_WorldMatrix, myCgParam_ViewMatrix, myCgParam_ProjectionMatrix;
CGparameter myCgParam_LightPosition, myCgParam_LightColor;
CGparameter myCgParam_DiffuseMap;
CGparameter myCgParam_Shadow;

Matrix4f myProjectionMatrix;
Matrix4f myViewMatrix;

Vector3 myEyePos(0, 60, 60);

Vector3 myLightPos(40, 40, 40);
Vector3 myLightColor(1.0f, 1.0f, 1.0f);

Vector3 myPlaneNormal;
float myPlaneDist;

struct SimpleVertex
{
	Vector3 Positon;
	Vector3 Normal;
	Vector2 Tex;
};


void Reshape(int width, int height);
void Display(void);
void IdleFunc(void);
void InitOpenGL(void);
void InitCg();
void InitContent(void);
void CheckForCgError(const char *situation);


void CheckForCgError(const char *situation)
{
	CGerror error;
	const char *string = cgGetLastErrorString(&error);

	if (error != CG_NO_ERROR) {
		printf("%s: %s: %s\n",
			myProgramName, situation, string);
		if (error == CG_COMPILER_ERROR) {
			printf("%s\n", cgGetLastListing(myCgContext));
		}
		exit(1);
	}
}

void UseSamplerParameter(CGeffect effect, const char *paramName, GLuint texobj)
{
	CGparameter param = cgGetNamedEffectParameter(effect, paramName);

	if (!param) {
		fprintf(stderr, "%s: expected effect parameter named %s\n",
			myProgramName, paramName);
		exit(1);
	}
	cgGLSetTextureParameter(param, texobj);
	cgSetSamplerState(param);
}

int main(int argc, char **argv)
{
	glutInitWindowSize(640, 480);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInit(&argc, argv);

	glutCreateWindow(myProgramName);
	glutDisplayFunc(Display);
	glutReshapeFunc(Reshape);
	glutIdleFunc(IdleFunc);

	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "%s: failed to initialize GLEW, OpenGL 1.1 required.\n", myProgramName);    
		exit(1);
	}

	myCgContext = cgCreateContext();
	CheckForCgError("creating context");
	cgGLRegisterStates(myCgContext);
	cgGLSetManageTextureParameters(myCgContext, CG_TRUE);

	InitOpenGL();
	InitCg();
	InitContent();

	glutMainLoop();
	return 0;
}

void Reshape( int width, int height )
{
	if(height == 0)
		height = 1;

	appWidth = width;
	appHeight = height;

	float aspectRatio = (float) width / (float) height;

	BuildPerspectiveMatrix(appFiewOfView, aspectRatio, 1.0f, 1000.0f, myProjectionMatrix);

	glViewport(0,0,width,height);
}

void InitOpenGL( void )
{
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);				// This Will Clear The Background Color To Black
	glClearDepth(1.0);									// Enables Clearing Of The Depth Buffer
	glDepthFunc(GL_LESS);								// The Type Of Depth Test To Do
	glEnable(GL_DEPTH_TEST);							// Enables Depth Testing
	glShadeModel(GL_SMOOTH);							// Enables Smooth Color Shading
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations

	glEnable(GL_TEXTURE_2D);

	float aspectRatio = (float) appWidth / (float) appHeight;
	float fieldOfView = 90.0f; 
}

#define GET_PARAM(effect, name)			\
	myCgParam_##name =  cgGetNamedEffectParameter(effect, #name); \
	CheckForCgError("could not get " #name " parameter");


void InitCg()
{
	myCgEffect = cgCreateEffectFromFile(myCgContext, "SimpleEffect.cgfx", NULL);
	CheckForCgError("creating SimpleEffect.cgfx effect");
	assert(myCgEffect);

	myCgTechnique = cgGetFirstTechnique(myCgEffect);
	while (myCgTechnique && cgValidateTechnique(myCgTechnique) == CG_FALSE) {
		fprintf(stderr, "%s: Technique %s did not validate.  Skipping.\n",
			myProgramName, cgGetTechniqueName(myCgTechnique));
		myCgTechnique = cgGetNextTechnique(myCgTechnique);
	}
	if (myCgTechnique) {
		fprintf(stderr, "%s: Use technique %s.\n",
			myProgramName, cgGetTechniqueName(myCgTechnique));
	} else {
		fprintf(stderr, "%s: No valid technique\n",
			myProgramName);
		exit(1);
	}

	GET_PARAM(myCgEffect, WorldMatrix);
	GET_PARAM(myCgEffect, ViewMatrix);
	GET_PARAM(myCgEffect, ProjectionMatrix);
	GET_PARAM(myCgEffect, DiffuseMap);
	//GET_PARAM(myCgEffect, LightPosition);
	GET_PARAM(myCgEffect, LightColor);
	GET_PARAM(myCgEffect, Shadow);

	myCgParam_LightPosition =  cgGetNamedEffectParameter(myCgEffect, "LightPositon"); 
	CheckForCgError("could not get LightPositon  parameter");

}


void InitContent( void )
{
	glGenBuffers(1, &myCubeVBO);
	glBindBuffer(GL_ARRAY_BUFFER, myCubeVBO);

	SimpleVertex vertices[] = 
	{
		// Top
		{ Vector3( -1.0f,  1.0f, -1.0f ), Vector3(0, 1, 0), Vector2( 0.0f, 0.0f ) },
		{ Vector3(  1.0f,  1.0f, -1.0f ), Vector3(0, 1, 0), Vector2( 1.0f, 0.0f ) },
		{ Vector3(  1.0f,  1.0f,  1.0f ), Vector3(0, 1, 0), Vector2( 1.0f, 1.0f ) },
		{ Vector3( -1.0f,  1.0f,  1.0f ), Vector3(0, 1, 0), Vector2( 0.0f, 1.0f ) },

		// Botton
		{ Vector3( -1.0f, -1.0f, -1.0f ), Vector3(0, -1, 0), Vector2( 0.0f, 0.0f ) },
		{ Vector3(  1.0f, -1.0f, -1.0f ), Vector3(0, -1, 0), Vector2( 1.0f, 0.0f ) },
		{ Vector3(  1.0f, -1.0f,  1.0f ), Vector3(0, -1, 0), Vector2( 1.0f, 1.0f ) },
		{ Vector3( -1.0f, -1.0f,  1.0f ), Vector3(0, -1, 0), Vector2( 0.0f, 1.0f ) },

		// Left
		{ Vector3( -1.0f, -1.0f,  1.0f ), Vector3(-1, 0, 0), Vector2( 1.0f, 0.0f ) },
		{ Vector3( -1.0f, -1.0f, -1.0f ), Vector3(-1, 0, 0), Vector2( 0.0f, 0.0f ) },
		{ Vector3( -1.0f,  1.0f, -1.0f ), Vector3(-1, 0, 0), Vector2( 0.0f, 1.0f ) },
		{ Vector3( -1.0f,  1.0f,  1.0f ), Vector3(-1, 0, 0), Vector2( 1.0f, 1.0f ) },

		// Right
		{ Vector3( 1.0f, -1.0f,  1.0f ), Vector3(1, 0, 0), Vector2( 0.0f, 0.0f ) },
		{ Vector3( 1.0f, -1.0f, -1.0f ), Vector3(1, 0, 0), Vector2( 1.0f, 0.0f ) },
		{ Vector3( 1.0f,  1.0f, -1.0f ), Vector3(1, 0, 0), Vector2( 1.0f, 1.0f ) },
		{ Vector3( 1.0f,  1.0f,  1.0f ), Vector3(1, 0, 0), Vector2( 0.0f, 1.0f ) },

		// Back
		{ Vector3( -1.0f, -1.0f, -1.0f ), Vector3(0, 0, -1), Vector2( 1.0f, 0.0f ) },
		{ Vector3(  1.0f, -1.0f, -1.0f ), Vector3(0, 0, -1), Vector2( 0.0f, 0.0f ) },
		{ Vector3(  1.0f,  1.0f, -1.0f ), Vector3(0, 0, -1), Vector2( 0.0f, 1.0f ) },
		{ Vector3( -1.0f,  1.0f, -1.0f ), Vector3(0, 0, -1), Vector2( 1.0f, 1.0f ) },

		// Front 
		{ Vector3( -1.0f, -1.0f, 1.0f ), Vector3(0, 0, 1), Vector2( 0.0f, 0.0f ) },
		{ Vector3(  1.0f, -1.0f, 1.0f ), Vector3(0, 0, 1), Vector2( 1.0f, 0.0f ) },
		{ Vector3(  1.0f,  1.0f, 1.0f ), Vector3(0, 0, 1), Vector2( 1.0f, 1.0f ) },
		{ Vector3( -1.0f,  1.0f, 1.0f ), Vector3(0, 0, 1), Vector2( 0.0f, 1.0f ) },

	};

	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &myCubeIBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, myCubeIBO);

	GLubyte indices[] =
	{
		3,1,0,
		2,1,3,

		6,4,5,
		7,4,6,

		11,9,8,
		10,9,11,

		14,12,13,
		15,12,14,

		19,17,16,
		18,17,19,

		22,20,21,
		23,20,22
	};
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);



	glGenTextures(1, &myCubeTex);
	glBindTexture(GL_TEXTURE_2D, myCubeTex);
	{
		nv::Image image;
		image.loadImageFromFile("Media/Glass.dds");
		glTexImage2D(GL_TEXTURE_2D, 0, image.getInternalFormat(), image.getWidth(), 
			image.getHeight(), 0, image.getFormat(), image.getType(), image.getLevel(0));
	}


	glGenTextures(1, &myPlanTex);
	glBindTexture(GL_TEXTURE_2D, myPlanTex);
	{
		nv::Image image;
		image.loadImageFromFile("Media/Grass.dds");
		glCompressedTexImage2D(GL_TEXTURE_2D, 0, image.getInternalFormat(),
			image.getWidth(), image.getHeight(), 0, image.getImageSize(), image.getLevel(0));
	}

}


//void EyeMove()
//{
//	static float angle1=0.0f;
//	myEyePos[0]=40.0f*sin(angle1);
//	myEyePos[1]= 40.0f;
//	myEyePos[2]=40.0f*cos(angle1);
//
//	angle1+=0.01f;
//}

void LightMove()
{
	static float angle1=0.0f;
	myLightPos[0]=40*sin(angle1);
	myLightPos[1]= 40;
	myLightPos[2]=40*cos(angle1);

	angle1+=0.01f;
}

void DrawCubeModel(const Matrix4f& World)
{

	cgSetMatrixParameterfr(myCgParam_WorldMatrix, (float*)&World.m);


	CGpass pass = cgGetFirstPass(myCgTechnique);
	while (pass) {
		cgSetPassState(pass);

		glEnableClientState(GL_VERTEX_ARRAY);   
		glEnableClientState(GL_NORMAL_ARRAY);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		glBindBuffer(GL_ARRAY_BUFFER, myCubeVBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, myCubeIBO);

		glVertexPointer(3, GL_FLOAT,  sizeof(SimpleVertex), BUFFER_OFFSET(0));
		glNormalPointer(GL_FLOAT, sizeof(SimpleVertex), BUFFER_OFFSET(sizeof(Vector3)));
		glTexCoordPointer(2, GL_FLOAT, sizeof(SimpleVertex), BUFFER_OFFSET(sizeof(Vector3)*2));
		
		glDrawElements(GL_TRIANGLES, 6 * 2 * 3, GL_UNSIGNED_BYTE, 0);


		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		cgResetPassState(pass);
		pass = cgGetNextPass(pass);
	}

}


void Display( void )
{
	//EyeMove();
	LightMove();

	cgSetParameter3f(myCgParam_LightPosition, myLightPos.x, myLightPos.y, myLightPos.z);
	cgSetParameter3f(myCgParam_LightColor, myLightColor.x, myLightColor.y, myLightColor.z);



	Vector3 viewVec = Vector3(0 - myEyePos.x, 0 - myEyePos.y, 0 - myEyePos.z);
	Vector3 upVec = Vector3(0, 1, 0);
	Vector3 rightVec = cross(upVec, viewVec);
	upVec = cross(viewVec, rightVec);
	upVec = normalize(upVec);

	BuildLookAtMatrix(myEyePos.x, myEyePos.y, myEyePos.z, 
		0,0,0, upVec.x, upVec.y, upVec.z, myViewMatrix);

	cgSetMatrixParameterfr(myCgParam_ViewMatrix, (float*)&myViewMatrix.m);
	cgSetMatrixParameterfr(myCgParam_ProjectionMatrix, (float*)&myProjectionMatrix.m);


	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	


	// Not Shadow
	cgSetParameter1i(myCgParam_Shadow, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, myPlanTex);

	cgGLSetTextureParameter(myCgParam_DiffuseMap, myPlanTex);
	cgSetSamplerState(myCgParam_DiffuseMap);

	// Draw Plane
	Matrix4f planeWorld;
	planeWorld.InitIdentity();
	cgSetMatrixParameterfr(myCgParam_WorldMatrix, (float*)&planeWorld.m);

	CGpass pass = cgGetFirstPass(myCgTechnique);
	while (pass) {
		cgSetPassState(pass);

		glBegin(GL_QUADS);
		glNormal3f(0, 1, 0);

		glTexCoord2f(0, 1);
		glVertex3f(-50, 0.0f, -50);

		glTexCoord2f(0, 0);
		glVertex3f(-50, 0.0f, 50);

		glTexCoord2f(1, 0);
		glVertex3f(50, 0.0f, 50);

		glTexCoord2f(1, 1);
		glVertex3f(50, 0.0f, -50);
		glEnd();

		cgResetPassState(pass);
		pass = cgGetNextPass(pass);
	}

	//Draw Cube
	glBindTexture(GL_TEXTURE_2D, myCubeTex);
	cgGLSetTextureParameter(myCgParam_DiffuseMap, myCubeTex);
	cgSetSamplerState(myCgParam_DiffuseMap);

	Matrix4f scale, translate, world;
	MakeTranslateMatrix(0, 5, 0, translate);
	MakeScaleMatrix(5, 5, 5, scale);
	world = translate * scale;
	DrawCubeModel(world);

	cgSetParameter1i(myCgParam_Shadow, 1);

	Matrix4f shadowMatrix;
	MakePlanarShadowMatrix(0, 1, 0, 0, myLightPos.x, myLightPos.y, myLightPos.z, shadowMatrix);

	Matrix4f shadowWorld = shadowMatrix *  world;
	cgSetMatrixParameterfr(myCgParam_WorldMatrix, (float*)&shadowWorld.m);
	glEnable( GL_POLYGON_OFFSET_FILL ); 
	glPolygonOffset(  -0.1f, 0.2f  );
	DrawCubeModel(shadowWorld);
	glPolygonOffset( 0.0f, 0.0f );
	glDisable( GL_POLYGON_OFFSET_FILL );

	glutSwapBuffers();
}

void IdleFunc( void )
{
	static int lastUpdate = 0;
	static int frames = 0;
	char buf[1024];

	glutPostRedisplay(); // calls your display callback function

	int currentTime = glutGet( GLUT_ELAPSED_TIME );
	frames++;

	// is the time difference between lastUpdate and current time > one second ( 1000 ms )?
	if ( ( currentTime - lastUpdate ) >= 1000 ){

		sprintf( buf, "Planar Shadow Demo, FPS: %d", frames );
		glutSetWindowTitle( buf );
		frames = 0;
		lastUpdate = currentTime;
	}
}

