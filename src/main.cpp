#include "gl_util.hpp"

/*
  GLM
*/
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "lodepng.h"

#include "uv_mapper/uv_mapper.hpp"

#include <sstream>
#include <fstream>

using std::string;
using std::vector;
using glm::vec3;

/*
  Global variables below.
*/

vector<float> vertices;
vector<float> normals;
vector<float> uvs;

vector<int> faces;

vector<float> edgeTriangles;


// vbos for mesh.
GLuint indexVbo;
GLuint vertexVbo;
GLuint normalVbo;
GLuint uvVbo;

// vbo for edges.
GLuint edgeVbo; // vbo for edge vertices.

string meshfile;

GLuint checkerTexture;
GLuint customTexture;
GLuint blackTexture;
GLuint currentTexture;

GLuint vao;

int viewMode = 1;

const int WINDOW_WIDTH = 960;
const int WINDOW_HEIGHT = 650;

GLFWwindow* window;

float cameraYaw = 4.65f;
float cameraPitch = 1.37f;
float cameraZoom = 6.3f;
glm::vec3 cameraPos;
glm::mat4 viewMatrix;
glm::mat4 projectionMatrix;

GLuint normalShader;
GLuint edgeShader;

double prevMouseX = 0;
double prevMouseY = 0;

double curMouseX = 0;
double curMouseY = 0;

const int RENDER_SPECULAR = 0;
const int RENDER_PROCEDURAL_TEXTURE = 1;

float g_MouseWheel = 0.0;

void ScrollCallback(GLFWwindow*, double /*xoffset*/, double yoffset)
{
    cameraZoom += yoffset;
}

/*
  Update view matrix according pitch and yaw. Is called every frame.
*/
void UpdateViewMatrix() {

    glm::mat4 cameraTransform;

    cameraTransform = glm::rotate(cameraTransform, cameraYaw, glm::vec3(0.f, 1.f, 0.f)); // add yaw
    cameraTransform = glm::rotate(cameraTransform, cameraPitch, glm::vec3(0.f, 0.f, 1.f)); // add pitch

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 center(0.0f, 0.0f, 0.0f);
    cameraPos = glm::vec3(cameraTransform * glm::vec4(cameraZoom, 0.0, 0.0, 1.0));

    viewMatrix = glm::lookAt(
	cameraPos,
	center,
	up
	);
}

bool FileExists(const char *fileName){
    std::ifstream infile(fileName);
    return infile.good();
}


void TakeScreenshot() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);


    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    unsigned char* pixels = new unsigned char[ 3 * width * height];
    int fbWidth, fbHeight;

    // read pixels.
    glReadPixels((GLint)0, (GLint)0,
                 (GLint)width, (GLint)height,
                 GL_RGB, GL_UNSIGNED_BYTE, pixels);

    // filter transparent values.
    unsigned char* mypixels = new unsigned char[ 4 * width * height];
    int p = 0;
    for(int i = 0; i < 3*width*height; i+=3) {
        mypixels[p++] = pixels[i+0];
        mypixels[p++] = pixels[i+1];
        mypixels[p++] = pixels[i+2];

        if(pixels[i+0] == 51 && pixels[i+1] == 51 && pixels[i+2]==153)
            mypixels[p++] = 0;//pixels[i+3];
        else
            mypixels[p++] = 255;
    }

    // flip image.
    unsigned int* intPixels = (unsigned int*)mypixels;
    for (int i=0;i<width;++i){
	for (int j=0;j<height/2;++j){
	    unsigned int temp = intPixels[j * width + i];

	    intPixels[j * width + i] = intPixels[ (height-j-1)*width + i];
	    intPixels[(height-j-1)*width + i] = temp;
        }
    }
    mypixels = (unsigned char*)intPixels;

    // find screenshot filename.
    int i = 0;
    string filename;
    while(true) {
        filename = "screenshot_" + std::to_string(i) + ".png";
        if(!FileExists(filename.c_str()))
            break;
        i++;
    }
    // finally save.
    lodepng_encode32_file(filename.c_str(), mypixels, width, height);

    printf("Saved screenshot to %s\n", filename.c_str());
}

void LoadMesh(void) {
    using namespace std;

    string inputfile = meshfile;

    ifstream file(inputfile.c_str());

    if(!file.is_open()){
        printf("ERROR: could not open obj file %s\n", inputfile.c_str());
        exit(1);
    }

    // parse the obj file:
    string token;
    while(!file.eof()) {
        token = "";
        file >> token;

        if(token == "v") { // vertex.
            float x, y, z;
            file >> x >> y >> z;
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }

        else if(token=="f")
        {
            string line;
            getline(file, line);

            istringstream stream(line);
            vector<string> vertices;
            copy(istream_iterator<string>(stream),
                 istream_iterator<string>(),
                 back_inserter(vertices));

            if(vertices.size() != 3) {
                printf("ERROR: Found primitive with %ld vertices. But only meshes with only triangles are accepted!", vertices.size());
                exit(1);
            }

            for(size_t i=0 ; i< 3 ; ++i) {
                string::size_type _pos = vertices[i].find('/', 0);
                string _indexStr = vertices[i].substr(0, _pos);

                faces.push_back(stoi(_indexStr)-1);
            }
        }
    }

    float xmin = FLT_MAX;
    float ymin = FLT_MAX;
    float zmin = FLT_MAX;

    float xmax = -FLT_MAX;
    float ymax = -FLT_MAX;
    float zmax = -FLT_MAX;

    // center the mesh.
    for(int i = 0; i < vertices.size(); i+=3) {
        float x = vertices[i + 0];
        float y = vertices[i + 1];
        float z = vertices[i + 2];

        if(x > xmax) xmax = x;
        if(y > ymax) ymax = y;
        if(z > zmax) zmax = z;

        if(x < xmin) xmin = x;
        if(y < ymin) ymin = y;
        if(z < zmin) zmin = z;
    }
    float xcenter = (xmin + xmax) * 0.5f;
    float ycenter = (ymin + ymax) * 0.5f;
    float zcenter = (zmin + zmax) * 0.5f;
    for(int i = 0; i < vertices.size(); i+=3) {
        vertices[i + 0] -= xcenter;
        vertices[i + 1] -= ycenter;
        vertices[i + 2] -= zcenter;
    }

    vector<float> inVertices = vertices;
    vector<int> inFaces = faces;

    faces.clear();
    vertices.clear();

    vector<float> uvEdges;
    // compute uv map.
    uvMap(
        inVertices, inFaces,
        vertices, faces, uvs, &uvEdges);

    for(int i = 0; i < vertices.size(); i++) {
        normals.push_back(0);
    }

    // estimate normals from mesh.
    for(int i = 0; i < faces.size(); i+=3) {
        int i0 = faces[i + 0];
        int i1 = faces[i + 1];
        int i2 = faces[i + 2];

        glm::vec3 p0(
            vertices[i0 * 3 + 0],
            vertices[i0 * 3 + 1],
            vertices[i0 * 3 + 2]
            );

        glm::vec3 p1(
            vertices[i1 * 3 + 0],
            vertices[i1 * 3 + 1],
            vertices[i1 * 3 + 2]
            );

        glm::vec3 p2(
            vertices[i2 * 3 + 0],
            vertices[i2 * 3 + 1],
            vertices[i2 * 3 + 2]
            );

        glm::vec3 n = glm::normalize(glm::cross(glm::normalize(p2 - p0), glm::normalize(p1 - p0)));

        normals[i0 * 3 + 0] += n.x;
        normals[i0 * 3 + 1] += n.y;
        normals[i0 * 3 + 2] += n.z;

        normals[i1 * 3 + 0] += n.x;
        normals[i1 * 3 + 1] += n.y;
        normals[i1 * 3 + 2] += n.z;

        normals[i2 * 3 + 0] += n.x;
        normals[i2 * 3 + 1] += n.y;
        normals[i2 * 3 + 2] += n.z;
    }
    for(int i = 0; i < vertices.size(); i+=3) {
        glm::vec3 n(
            normals[i + 0],
            normals[i + 1],
            normals[i + 2]
            );

        n = glm::normalize(n);

        normals[i + 0] = n.x;
        normals[i + 1] = n.y;
        normals[i + 2] = n.z;
    }

    //
    // Upload the model to OpenGL.
    //

    GL_C(glGenBuffers(1, &indexVbo));
    GL_C(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVbo));
    GL_C(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)* faces.size(), faces.data(), GL_STATIC_DRAW));

    GL_C(glGenBuffers(1, &vertexVbo));
    GL_C(glBindBuffer(GL_ARRAY_BUFFER, vertexVbo));
    GL_C(glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*vertices.size(), vertices.data() , GL_STATIC_DRAW));

    GL_C(glGenBuffers(1, &normalVbo));
    GL_C(glBindBuffer(GL_ARRAY_BUFFER, normalVbo));
    GL_C(glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*normals.size(), normals.data() , GL_STATIC_DRAW));

    GL_C(glGenBuffers(1, &uvVbo));
    GL_C(glBindBuffer(GL_ARRAY_BUFFER, uvVbo));
    GL_C(glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*uvs.size(), uvs.data() , GL_STATIC_DRAW));

    // we render the uv edges as triangles:
    for(int i = 0; i < uvEdges.size(); i+=4) {
        glm::vec2 p0(
            uvEdges[i+0],
            uvEdges[i+1]
            );

        glm::vec2 p1(
            uvEdges[i+2],
            uvEdges[i+3]
            );

        glm::vec2 v = p1 - p0;

        glm::vec2 n(v.y, -v.x);
        n = glm::normalize(n);

        float U = 0.003;

        glm::vec2 a = p0 + n * U;
        glm::vec2 b = p1 + n * U;

        glm::vec2 c = p1 - n * U;
        glm::vec2 d = p0 - n * U;

        edgeTriangles.push_back(a.x); edgeTriangles.push_back(a.y);
        edgeTriangles.push_back(c.x); edgeTriangles.push_back(c.y);
        edgeTriangles.push_back(b.x); edgeTriangles.push_back(b.y);

        edgeTriangles.push_back(a.x); edgeTriangles.push_back(a.y);
        edgeTriangles.push_back(d.x); edgeTriangles.push_back(d.y);
        edgeTriangles.push_back(c.x); edgeTriangles.push_back(c.y);

    }

    GL_C(glGenBuffers(1, &edgeVbo));
    GL_C(glBindBuffer(GL_ARRAY_BUFFER, edgeVbo));
    GL_C(glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*edgeTriangles.size(), edgeTriangles.data() , GL_STATIC_DRAW));
}

GLuint CreateTexture(unsigned int width, unsigned int height, unsigned char* data) {
    GLuint texture;
    GL_C(glGenTextures(1, &texture));

    // we only need one texture, so we bind here, and keep it bound for the rest of the program.
    GL_C(glBindTexture(GL_TEXTURE_2D, texture));

    GL_C(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));

    GL_C(glActiveTexture(GL_TEXTURE0));

    GL_C(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    GL_C(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));

    GL_C(glGenerateMipmap(GL_TEXTURE_2D));

    GL_C(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
    GL_C(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    return texture;
}

GLuint LoadTexture(const std::string& filename) {
    std::vector<unsigned char> buffer;
    lodepng::load_file(buffer, filename.c_str());

    lodepng::State state;
    unsigned int width;
    unsigned int height;
    std::vector<unsigned char> imageData;
    unsigned error = lodepng::decode(imageData, width, height, state, buffer);

    if (error != 0){
        printf("Could not load image %s: %s\n",
               filename.c_str(),
               lodepng_error_text(error));
        exit(1);
    }

    return CreateTexture(width, height,imageData.data());
}

void InitGlfw() {
    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "TESS", NULL, NULL);
    if (! window ) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetScrollCallback(window, ScrollCallback);

    glfwMakeContextCurrent(window);

    // load GLAD.
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    // Bind and create VAO, otherwise, we can't do anything in OpenGL.
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GL_C(glDisable(GL_CULL_FACE));
    GL_C(glEnable(GL_DEPTH_TEST));
}

void Render() {
    int fbWidth, fbHeight;
    int wWidth, wHeight;

    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glfwGetWindowSize(window, &wWidth, &wHeight);

    // important that we do this, otherwise it won't work on retina!
    float ratio = fbWidth / (float)wWidth; //

    // a tiny left part of the window is dedicated to GUI. So shift the viewport to the right some.
    GL_C(glViewport(0, 0, fbWidth, fbHeight));
    GL_C(glClearColor(0.2f, 0.2f, 0.6f, 1.0f));
    GL_C(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    // update matrices.
    UpdateViewMatrix();
    glm::mat4 MVP = projectionMatrix * viewMatrix;

    if(viewMode ==1) { // render mdesh
        GLuint shader = normalShader;
        GL_C(glUseProgram(shader));

        GL_C(glUniformMatrix4fv(glGetUniformLocation(shader, "uMvp"), 1, GL_FALSE, glm::value_ptr(MVP) ));
        GL_C(glBindTexture(GL_TEXTURE_2D, currentTexture));
        GL_C(glActiveTexture(GL_TEXTURE0));
        GL_C(glUniform1i(glGetUniformLocation(shader, "uTexture"), 0));
        GL_C(glUniform1i(glGetUniformLocation(shader, "uViewMode"), viewMode));


        GL_C(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVbo));

        GL_C(glEnableVertexAttribArray(0));
        GL_C(glBindBuffer(GL_ARRAY_BUFFER, vertexVbo));
        GL_C(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0));

        GL_C(glEnableVertexAttribArray(1));
        GL_C(glBindBuffer(GL_ARRAY_BUFFER, normalVbo));
        GL_C(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0));

        GL_C(glEnableVertexAttribArray(2));
        GL_C(glBindBuffer(GL_ARRAY_BUFFER, uvVbo));
        GL_C(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void*)0));

        GL_C(glDrawElements(
                 GL_TRIANGLES,
                 faces.size() , GL_UNSIGNED_INT, 0));

    } else { // render uv edges.
        GLuint shader = edgeShader;
        GL_C(glUseProgram(shader));

        MVP = projectionMatrix * glm::lookAt(
            glm::vec3(0,0,2.5),
            glm::vec3(0,0,0),
            glm::vec3(0,1,0)
            );

        GL_C(glUniformMatrix4fv(glGetUniformLocation(shader, "uMvp"), 1, GL_FALSE, glm::value_ptr(MVP) ));

        GL_C(glEnableVertexAttribArray(0));
        GL_C(glBindBuffer(GL_ARRAY_BUFFER, edgeVbo));
        GL_C(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0));

        GL_C(glDrawArrays(GL_TRIANGLES, 0, edgeTriangles.size()/2));
    }
}


void HandleInput() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        exit(1);
    }

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        viewMode = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        viewMode = 2;
    }

    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        currentTexture = checkerTexture;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
        currentTexture = customTexture;
    }
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
        currentTexture = blackTexture;
    }

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        TakeScreenshot();
    }

    prevMouseX = curMouseX;
    prevMouseY = curMouseY;
    glfwGetCursorPos(window, &curMouseX, &curMouseY);


    float MOUSE_SENSITIVITY = 0.005;

    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);

    // we change yaw and pitch by dragging with the mouse.
    if (state == GLFW_PRESS) {
        cameraYaw += (curMouseX - prevMouseX ) * MOUSE_SENSITIVITY;
        cameraPitch += (curMouseY - prevMouseY ) * MOUSE_SENSITIVITY;
    }
}

void PrintHelp() {
    printf("Usage:");
    printf("auto_uv: [--texture=name] name");
    exit(0);
}

int main(int argc, char** argv) {
    std::string customTextureFile = "";
    if(argc == 1) {
        PrintHelp();
    } else {
        // parse texture argument.
        if(std::string(argv[1]).substr(0,10) == "--texture=") {
            customTextureFile = std::string(argv[1]).substr(10);
        }

        // last one is always mesh file.
        meshfile = std::string(argv[argc-1]);
    }

    printf("Keyboard commands:\n");

    printf("   p\tTake screenshot:\n");
    printf("   1\tView uv-mapped mesh:\n");
    printf("   2\tView uv map:\n");
    printf("   3\tUse checker texture:\n");
    printf("   4\tUse custom texture specified in command line arguments:\n");
    printf("   5\tUse no texture:\n");

    InitGlfw();

    normalShader =  LoadNormalShader(
        "#version 330\n"
        "layout(location = 0) in vec3 vsPos;"
        "layout(location = 1) in vec3 vsNormal;"
        "layout(location = 2) in vec2 vsUv;"

        "out vec3 fsPos;"
        "out vec3 fsNormal;"
        "out vec2 fsUv;"

        "uniform mat4 uMvp;"

        "void main()"
        "{"
        "    fsPos = vsPos;"
        "    fsNormal = vsNormal;"
        "    fsUv = vsUv;"

        "    gl_Position = uMvp * vec4(vsPos, 1.0);"
        "}"
        ,

        "#version 330\n"
        "in vec3 fsPos;"
        "in vec3 fsNormal;"
        "in vec2 fsUv;"

        "out vec3 color;"


        "uniform sampler2D uTexture;"

        "void main()"
        "{"
        "vec3 n = normalize(fsNormal);"
        "vec3 l = normalize(vec3(+.0, -0.43, -0.5));"

        "vec3 diff = vec3(clamp(dot(n, l), 0.0, 1.0 )  ) ;"

        " color =  texture( uTexture, fsUv ).xyz * 0.4 + 0.4* diff;"

        "}"
        );

    edgeShader =  LoadNormalShader(
        "#version 330\n"
        "layout(location = 0) in vec2 vsPos;"
        "uniform mat4 uMvp;"
        "void main()"
        "{"
        "    gl_Position = uMvp * vec4(vsPos, 0.0, 1.0);"
        "}"
        ,

        "#version 330\n"
        "out vec3 color;"

        "void main()"
        "{"
        "color = vec3(0.0);"
        "}"
        );


    // setup projection matrix.
    projectionMatrix = glm::perspective(0.9f, (float)(WINDOW_WIDTH) / WINDOW_HEIGHT, 0.1f, 200.0f);

    LoadMesh();

    unsigned char checkerTextureData[4 * 256 * 256];

    // procedural checker texture.
    int p = 0;
    for(int i = 0; i < 256; i++)  {
        for(int j = 0; j < 256; j++)  {
            int x = i / 32;
            int y = j / 32;

            int a = (x + y) % 2 == 0 ? 255 : 0;
            checkerTextureData[p++] = a;
            checkerTextureData[p++] = a;
            checkerTextureData[p++] = a;
            checkerTextureData[p++] = 255;
        }
    }
    checkerTexture = CreateTexture(256, 256, checkerTextureData);

    // all black texture.
    unsigned char blackTextureData[4];
    blackTextureData[0] = 0;
    blackTextureData[1] = 0;
    blackTextureData[2] = 0;
    blackTextureData[3] = 0;
    blackTexture = CreateTexture(1, 1, blackTextureData);

    if(customTextureFile != "") {
        customTexture = LoadTexture(customTextureFile);
    } else {
        customTexture = blackTexture;
    }
    currentTexture = checkerTexture;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

	Render();

	HandleInput();

	// set window title.
	string windowTitle =  "Teapot render time: ";
	glfwSetWindowTitle(window, windowTitle.c_str());

        /* display and process events through callbacks */
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    exit(EXIT_SUCCESS);
}
