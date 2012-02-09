#include <string>
#include <vector>
#include <exception>
#include <memory>
#include <glimg/glimg.h>
#include <glimg/TextureGenerator.h>
#include <glload/gl_3_3.h>
#include <GL/freeglut.h>
#include <glutil/MatrixStack.h>
#include <glutil/MousePoles.h>
#include "../framework/framework.h"
#include "../framework/Mesh.h"
#include "../framework/Timer.h"
#include "../framework/UniformBlockArray.h"
#include "../framework/directories.h"
#include "../framework/MousePole.h"
#include "../framework/Interpolators.h"
#include "../framework/Scene.h"
#include "../framework/SceneBinders.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define ARRAY_COUNT( array ) (sizeof( array ) / (sizeof( array[0] ) * (sizeof( array ) != sizeof(void*) || sizeof( array[0] ) <= sizeof(void*))))

const float g_fzNear = 1.0f;
const float g_fzFar = 1000.0f;

const int g_projectionBlockIndex = 0;
const int g_lightBlockIndex = 1;
const int g_colorTexUnit = 0;

struct ProjectionBlock
{
	glm::mat4 cameraToClipMatrix;
};

GLuint g_projectionUniformBuffer = 0;
GLuint g_lightUniformBuffer = 0;

const int NUM_SAMPLERS = 2;
GLuint g_samplers[NUM_SAMPLERS];

void CreateSamplers()
{
	glGenSamplers(NUM_SAMPLERS, &g_samplers[0]);

	for(int samplerIx = 0; samplerIx < NUM_SAMPLERS; samplerIx++)
	{
		glSamplerParameteri(g_samplers[samplerIx], GL_TEXTURE_WRAP_S, GL_REPEAT);
		glSamplerParameteri(g_samplers[samplerIx], GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	//Linear mipmap linear
	glSamplerParameteri(g_samplers[0], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(g_samplers[0], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	//Max anisotropic
	GLfloat maxAniso = 0.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);

	glSamplerParameteri(g_samplers[1], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(g_samplers[1], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameterf(g_samplers[1], GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
}

void LoadTextures()
{
	try
	{
/*
		std::string filename(Framework::FindFileOrThrow("terrain_tex.dds"));

		std::auto_ptr<glimg::ImageSet> pImageSet(glimg::loaders::dds::LoadFromFile(filename.c_str()));

		glGenTextures(1, &g_linearTexture);
		glBindTexture(GL_TEXTURE_2D, g_linearTexture);

		glimg::OpenGLPixelTransferParams xfer = glimg::GetUploadFormatType(pImageSet->GetFormat(), 0);

		for(int mipmapLevel = 0; mipmapLevel < pImageSet->GetMipmapCount(); mipmapLevel++)
		{
			glimg::SingleImage image = pImageSet->GetImage(mipmapLevel, 0, 0);
			glimg::Dimensions dims = image.GetDimensions();

			glTexImage2D(GL_TEXTURE_2D, mipmapLevel, GL_SRGB8_ALPHA8, dims.width, dims.height, 0,
				xfer.format, xfer.type, image.GetImageData());
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, pImageSet->GetMipmapCount() - 1);

		glBindTexture(GL_TEXTURE_2D, 0);
*/
	}
	catch(std::exception &e)
	{
		printf("%s\n", e.what());
		throw;
	}
}

////////////////////////////////
//View setup.
glutil::ViewData g_initialView =
{
	glm::vec3(0.0f, 0.0f, 0.0f),
	glm::fquat(0.98481f, 0.173648f, 0.0f, 0.0f),
	15.0f,
	0.0f
};

glutil::ViewScale g_initialViewScale =
{
	5.0f, 40.0f,
	2.0f, 0.5f,
	4.0f, 1.0f,
	90.0f/250.0f
};


glutil::ViewData g_initPersView =
{
	glm::vec3(0.0f, 0.0f, 0.0f),
	glm::fquat(1.0f, 0.0f, 0.0f, 0.0f),
	5.0f,
	0.0f
};

glutil::ViewScale g_initPersViewScale =
{
	0.05f, 10.0f,
	0.1f, 0.05f,
	4.0f, 1.0f,
	90.0f/250.0f
};

glutil::ViewPole g_viewPole(g_initialView, g_initialViewScale, glutil::MB_LEFT_BTN);
glutil::ViewPole g_persViewPole(g_initPersView, g_initPersViewScale, glutil::MB_RIGHT_BTN);

namespace
{
	void MouseMotion(int x, int y)
	{
		Framework::ForwardMouseMotion(g_viewPole, x, y);
		Framework::ForwardMouseMotion(g_persViewPole, x, y);
	}

	void MouseButton(int button, int state, int x, int y)
	{
		Framework::ForwardMouseButton(g_viewPole, button, state, x, y);
		Framework::ForwardMouseButton(g_persViewPole, button, state, x, y);
	}

	void MouseWheel(int wheel, int direction, int x, int y)
	{
		Framework::ForwardMouseWheel(g_viewPole, wheel, direction, x, y);
	}
}

Framework::Scene *g_pScene = NULL;
std::vector<Framework::NodeRef> g_nodes;
Framework::Timer g_timer(Framework::Timer::TT_LOOP, 10.0f);

Framework::UniformVec4Binder g_blueBinder;
Framework::UniformVec4Binder g_redBinder;

void LoadAndSetupScene()
{
	g_nodes.clear();
	g_pScene = new Framework::Scene("dp_scene.xml");

	g_nodes.push_back(g_pScene->FindNode("blueSphere"));
	g_nodes.push_back(g_pScene->FindNode("redSphere"));

	//No more things that can throw.
	g_blueBinder.AssociateWithProgram(g_nodes[0].GetProgram(), "objectColor");
	g_blueBinder.SetValue(glm::vec4(0.3f, 0.3f, 1.0f, 1.0f));
	g_nodes[0].SetStateBinder(&g_blueBinder);
	g_redBinder.AssociateWithProgram(g_nodes[1].GetProgram(), "objectColor");
	g_redBinder.SetValue(glm::vec4(1.0f, 0.1f, 0.1f, 1.0f));
	g_nodes[1].SetStateBinder(&g_redBinder);
}

//Called after the window and OpenGL are initialized. Called exactly once, before the main loop.
void init()
{
	glutMouseFunc(MouseButton);
	glutMotionFunc(MouseMotion);
	glutMouseWheelFunc(MouseWheel);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);

	const float depthZNear = 0.0f;
	const float depthZFar = 1.0f;

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glDepthRange(depthZNear, depthZFar);
	glEnable(GL_DEPTH_CLAMP);
	glEnable(GL_FRAMEBUFFER_SRGB);

	//Setup our Uniform Buffers
	glGenBuffers(1, &g_projectionUniformBuffer);
	glBindBuffer(GL_UNIFORM_BUFFER, g_projectionUniformBuffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(ProjectionBlock), NULL, GL_DYNAMIC_DRAW);

	glBindBufferRange(GL_UNIFORM_BUFFER, g_projectionBlockIndex, g_projectionUniformBuffer,
		0, sizeof(ProjectionBlock));

	try
	{
		LoadAndSetupScene();
	}
	catch(std::exception &except)
	{
		printf("%s\n", except.what());
		throw;
	}

/*
	glGenBuffers(1, &g_lightUniformBuffer);
	glBindBuffer(GL_UNIFORM_BUFFER, g_lightUniformBuffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(LightBlock), NULL, GL_STREAM_DRAW);

	glBindBufferRange(GL_UNIFORM_BUFFER, g_lightBlockIndex, g_lightUniformBuffer,
		0, sizeof(LightBlock));

	glBindBuffer(GL_UNIFORM_BUFFER, 0);
*/

//	LoadTextures();
//	CreateSamplers();
}

using Framework::Timer;

int g_currSampler = 0;

bool g_bDrawCameraPos = false;

int g_displayWidth = 700;
int g_displayHeight = 350;

//Called to update the display.
//You should call glutSwapBuffers after all of your rendering to display what you rendered.
//If you need continuous updates of the screen, call glutPostRedisplay() at the end of the function.
void display()
{
	if(!g_pScene)
		return;

	g_timer.Update();

	glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	g_nodes[0].NodeSetOrient(glm::rotate(glm::fquat(),
		360.0f * g_timer.GetAlpha(), glm::vec3(0.0f, 1.0f, 0.0f)));

	glm::ivec2 displaySize(g_displayWidth / 2, g_displayHeight);

	{
		glutil::MatrixStack persMatrix;
		persMatrix.Perspective(60.0f, (displaySize.x / (float)displaySize.y), g_fzNear, g_fzFar);

		ProjectionBlock projData;
		projData.cameraToClipMatrix = persMatrix.Top();

		glBindBuffer(GL_UNIFORM_BUFFER, g_projectionUniformBuffer);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ProjectionBlock), &projData);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	glViewport(0, 0, (GLsizei)displaySize.x, (GLsizei)displaySize.y);
	g_pScene->Render(g_viewPole.CalcMatrix());

	{
		glutil::MatrixStack persMatrix;
		persMatrix.ApplyMatrix(glm::mat4(glm::mat3(g_persViewPole.CalcMatrix())));
		persMatrix.Perspective(60.0f, (displaySize.x / (float)displaySize.y), g_fzNear, g_fzFar);

		ProjectionBlock projData;
		projData.cameraToClipMatrix = persMatrix.Top();

		glBindBuffer(GL_UNIFORM_BUFFER, g_projectionUniformBuffer);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ProjectionBlock), &projData);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	glViewport(displaySize.x + (g_displayWidth % 2), 0,
		(GLsizei)displaySize.x, (GLsizei)displaySize.y);
	g_pScene->Render(g_viewPole.CalcMatrix());

/*
    if(!g_pLightEnv)
        return;

	glEnable(GL_FRAMEBUFFER_SRGB);

	g_pLightEnv->UpdateTime();

	glm::vec4 bgColor = g_pLightEnv->GetBackgroundColor();
	glClearColor(bgColor.x, bgColor.y, bgColor.z, bgColor.w);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glutil::MatrixStack modelMatrix;
	modelMatrix.ApplyMatrix(g_viewPole.CalcMatrix());

	LightBlock lightData = g_pLightEnv->GetLightBlock(g_viewPole.CalcMatrix());

	glBindBuffer(GL_UNIFORM_BUFFER, g_lightUniformBuffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(LightBlock), &lightData, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	if(g_pSphere && g_pTerrain)
	{
		glutil::PushStack push(modelMatrix);
		modelMatrix.RotateX(-90.0f);

		glUseProgram(g_progStandard.theProgram);
		glUniformMatrix4fv(g_progStandard.modelToCameraMatrixUnif, 1, GL_FALSE,
			glm::value_ptr(modelMatrix.Top()));
		glUniform1i(g_progStandard.numberOfLightsUnif, g_pLightEnv->GetNumLights());

		glActiveTexture(GL_TEXTURE0 + g_colorTexUnit);
		glBindTexture(GL_TEXTURE_2D, g_linearTexture);
		glBindSampler(g_colorTexUnit, g_samplers[g_currSampler]);

		g_pTerrain->Render("lit-tex");

		glBindSampler(g_colorTexUnit, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		glUseProgram(0);


		push.ResetStack();

		//Render the sun
		{
			glm::vec3 sunlightDir(g_pLightEnv->GetSunlightDirection());
			modelMatrix.Translate(sunlightDir * 500.0f);
			modelMatrix.Scale(30.0f, 30.0f, 30.0f);

			glUseProgram(g_progUnlit.theProgram);
			glUniformMatrix4fv(g_progUnlit.modelToCameraMatrixUnif, 1, GL_FALSE,
				glm::value_ptr(modelMatrix.Top()));

			glm::vec4 lightColor = g_pLightEnv->GetSunlightScaledIntensity(), gamma;
			glUniform4fv(g_progUnlit.objectColorUnif, 1, glm::value_ptr(lightColor));
			g_pSphere->Render("flat");
		}

		//Draw lights
		for(int light = 0; light < g_pLightEnv->GetNumPointLights(); light++)
		{
			push.ResetStack();

			modelMatrix.Translate(g_pLightEnv->GetPointLightWorldPos(light));

			glUseProgram(g_progUnlit.theProgram);
			glUniformMatrix4fv(g_progUnlit.modelToCameraMatrixUnif, 1, GL_FALSE,
				glm::value_ptr(modelMatrix.Top()));

			glm::vec4 lightColor = g_pLightEnv->GetPointLightScaledIntensity(light);
			glUniform4fv(g_progUnlit.objectColorUnif, 1, glm::value_ptr(lightColor));
			g_pSphere->Render("flat");
		}


		push.ResetStack();

		if(g_bDrawCameraPos)
		{
			//Draw lookat point.
			modelMatrix.SetIdentity();
			modelMatrix.Translate(glm::vec3(0.0f, 0.0f, -g_viewPole.GetView().radius));

			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);
			glUseProgram(g_progUnlit.theProgram);
			glUniformMatrix4fv(g_progUnlit.modelToCameraMatrixUnif, 1, GL_FALSE,
				glm::value_ptr(modelMatrix.Top()));
			glUniform4f(g_progUnlit.objectColorUnif, 0.25f, 0.25f, 0.25f, 1.0f);
			g_pSphere->Render("flat");
			glDepthMask(GL_TRUE);
			glEnable(GL_DEPTH_TEST);
			glUniform4f(g_progUnlit.objectColorUnif, 1.0f, 1.0f, 1.0f, 1.0f);
			g_pSphere->Render("flat");
		}
	}

	*/

    glutPostRedisplay();
	glutSwapBuffers();
}

//Called whenever the window is resized. The new window size is given, in pixels.
//This is an opportunity to call glViewport or glScissor to keep up with the change in size.
void reshape (int w, int h)
{
	g_displayWidth = w;
	g_displayHeight = h;
	glutPostRedisplay();
}


//Called whenever a key on the keyboard was pressed.
//The key is given by the ''key'' parameter, which is in ASCII.
//It's often a good idea to have the escape key (ASCII value 27) call glutLeaveMainLoop() to
//exit the program.
void keyboard(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 27:
		delete g_pScene;
		g_pScene = NULL;
		glutLeaveMainLoop();
		return;
	case 32:
		g_persViewPole.Reset();
		break;
	case 'p':
		{
			glm::mat4 tm = g_persViewPole.CalcMatrix();
			printf("%f\t%f\t%f\t%f\n", tm[0][0], tm[1][0], tm[2][0], tm[3][0]);
			printf("%f\t%f\t%f\t%f\n", tm[0][1], tm[1][1], tm[2][1], tm[3][1]);
			printf("%f\t%f\t%f\t%f\n", tm[0][2], tm[1][2], tm[2][2], tm[3][2]);
			printf("%f\t%f\t%f\t%f\n", tm[0][3], tm[1][3], tm[2][3], tm[3][3]);
		}
		break;
	case '\r':
		{
			std::auto_ptr<Framework::Scene> pOldScene(g_pScene);
			g_pScene = NULL;
			std::vector<Framework::NodeRef> tmpNodes;
			tmpNodes.swap(g_nodes);
			try
			{
				LoadAndSetupScene();
			}
			catch(std::exception &except)
			{
				printf("Failed to reload, due to: %s\n", except.what());
				if(g_pScene)
					delete g_pScene;
				g_pScene = pOldScene.release();
				g_nodes.swap(tmpNodes);
				return;
			}
		}
		break;
	}

	g_viewPole.CharPress(key);
}

unsigned int defaults(unsigned int displayMode, int &width, int &height)
{
	width = g_displayWidth;
	height = g_displayHeight;
	return displayMode | GLUT_SRGB;
}