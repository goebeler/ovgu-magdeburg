// ******* GPU Histogram ********

#include <windows.h>

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <iostream>
#include <string>
#include <fstream>
#include <cmath>
#include <vector>

using namespace std;

// Bildaufloesung
#define PIC_WIDTH 512
#define PIC_HEIGHT 512

#define PI 3.141592f

#define ROTATE 1
#define MOVE 2

float center[3] = { 0.0f, 0.0f, 0.0f };
GLfloat viewPosition[4] = { 0.0, 0.0, 3.0, 1.0 };
GLfloat viewDirection[4] = { -0.0, -0.0, -1.0, 0.0 };
GLfloat viewAngle = 45.0f;
GLfloat viewNear = 0.01f;
GLfloat viewFar = 10000.0f;

float thetaStart = PI / 2.0f;
float phiStart = PI / 2.0f;
float rStart = 3.5f;

float theta = thetaStart;
float phi = phiStart;
float r = rStart;

float oldX, oldY;
int motionState;

// Texture Ids
GLuint imageTextureId = 0;
GLuint depthTextureId = 0;
GLuint histogramTextureId = 0;

// Framebuffer Object Ids
GLuint createImageFB = 0;
GLuint createHistogramFB = 0;

// GLSL Variables (Shader Ids, Locations, ...)
GLuint vertexShaderCreateHistogram;
GLuint fragmentShaderCreateHistogram;
GLuint shaderProgramCreateHistogram;

GLint imageTextureLocation;

//// - - - selbst angelegte Variabeln - - -
//
//// Einfache Liste von Eckpunkt-Daten -> z.B.Vertices, Normals, Colors oder Texturkoordinaten
//GLuint vboHistogram;
GLuint uniformProjection;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// Print information about the compiling step
void printShaderInfoLog(GLuint shader)
{
	GLint infologLength = 0;
	GLsizei charsWritten = 0;
	char *infoLog;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infologLength);
	infoLog = (char *)malloc(infologLength);
	glGetShaderInfoLog(shader, infologLength, &charsWritten, infoLog);
	printf("%s\n", infoLog);
	free(infoLog);
}

// Print information about the linking step
void printProgramInfoLog(GLuint program)
{
	GLint infoLogLength = 0;
	GLsizei charsWritten = 0;
	char *infoLog;

	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
	infoLog = (char *)malloc(infoLogLength);
	glGetProgramInfoLog(program, infoLogLength, &charsWritten, infoLog);
	printf("%s\n", infoLog);
	free(infoLog);
}

// Reads a file and returns the content as a string
string readFile(string fileName)
{
	string fileContent;
	string line;

	ifstream file(fileName.c_str());
	if (file.is_open()) {
		while (!file.eof()){
			getline(file, line);
			line += "\n";
			fileContent += line;
		}
		file.close();
	}
	else
		cout << "ERROR: Unable to open file " << fileName << endl;

	return fileContent;
}


// Hilfsfunktion um Vertex & Fragment Shader einzuladen 
void loadShaderProgram(GLuint &shaderProgram, GLuint &vertexShader, char* vertexShaderName, GLuint &fragmentShader, char* fragmentShaderName)
{
	// Create empty shader object (vertex shader)
	vertexShader = glCreateShader(GL_VERTEX_SHADER);

	// Read vertex shader source 
	string shaderSource = readFile(vertexShaderName);
	const char* sourcePtr = shaderSource.c_str();

	// Attach shader code
	glShaderSource(vertexShader, 1, &sourcePtr, NULL);

	// Compile
	glCompileShader(vertexShader);
	printShaderInfoLog(vertexShader);

	// Create empty shader object (fragment shader)
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Read vertex shader source 
	shaderSource = readFile(fragmentShaderName);
	sourcePtr = shaderSource.c_str();

	// Attach shader code
	glShaderSource(fragmentShader, 1, &sourcePtr, NULL);

	// Compile
	glCompileShader(fragmentShader);
	printShaderInfoLog(fragmentShader);

	// Create shader program
	shaderProgram = glCreateProgram();

	// Attach shader
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);

	// Link program
	glLinkProgram(shaderProgram);
	printProgramInfoLog(shaderProgram);
}


// alle Texturen und FBOs anlegen
int initFBOTextures()
{
	// Textur anlegen
	glGenTextures(1, &imageTextureId);
	glBindTexture(GL_TEXTURE_2D, imageTextureId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PIC_WIDTH, PIC_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	// Depth Buffer Textur anlegen 
	glGenTextures(1, &depthTextureId);
	glBindTexture(GL_TEXTURE_2D, depthTextureId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, PIC_WIDTH, PIC_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// FBO anlegen und Texturen zuweisen
	glGenFramebuffers(1, &createImageFB);
	glBindFramebuffer(GL_FRAMEBUFFER, createImageFB);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, imageTextureId, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTextureId, 0);

	// Texture f�r Histogrammdaten
	glGenTextures(1, &histogramTextureId);
	glBindTexture(GL_TEXTURE_2D, histogramTextureId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 256, 1, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// FBO f�r Histogrammdaten Textur
	glGenFramebuffers(1, &createHistogramFB);
	glBindFramebuffer(GL_FRAMEBUFFER, createHistogramFB);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, histogramTextureId, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, imageTextureId);


	// check framebuffer status
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	switch (status)
	{
	case GL_FRAMEBUFFER_COMPLETE:
		cout << "FBO complete" << endl;
		break;
	case GL_FRAMEBUFFER_UNSUPPORTED:
		cout << "FBO configuration unsupported" << endl;
		return 1;
	default:
		cout << "FBO programmer error" << endl;
		return 1;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return 0;
}

// calc the view position and direction from theta/phi coordinates
void calcViewerCamera(float theta, float phi, float r)
{
	float x = r * sin(theta) * cos(phi);
	float y = r * cos(theta);
	float z = r * sin(theta) * sin(phi);

	viewPosition[0] = center[0] + x;
	viewPosition[1] = center[1] + y;
	viewPosition[2] = center[2] + z;
	viewDirection[0] = -x;
	viewDirection[1] = -y;
	viewDirection[2] = -z;
}

// Alle Shader einladen & uniform Variablen setzen
void initGLSL()
{
	loadShaderProgram(shaderProgramCreateHistogram, vertexShaderCreateHistogram, "create_histogram.vert", fragmentShaderCreateHistogram, "create_histogram.frag");
	glUseProgram(shaderProgramCreateHistogram);

	imageTextureLocation = glGetUniformLocation(shaderProgramCreateHistogram, "imageTexture");
	glUniform1i(imageTextureLocation, 0);

	// gleichzeitig Vertex Buffer Objects initialisieren
	// Hinweis: Die Erzeugung eiens VBOs vollzieht sich in 4 Schritten
	// - Generieren eines Buffer Handles
	// - Binden des Buffers (Target = GL_ARRAY_BUFFER)
	// - Buffer Daten in den VRAM kopieren (Usage = GL_STATIC_DRAW)
	// - Buffer nicht mehr binden (stattdessen eine 0 binden).	-> lueckenlos

	//// Data fuer BufferData anlegen und berechnen
	//GLfloat* vertexArray = new GLfloat[PIC_WIDTH*PIC_HEIGHT*2*4];  // 2 float-Werte pro Pixel // pro Pixel 4 Eckpunkte
	//// Array fuellen
	//int index = 0;
	//for (int x = 0; x < PIC_WIDTH; x++)
	//{
	//	for (int y = 0; y < PIC_HEIGHT; y++)
	//	{
	//		index = (x*PIC_WIDTH + y) * 2*4;

	//		// x und y casten zur Vermeidung von Fehlern Division
	//		float floatX = static_cast<float>(x);
	//		float floatY = static_cast<float>(y);

	//		// Eckpunkt links unten // je 2 float Werte fuer die Position berechnen
	//		vertexArray[index] = floatX / PIC_WIDTH; // teilen durch Breite bzw. Hoehe um im Bereich 0 .. 1 zu bleiben
	//		vertexArray[index + 1] = floatY / PIC_HEIGHT;

	//		// Eckpunkt rechts unten
	//		vertexArray[index + 4] = (floatX + 1) / PIC_WIDTH;
	//		vertexArray[index + 5] = floatY / PIC_HEIGHT;

	//		// Eckpunkt rechts oben
	//		vertexArray[index + 6] = (floatX + 1) / PIC_WIDTH;
	//		vertexArray[index + 7] = (floatY + 1) / PIC_HEIGHT;

	//		// Eckpunkt links oben
	//		vertexArray[index + 2] = floatX / PIC_WIDTH;
	//		vertexArray[index + 3] = (floatY + 1) / PIC_HEIGHT;
	//	}
	//}

	//glGenBuffers(1, &vboHistogram); // erzeugt 1 VBO und liefert dessen ID im Array vboHistogram
	//glBindBuffer(GL_ARRAY_BUFFER, vboHistogram); // aktiviert VBO
	//glBufferData(GL_ARRAY_BUFFER, (PIC_HEIGHT*PIC_WIDTH*2*4)*sizeof(float), vertexArray, GL_STATIC_DRAW); // Daten in VBO eintragen
	//glVertexPointer(3, GL_FLOAT, 0, NULL);
}

void drawScene()
{
	// Tiefentest und Beleuchtung anschalten.
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);

	// Perspektivische Projektionsmatrix verwenden.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(viewAngle, 1.0f, viewNear, viewFar);

	// View-Matrix setzen
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	calcViewerCamera(theta, phi, r);
	gluLookAt(viewPosition[0], viewPosition[1], viewPosition[2],
		viewPosition[0] + viewDirection[0], viewPosition[1] + viewDirection[1], viewPosition[2] + viewDirection[2],
		0, 1, 0);

	// Teekanne mit Fixed-Function-Pipeline rendern.
	glUseProgram(0);
	glutSolidTeapot(1.0);
}

// OpenGL display Funktion
void display()
{
	float hPixels[256];

	int timeStart = glutGet(GLUT_ELAPSED_TIME);

	// ********* Teekanne in FBO rendern **********

	// FBO binden, in das gerendert werden soll.
	glBindFramebuffer(GL_FRAMEBUFFER, createImageFB);      // activate fbo                 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, imageTextureId);

	// Teekanne rendern.
	drawScene();

	// ********* Histogrammdaten erzeugen, indem f�r jedes Pixel ein Vertex gerendert wird. Der Vertex Shader liest den Farbwert aus und berechnet seinen Position, abh�ngig von der Helligkeit des gelesenen Pixels. **********

	glUseProgram(shaderProgramCreateHistogram);
	// Orthografische Projektion nutzen, damit die Vertices auch korrekt auf die Pixel fallen.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, PIC_WIDTH, 0, PIC_HEIGHT, -1, 1);

	// Die Uniform Location der Projektionsmatrix bestimmen & benutzerdefinierte Variable in Shader laden - -
	float projMatrix[16];
	glGetFloatv(GL_PROJECTION_MATRIX, projMatrix);

	uniformProjection = glGetUniformLocation(shaderProgramCreateHistogram, "Projection");
	glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, projMatrix);
	// - - - - - - -

	// Einheitsmatrix als Model-View Matrix verwenden. (Blick in z-Richtung)
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// TODO: Additives Blending aktivieren (mit jedem ankommenden Pixel wird der Counter um 1 erh�ht)
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE); // immer 1/255 draufaddieren

	// TODO: Tiefentest und Beleuchtung abschalten
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);

	// TODO: Histogram-Shader f�r jedes Pixel des Bildes ausf�hren.
	// Eckpunkte pro Pixel zeichnen
	//glBegin(GL_POINTS);
	//for (int x = 0; x < PIC_WIDTH; x++)
	//{
	//	for (int y = 0; y < PIC_HEIGHT; y++)
	//	{
	//		glVertex2f((float)x, (float)y);
	//	}
	//}
	//glEnd();
	glBegin(GL_POINTS);
	for (int i = 0; i < PIC_WIDTH * PIC_HEIGHT; i++)
	{
		glVertex2f(i % PIC_WIDTH, (i / PIC_WIDTH));
	}
	glEnd();
	glUseProgram(0);

	// Histogramm-Daten von VRAM zu RAM streamen (in das Array hPixels) -> ausgelesen wird nur der Rotanteil
	glReadPixels(0, 0, 256, 1, GL_RED, GL_FLOAT, hPixels); // statt GL_R sowie oben

	// TODO: Blending abschalten	
	glDisable(GL_BLEND);

	// ********* Teekanne in Backbuffer rendern **********

	// Rendern in das FBO beenden. Fortan wird wieder in den Backbuffer gerendert.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);       // deactivate fbo
	// Backbuffer wieder leeren.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// Szene in den Backbuffer rendern.
	drawScene();

	// ********* Histogramm zeichnen **********

	// TODO: Rendern Sie die Liniensegmente. Sie k�nnen ModelView- und Projection-Matrix auf 
	// die Einheitsmatrix setzen und die Vertices direkt im Clipping-Space an die GPU schicken
	// oder alternativ eine Projektionsmatrix bauen, die es Ihnen erlaubt, die Positionen in
	// Bildschirmkoordinaten (0..PIC_WIDTH-1, 0..PIC_HEIGHT-1) anzugeben.

	glUseProgram(shaderProgramCreateHistogram);

	// alles wie oben benutzen
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, PIC_WIDTH, 0, PIC_HEIGHT, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glUseProgram(0);

	// Linien zeichnen
	glBegin(GL_LINES);
	for (unsigned int i = 0; i < 255; ++i)
	{
		glColor3f(1, 1, 1);
		// Anfangspunkt am unteren Bildrand
		glVertex2f(i, 0);
		// Endpunkt = addierte Werte aus hPixels
		glVertex2f(i, hPixels[i] * 256); // statt 255, damit Histogramm nicht in der Mitte abgeschnitten wird
	}
	glEnd();

	// Frame ist beendet, Buffer swappen.
	glutSwapBuffers();

	// Verstrichene Zeit ausgeben.
	int timeEnd = glutGet(GLUT_ELAPSED_TIME);
	printf("Delay %4d\r", timeEnd - timeStart);
}

// use a virtual trackball as mouse control
void mouseMotion(int x, int y)
{
	float deltaX = x - oldX;
	float deltaY = y - oldY;


	if (motionState == ROTATE) {
		theta -= 0.002f * deltaY;

		if (theta < 0.002f) theta = 0.002f;
		else if (theta > PI - 0.002f) theta = PI - 0.002f;

		phi += 0.002f * deltaX;
		if (phi < 0) phi += 2 * PI;
		else if (phi > 2 * PI) phi -= 2 * PI;
	}
	else if (motionState == MOVE) {
		r += 0.01f * deltaY;
	}


	oldX = (float)x;
	oldY = (float)y;

	glutPostRedisplay();
}

void mouse(int button, int state, int x, int y)
{
	oldX = (float)x;
	oldY = (float)y;

	if (button == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) {
			motionState = ROTATE;
		}
	}
	else if (button == GLUT_RIGHT_BUTTON) {
		if (state == GLUT_DOWN) {
			motionState = MOVE;
		}
	}
}

int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE | GLUT_MULTISAMPLE);
	glutInitWindowSize(PIC_WIDTH, PIC_HEIGHT);
	glutCreateWindow("GPU Histogram");

	// Init glew so that the GLSL functionality will be available
	if (glewInit() != GLEW_OK)
		cout << "GLEW init failed!" << endl;

	initFBOTextures();
	initGLSL();

	glutMotionFunc(mouseMotion);
	glutMouseFunc(mouse);
	glutDisplayFunc(display);
	glutIdleFunc(display);

	glViewport(0, 0, PIC_WIDTH, PIC_HEIGHT);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glEnable(GL_DEPTH_TEST);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(viewAngle, 1.0, viewNear, viewFar);
	glMatrixMode(GL_MODELVIEW);

	glutMainLoop();

	return 0;
}