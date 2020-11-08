// 지원되는 GLS 셰이더는 단편 Blinn-Phong 및 일반 매핑별로 제공된다.
// 둘 다 OpenGL 기본 음의 z축을 비추는 단일 고정 방향 광원만 지원한다.

// OBJ 파일을 런타임에 로드할 수 있도록 최소 파일 메뉴가 제공된다.
// 데모에는 OBJ 파일에 대한 드래그 앤 드롭 지원이 포함된다.
// OBJ 파일을 데모 창에 떨어뜨리면 OBJ 파일이 로드된다.
// 이 데모에는 완전히 자기자신이 담겨 있다. 
// 두 개의 GLS 셰이더는 리소스로 데모 EXE 파일에 내장되어 있다.

// 마우스 왼쪽 버튼을 클릭하고 끌어서 모델을 번역하십시오.
// 마우스를 사용하여 모델을 확대/축소할 때 중간 버튼을 클릭하고 끄십시오.
// 마우스 오른쪽 버튼을 클릭하고 마우스를 사용하여 모델을 회전하십시오.

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <commdlg.h>	// for open file dialog box
#include <shellapi.h>   // for drag and drop support
#include <GL/gl.h>
#include <GL/glu.h>
#include <cassert>
#include <cmath>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_DEBUG)
#include <crtdbg.h>
#endif

#include "bitmap.h"
#include "gl2.h"
#include "model_obj.h"
#include "resource.h"
#include "WGL_ARB_multisample.h"

//-----------------------------------------------------------------------------
// Constants.
//-----------------------------------------------------------------------------

#define APP_TITLE "OpenGL OBJ Viewer"

// Windows Vista compositing support.
#if !defined(PFD_SUPPORT_COMPOSITION)
#define PFD_SUPPORT_COMPOSITION 0x00008000
#endif

// GL_EXT_texture_filter_anisotropic
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

#define CAMERA_FOVY  60.0f
#define CAMERA_ZFAR  10.0f
#define CAMERA_ZNEAR 0.1f

#define MOUSE_ORBIT_SPEED 0.30f     // 0 = SLOWEST, 1 = FASTEST
#define MOUSE_DOLLY_SPEED 0.02f     // same as above...but much more sensitive
#define MOUSE_TRACK_SPEED 0.005f    // same as above...but much more sensitive

//-----------------------------------------------------------------------------
// Type definitions.
//-----------------------------------------------------------------------------

typedef std::map<std::string, GLuint> ModelTextures;

//-----------------------------------------------------------------------------
// Globals.
//-----------------------------------------------------------------------------

HWND                g_hWnd;
HDC                 g_hDC;
HGLRC               g_hRC;
HINSTANCE           g_hInstance;
int                 g_framesPerSecond;
int                 g_windowWidth;
int                 g_windowHeight;
int                 g_msaaSamples;
GLuint              g_nullTexture;
GLuint              g_blinnPhongShader;
GLuint              g_normalMappingShader;
float               g_maxAnisotrophy;
float               g_heading;
float               g_pitch;
float               g_cameraPos[3];
float               g_targetPos[3];
bool                g_isFullScreen;
bool                g_hasFocus;
bool                g_enableWireframe;
bool                g_enableTextures = true;
bool                g_supportsProgrammablePipeline;
bool                g_cullBackFaces = true;
ModelOBJ            g_model;
ModelTextures       g_modelTextures;

//-----------------------------------------------------------------------------
// Functions Prototypes.
//-----------------------------------------------------------------------------

void    Cleanup();
void    CleanupApp();
GLuint  CompileShader(GLenum type, const GLchar *pszSource, GLint length);
HWND    CreateAppWindow(const WNDCLASSEX &wcl, const char *pszTitle);
GLuint  CreateNullTexture(int width, int height);
void    DrawFrame();
void    DrawModelUsingFixedFuncPipeline();
void    DrawModelUsingProgrammablePipeline();
bool    ExtensionSupported(const char *pszExtensionName);
float   GetElapsedTimeInSeconds();
bool    Init();
void    InitApp();
void    InitGL();
GLuint  LinkShaders(GLuint vertShader, GLuint fragShader);
void    LoadModel(const char *pszFilename);
GLuint  LoadShaderProgramFromResource(const char *pResouceId, std::string &infoLog);
GLuint  LoadTexture(const char *pszFilename);
void    Log(const char *pszMessage);
void    ProcessMenu(HWND hWnd, WPARAM wParam, LPARAM lParam);
void    ProcessMouseInput(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void    ReadTextFileFromResource(const char *pResouceId, std::string &buffer);
void    ResetCamera();
void    SetProcessorAffinity();
void    ToggleFullScreen();
void    UnloadModel();
void    UpdateFrame(float elapsedTimeSec);
void    UpdateFrameRate(float elapsedTimeSec);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//-----------------------------------------------------------------------------
// Functions.
//-----------------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
#if defined _DEBUG
    _CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    MSG msg = {0};
    WNDCLASSEX wcl = {0};

    wcl.cbSize = sizeof(wcl);
    wcl.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wcl.lpfnWndProc = WindowProc;
    wcl.cbClsExtra = 0;
    wcl.cbWndExtra = 0;
    wcl.hInstance = g_hInstance = hInstance;
    wcl.hIcon = LoadIcon(0, IDI_APPLICATION);
    wcl.hCursor = LoadCursor(0, IDC_ARROW);
    wcl.hbrBackground = 0;
    wcl.lpszMenuName = MAKEINTRESOURCE(MENU_FIXED_FUNC);
    wcl.lpszClassName = "GLWindowClass";
    wcl.hIconSm = 0;

    if (!RegisterClassEx(&wcl))
        return 0;

    g_hWnd = CreateAppWindow(wcl, APP_TITLE);

    if (g_hWnd)
    {
        SetProcessorAffinity();

        if (Init())
        {
            ShowWindow(g_hWnd, nShowCmd);
            UpdateWindow(g_hWnd);

            while (true)
            {
                while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
                {
                    if (msg.message == WM_QUIT)
                        break;

                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

                if (msg.message == WM_QUIT)
                    break;

                if (g_hasFocus)
                {
                    UpdateFrame(GetElapsedTimeInSeconds());
                    DrawFrame();
                    SwapBuffers(g_hDC);
                }
                else
                {
                    WaitMessage();
                }
            }
        }

        Cleanup();
        UnregisterClass(wcl.lpszClassName, hInstance);
    }

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static char szFilename[MAX_PATH];

    switch (msg)
    {
    case WM_ACTIVATE:
        switch (wParam)
        {
        default:
            break;

        case WA_ACTIVE:
        case WA_CLICKACTIVE:
            g_hasFocus = true;
            break;

        case WA_INACTIVE:
            if (g_isFullScreen)
                ShowWindow(hWnd, SW_MINIMIZE);
            g_hasFocus = false;
            break;
        }
        break;

    case WM_CHAR:
        switch (static_cast<int>(wParam))
        {
        case VK_ESCAPE:
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;

        case 'r':
        case 'R':
            PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(MENU_VIEW_RESET, 0), 0);
            break;

        case 't':
        case 'T':
            PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(MENU_VIEW_TEXTURED, 0), 0);
            break;

        case 'w':
        case 'W':
            PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(MENU_VIEW_WIREFRAME, 0), 0);
            break;

        default:
            break;
        }
        break;

    case WM_COMMAND:
        ProcessMenu(hWnd, wParam, lParam);
        return 0;

    case WM_CREATE:
        DragAcceptFiles(hWnd, TRUE);
        break;

    case WM_DESTROY:
        DragAcceptFiles(hWnd, FALSE);
        PostQuitMessage(0);
        return 0;

    case WM_DROPFILES:
        DragQueryFile(reinterpret_cast<HDROP>(wParam), 0, szFilename, MAX_PATH);
        DragFinish(reinterpret_cast<HDROP>(wParam));

        try
        {
            if (strstr(szFilename, ".obj") || strstr(szFilename, ".OBJ"))
            {
                UnloadModel();
                LoadModel(szFilename);
                ResetCamera();
            }
            else
            {
                throw std::runtime_error("File is not a valid .OBJ file");
            }            
        }
        catch (const std::runtime_error &e)
        {
            Log(e.what());
        }
        return 0;

    case WM_SIZE:
        g_windowWidth = static_cast<int>(LOWORD(lParam));
        g_windowHeight = static_cast<int>(HIWORD(lParam));
        break;

    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN)
            PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(MENU_VIEW_FULLSCREEN, 0), 0);
        break;

    default:
        ProcessMouseInput(hWnd, msg, wParam, lParam);
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void Cleanup()
{
    CleanupApp();

    if (g_hDC)
    {
        if (g_hRC)
        {
            wglMakeCurrent(g_hDC, 0);
            wglDeleteContext(g_hRC);
            g_hRC = 0;
        }

        ReleaseDC(g_hWnd, g_hDC);
        g_hDC = 0;
    }
}

void CleanupApp()
{
    UnloadModel();

    if (g_nullTexture)
    {
        glDeleteTextures(1, &g_nullTexture);
        g_nullTexture = 0;
    }

    if (g_supportsProgrammablePipeline)
    {
        glUseProgram(0);

        if (g_blinnPhongShader)
        {
            glDeleteProgram(g_blinnPhongShader);
            g_blinnPhongShader = 0;
        }

        if (g_normalMappingShader)
        {
            glDeleteProgram(g_normalMappingShader);
            g_normalMappingShader = 0;
        }
    }
}

GLuint CompileShader(GLenum type, const GLchar *pszSource, GLint length)
{
    // Compiles the shader given it's source code. Returns the shader object.
    // A std::string object containing the shader's info log is thrown if the
    // shader failed to compile.
    //
    // 'type' is either GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
    // 'pszSource' is a C style string containing the shader's source code.
    // 'length' is the length of 'pszSource'.

    GLuint shader = glCreateShader(type);

    if (shader)
    {
        GLint compiled = 0;

        glShaderSource(shader, 1, &pszSource, &length);
        glCompileShader(shader);
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

        if (!compiled)
        {
            GLsizei infoLogSize = 0;
            std::string infoLog;

            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogSize);
            infoLog.resize(infoLogSize);
            glGetShaderInfoLog(shader, infoLogSize, &infoLogSize, &infoLog[0]);

            throw infoLog;
        }
    }

    return shader;
}

HWND CreateAppWindow(const WNDCLASSEX &wcl, const char *pszTitle)
{
    // Create a window that is centered on the desktop. It's exactly 1/4 the
    // size of the desktop. Don't allow it to be resized.

    DWORD wndExStyle = WS_EX_OVERLAPPEDWINDOW;
    DWORD wndStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
        WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

    HWND hWnd = CreateWindowEx(wndExStyle, wcl.lpszClassName, pszTitle,
        wndStyle, 0, 0, 0, 0, 0, 0, wcl.hInstance, 0);

    if (hWnd)
    {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int halfScreenWidth = screenWidth / 2;
        int halfScreenHeight = screenHeight / 2;
        int left = (screenWidth - halfScreenWidth) / 2;
        int top = (screenHeight - halfScreenHeight) / 2;
        RECT rc = {0};

        SetRect(&rc, left, top, left + halfScreenWidth, top + halfScreenHeight);
        AdjustWindowRectEx(&rc, wndStyle, FALSE, wndExStyle);
        MoveWindow(hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

        GetClientRect(hWnd, &rc);
        g_windowWidth = rc.right - rc.left;
        g_windowHeight = rc.bottom - rc.top;
    }

    return hWnd;
}

GLuint CreateNullTexture(int width, int height)
{
    // Create an empty white texture. This texture is applied to OBJ models
    // that don't have any texture maps. This trick allows the same shader to
    // be used to draw the OBJ model with and without textures applied.

    int pitch = ((width * 32 + 31) & ~31) >> 3; // align to 4-byte boundaries
    std::vector<GLubyte> pixels(pitch * height, 255);
    GLuint texture = 0;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA,
        GL_UNSIGNED_BYTE, &pixels[0]);

    return texture;
}

void DrawFrame()
{
    glViewport(0, 0, g_windowWidth, g_windowHeight);
    glClearColor(0.3f, 0.5f, 0.9f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(CAMERA_FOVY,
        static_cast<float>(g_windowWidth) / static_cast<float>(g_windowHeight),
        CAMERA_ZNEAR, CAMERA_ZFAR);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(g_cameraPos[0], g_cameraPos[1], g_cameraPos[2],
        g_targetPos[0], g_targetPos[1], g_targetPos[2],
        0.0f, 1.0f, 0.0f);

    glRotatef(g_pitch, 1.0f, 0.0f, 0.0f);
    glRotatef(g_heading, 0.0f, 1.0f, 0.0f);

    if (g_supportsProgrammablePipeline)
        DrawModelUsingProgrammablePipeline();
    else
        DrawModelUsingFixedFuncPipeline();
}

void DrawModelUsingFixedFuncPipeline()
{
    const ModelOBJ::Mesh *pMesh = 0;
    const ModelOBJ::Material *pMaterial = 0;
    const ModelOBJ::Vertex *pVertices = 0;
    ModelTextures::const_iterator iter;

    for (int i = 0; i < g_model.getNumberOfMeshes(); ++i)
    {
        pMesh = &g_model.getMesh(i);
        pMaterial = pMesh->pMaterial;
        pVertices = g_model.getVertexBuffer();

        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, pMaterial->ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pMaterial->diffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, pMaterial->specular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, pMaterial->shininess * 128.0f);

        if (g_enableTextures)
        {
            iter = g_modelTextures.find(pMaterial->colorMapFilename);

            if (iter == g_modelTextures.end())
            {
                glDisable(GL_TEXTURE_2D);
            }
            else
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, iter->second);
            }
        }
        else
        {
            glDisable(GL_TEXTURE_2D);
        }

        if (g_model.hasPositions())
        {
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(3, GL_FLOAT, g_model.getVertexSize(),
                g_model.getVertexBuffer()->position);
        }

        if (g_model.hasTextureCoords())
        {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, g_model.getVertexSize(),
                g_model.getVertexBuffer()->texCoord);
        }

        if (g_model.hasNormals())
        {
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, g_model.getVertexSize(),
                g_model.getVertexBuffer()->normal);
        }

        glDrawElements(GL_TRIANGLES, pMesh->triangleCount * 3, GL_UNSIGNED_INT,
            g_model.getIndexBuffer() + pMesh->startIndex);

        if (g_model.hasNormals())
            glDisableClientState(GL_NORMAL_ARRAY);

        if (g_model.hasTextureCoords())
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);

        if (g_model.hasPositions())
            glDisableClientState(GL_VERTEX_ARRAY);
    }
}

void DrawModelUsingProgrammablePipeline()
{
    const ModelOBJ::Mesh *pMesh = 0;
    const ModelOBJ::Material *pMaterial = 0;
    const ModelOBJ::Vertex *pVertices = 0;
    ModelTextures::const_iterator iter;
    GLuint texture = 0;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < g_model.getNumberOfMeshes(); ++i)
    {
        pMesh = &g_model.getMesh(i);
        pMaterial = pMesh->pMaterial;
        pVertices = g_model.getVertexBuffer();

        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, pMaterial->ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pMaterial->diffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, pMaterial->specular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, pMaterial->shininess * 128.0f);

        if (pMaterial->bumpMapFilename.empty())
        {
            // Per fragment Blinn-Phong code path.

            glUseProgram(g_blinnPhongShader);

            // Bind the color map texture.

            texture = g_nullTexture;

            if (g_enableTextures)
            {
                iter = g_modelTextures.find(pMaterial->colorMapFilename);

                if (iter != g_modelTextures.end())
                    texture = iter->second;
            }

            glActiveTexture(GL_TEXTURE0);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);

            // Update shader parameters.

            glUniform1i(glGetUniformLocation(
                g_blinnPhongShader, "colorMap"), 0);
            glUniform1f(glGetUniformLocation(
                g_blinnPhongShader, "materialAlpha"), pMaterial->alpha);
        }
        else
        {
            // Normal mapping code path.

            glUseProgram(g_normalMappingShader);

            // Bind the normal map texture.

            iter = g_modelTextures.find(pMaterial->bumpMapFilename);

            if (iter != g_modelTextures.end())
            {
                glActiveTexture(GL_TEXTURE1);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, iter->second);
            }

            // Bind the color map texture.

            texture = g_nullTexture;

            if (g_enableTextures)
            {
                iter = g_modelTextures.find(pMaterial->colorMapFilename);

                if (iter != g_modelTextures.end())
                    texture = iter->second;
            }

            glActiveTexture(GL_TEXTURE0);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);

            // Update shader parameters.

            glUniform1i(glGetUniformLocation(
                g_normalMappingShader, "colorMap"), 0);
            glUniform1i(glGetUniformLocation(
                g_normalMappingShader, "normalMap"), 1);
            glUniform1f(glGetUniformLocation(
                g_normalMappingShader, "materialAlpha"), pMaterial->alpha);
        }        

        // Render mesh.

        if (g_model.hasPositions())
        {
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(3, GL_FLOAT, g_model.getVertexSize(),
                g_model.getVertexBuffer()->position);
        }

        if (g_model.hasTextureCoords())
        {
            glClientActiveTexture(GL_TEXTURE0);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, g_model.getVertexSize(),
                g_model.getVertexBuffer()->texCoord);
        }

        if (g_model.hasNormals())
        {
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, g_model.getVertexSize(),
                g_model.getVertexBuffer()->normal);
        }

        if (g_model.hasTangents())
        {
            glClientActiveTexture(GL_TEXTURE1);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(4, GL_FLOAT, g_model.getVertexSize(),
                g_model.getVertexBuffer()->tangent);
        }

        glDrawElements(GL_TRIANGLES, pMesh->triangleCount * 3, GL_UNSIGNED_INT,
            g_model.getIndexBuffer() + pMesh->startIndex);

        if (g_model.hasTangents())
        {
            glClientActiveTexture(GL_TEXTURE1);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        }

        if (g_model.hasNormals())
            glDisableClientState(GL_NORMAL_ARRAY);

        if (g_model.hasTextureCoords())
        {
            glClientActiveTexture(GL_TEXTURE0);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        }

        if (g_model.hasPositions())
            glDisableClientState(GL_VERTEX_ARRAY);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

bool ExtensionSupported(const char *pszExtensionName)
{
    static const char *pszGLExtensions = 0;
    static const char *pszWGLExtensions = 0;

    if (!pszGLExtensions)
        pszGLExtensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));

    if (!pszWGLExtensions)
    {
        // WGL_ARB_extensions_string.

        typedef const char *(WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC);

        PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB =
            reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(
            wglGetProcAddress("wglGetExtensionsStringARB"));

        if (wglGetExtensionsStringARB)
            pszWGLExtensions = wglGetExtensionsStringARB(g_hDC);
    }

    if (!strstr(pszGLExtensions, pszExtensionName))
    {
        if (!strstr(pszWGLExtensions, pszExtensionName))
            return false;
    }

    return true;
}

float GetElapsedTimeInSeconds()
{
    // Returns the elapsed time (in seconds) since the last time this function
    // was called. This elaborate setup is to guard against large spikes in
    // the time returned by QueryPerformanceCounter().

    static const int MAX_SAMPLE_COUNT = 50;

    static float frameTimes[MAX_SAMPLE_COUNT];
    static float timeScale = 0.0f;
    static float actualElapsedTimeSec = 0.0f;
    static INT64 freq = 0;
    static INT64 lastTime = 0;
    static int sampleCount = 0;
    static bool initialized = false;

    INT64 time = 0;
    float elapsedTimeSec = 0.0f;

    if (!initialized)
    {
        initialized = true;
        QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&freq));
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&lastTime));
        timeScale = 1.0f / freq;
    }

    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&time));
    elapsedTimeSec = (time - lastTime) * timeScale;
    lastTime = time;

    if (fabsf(elapsedTimeSec - actualElapsedTimeSec) < 1.0f)
    {
        memmove(&frameTimes[1], frameTimes, sizeof(frameTimes) - sizeof(frameTimes[0]));
        frameTimes[0] = elapsedTimeSec;

        if (sampleCount < MAX_SAMPLE_COUNT)
            ++sampleCount;
    }

    actualElapsedTimeSec = 0.0f;

    for (int i = 0; i < sampleCount; ++i)
        actualElapsedTimeSec += frameTimes[i];

    if (sampleCount > 0)
        actualElapsedTimeSec /= sampleCount;

    return actualElapsedTimeSec;
}

bool Init()
{
    try
    {
        InitGL();
        InitApp();
        return true;
    }
    catch (const std::exception &e)
    {
        std::ostringstream msg;

        msg << "Application initialization failed!" << std::endl << std::endl;
        msg << e.what();

        Log(msg.str().c_str());
        return false;
    }    
}

void InitApp()
{
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (g_supportsProgrammablePipeline)
    {
        std::string infoLog;

        if (!(g_blinnPhongShader = LoadShaderProgramFromResource(
            reinterpret_cast<const char *>(SHADER_BLINN_PHONG), infoLog)))
            throw std::runtime_error("Failed to load Blinn-Phong shader.\n" + infoLog);

        if (!(g_normalMappingShader = LoadShaderProgramFromResource(
            reinterpret_cast<const char *>(SHADER_NORMAL_MAPPING), infoLog)))
            throw std::runtime_error("Failed to load normal mapping shader.\n" + infoLog);

        if (!(g_nullTexture = CreateNullTexture(2, 2)))
            throw std::runtime_error("Failed to create null texture.");
    }

    if (__argc == 2)
    {
        LoadModel(__argv[1]);
        ResetCamera();
    }
}

void InitGL()
{
    if (!(g_hDC = GetDC(g_hWnd)))
        throw std::runtime_error("GetDC() failed.");

    int pf = 0;
    PIXELFORMATDESCRIPTOR pfd = {0};
    OSVERSIONINFO osvi = {0};

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    if (!GetVersionEx(&osvi))
        throw std::runtime_error("GetVersionEx() failed.");

    // When running under Windows Vista or later support composition.
    if (osvi.dwMajorVersion > 6 || (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion >= 0))
        pfd.dwFlags |=  PFD_SUPPORT_COMPOSITION;

    ChooseBestMultiSampleAntiAliasingPixelFormat(pf, g_msaaSamples);

    if (!pf)
        pf = ChoosePixelFormat(g_hDC, &pfd);

    if (!SetPixelFormat(g_hDC, pf, &pfd))
        throw std::runtime_error("SetPixelFormat() failed.");

    if (!(g_hRC = wglCreateContext(g_hDC)))
        throw std::runtime_error("wglCreateContext() failed.");

    if (!wglMakeCurrent(g_hDC, g_hRC))
        throw std::runtime_error("wglMakeCurrent() failed.");

    GL2Init();

    g_supportsProgrammablePipeline = GL2SupportsGLVersion(2, 0);

    // Check for GL_EXT_texture_filter_anisotropic support.
    if (ExtensionSupported("GL_EXT_texture_filter_anisotropic"))
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &g_maxAnisotrophy);
    else
        g_maxAnisotrophy = 1.0f;
}

GLuint LinkShaders(GLuint vertShader, GLuint fragShader)
{
    // Links the compiled vertex and/or fragment shaders into an executable
    // shader program. Returns the executable shader object. If the shaders
    // failed to link into an executable shader program, then a std::string
    // object is thrown containing the info log.

    GLuint program = glCreateProgram();

    if (program)
    {
        GLint linked = 0;

        if (vertShader)
            glAttachShader(program, vertShader);

        if (fragShader)
            glAttachShader(program, fragShader);

        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &linked);

        if (!linked)
        {
            GLsizei infoLogSize = 0;
            std::string infoLog;

            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogSize);
            infoLog.resize(infoLogSize);
            glGetProgramInfoLog(program, infoLogSize, &infoLogSize, &infoLog[0]);

            throw infoLog;
        }

        // Mark the two attached shaders for deletion. These two shaders aren't
        // deleted right now because both are already attached to a shader
        // program. When the shader program is deleted these two shaders will
        // be automatically detached and deleted.

        if (vertShader)
            glDeleteShader(vertShader);

        if (fragShader)
            glDeleteShader(fragShader);
    }

    return program;
}

void LoadModel(const char *pszFilename)
{
    // Import the OBJ file and normalize to unit length.

    SetCursor(LoadCursor(0, IDC_WAIT));

    if (!g_model.import(pszFilename))
    {
        SetCursor(LoadCursor(0, IDC_ARROW));
        throw std::runtime_error("Failed to load model.");
    }

    g_model.normalize();

    // Load any associated textures.
    // Note the path where the textures are assumed to be located.

    const ModelOBJ::Material *pMaterial = 0;
    GLuint textureId = 0;
    std::string::size_type offset = 0;
    std::string filename;

    for (int i = 0; i < g_model.getNumberOfMaterials(); ++i)
    {
        pMaterial = &g_model.getMaterial(i);

        // Look for and load any diffuse color map textures.

        if (pMaterial->colorMapFilename.empty())
            continue;

        // Try load the texture using the path in the .MTL file.
        textureId = LoadTexture(pMaterial->colorMapFilename.c_str());

        if (!textureId)
        {
            offset = pMaterial->colorMapFilename.find_last_of('\\');

            if (offset != std::string::npos)
                filename = pMaterial->colorMapFilename.substr(++offset);
            else
                filename = pMaterial->colorMapFilename;

            // Try loading the texture from the same directory as the OBJ file.
            textureId = LoadTexture((g_model.getPath() + filename).c_str());
        }

        if (textureId)
            g_modelTextures[pMaterial->colorMapFilename] = textureId;

        // Look for and load any normal map textures.

        if (pMaterial->bumpMapFilename.empty())
            continue;

        // Try load the texture using the path in the .MTL file.
        textureId = LoadTexture(pMaterial->bumpMapFilename.c_str());

        if (!textureId)
        {
            offset = pMaterial->bumpMapFilename.find_last_of('\\');

            if (offset != std::string::npos)
                filename = pMaterial->bumpMapFilename.substr(++offset);
            else
                filename = pMaterial->bumpMapFilename;

            // Try loading the texture from the same directory as the OBJ file.
            textureId = LoadTexture((g_model.getPath() + filename).c_str());
        }

        if (textureId)
            g_modelTextures[pMaterial->bumpMapFilename] = textureId;
    }

    SetCursor(LoadCursor(0, IDC_ARROW));

    // Update the window caption.

    std::ostringstream caption;
    const char *pszBareFilename = strrchr(pszFilename, '\\');

    pszBareFilename = (pszBareFilename != 0) ? ++pszBareFilename : pszFilename;
    caption << APP_TITLE << " - " << pszBareFilename;

    SetWindowText(g_hWnd, caption.str().c_str());
}

GLuint LoadShaderProgramFromResource(const char *pResouceId, std::string &infoLog)
{
    infoLog.clear();

    GLuint program = 0;
    std::string buffer;

    // Read the text file containing the GLSL shader program.
    // This file contains 1 vertex shader and 1 fragment shader.
    ReadTextFileFromResource(pResouceId, buffer);

    // Compile and link the vertex and fragment shaders.
    if (buffer.length() > 0)
    {
        const GLchar *pSource = 0;
        GLint length = 0;
        GLuint vertShader = 0;
        GLuint fragShader = 0;

        std::string::size_type vertOffset = buffer.find("[vert]");
        std::string::size_type fragOffset = buffer.find("[frag]");

        try
        {
            // Get the vertex shader source and compile it.
            // The source is between the [vert] and [frag] tags.
            if (vertOffset != std::string::npos)
            {
                vertOffset += 6;        // skip over the [vert] tag
                pSource = reinterpret_cast<const GLchar *>(&buffer[vertOffset]);
                length = static_cast<GLint>(fragOffset - vertOffset);
                vertShader = CompileShader(GL_VERTEX_SHADER, pSource, length);
            }

            // Get the fragment shader source and compile it.
            // The source is between the [frag] tag and the end of the file.
            if (fragOffset != std::string::npos)
            {
                fragOffset += 6;        // skip over the [frag] tag
                pSource = reinterpret_cast<const GLchar *>(&buffer[fragOffset]);
                length = static_cast<GLint>(buffer.length() - fragOffset - 1);
                fragShader = CompileShader(GL_FRAGMENT_SHADER, pSource, length);
            }

            // Now link the vertex and fragment shaders into a shader program.
            program = LinkShaders(vertShader, fragShader);
        }
        catch (const std::string &errors)
        {
            infoLog = errors;
        }
    }

    return program;
}

GLuint LoadTexture(const char *pszFilename)
{
    GLuint id = 0;
    Bitmap bitmap;

    if (bitmap.loadPicture(pszFilename))
    {
        // The Bitmap class loads images and orients them top-down.
        // OpenGL expects bitmap images to be oriented bottom-up.
        bitmap.flipVertical();

        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (g_maxAnisotrophy > 1.0f)
        {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                g_maxAnisotrophy);
        }

        gluBuild2DMipmaps(GL_TEXTURE_2D, 4, bitmap.width, bitmap.height,
            GL_BGRA_EXT, GL_UNSIGNED_BYTE, bitmap.getPixels());
    }

    return id;
}

void Log(const char *pszMessage)
{
    MessageBox(0, pszMessage, "Error", MB_ICONSTOP);
}

//MFC 메뉴
void ProcessMenu(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    static char szFilename[MAX_PATH] = {'\0'};
    static OPENFILENAME ofn;

    switch (LOWORD(wParam))
    {
    case MENU_FILE_OPEN:	//파일 오픈 메뉴
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hWnd;
        ofn.lpstrFilter = "Alias|Wavefront (*.OBJ)\0*.obj\0";
        ofn.lpstrCustomFilter = 0;
        ofn.nFilterIndex = 1;
        ofn.lpstrFile = szFilename;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = "Open File";
        ofn.lpstrFileTitle = 0;
        ofn.lpstrDefExt = 0;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_READONLY | OFN_PATHMUSTEXIST;

        if (GetOpenFileName(reinterpret_cast<LPOPENFILENAME>(&ofn)))
        {
            UnloadModel();
            LoadModel(szFilename);
            ResetCamera();
        }

        break;

    case MENU_FILE_CLOSE:	//파일 닫기 메뉴
        UnloadModel();
        break;

    case MENU_FILE_EXIT:	//파일 나가기 메뉴
        SendMessage(hWnd, WM_CLOSE, 0, 0);
        break;

    case MENU_VIEW_FULLSCREEN:	//전체 화면 메뉴
        ToggleFullScreen();

        if (g_isFullScreen)
            CheckMenuItem(GetMenu(hWnd), MENU_VIEW_FULLSCREEN, MF_CHECKED);
        else
            CheckMenuItem(GetMenu(hWnd), MENU_VIEW_FULLSCREEN, MF_UNCHECKED);
        break;

    case MENU_VIEW_RESET:	//화면 리셋 메뉴
        ResetCamera();
        break;

    case MENU_VIEW_CULLBACKFACES:	//다시 불러오기 메뉴
        if (g_cullBackFaces = !g_cullBackFaces)
        {
            glEnable(GL_CULL_FACE);
            CheckMenuItem(GetMenu(hWnd), MENU_VIEW_CULLBACKFACES, MF_CHECKED);
        }
        else
        {
            glDisable(GL_CULL_FACE);
            CheckMenuItem(GetMenu(hWnd), MENU_VIEW_CULLBACKFACES, MF_UNCHECKED);
        }
        break;

    case MENU_VIEW_TEXTURED:	//텍스쳐 메뉴
        if (g_enableTextures = !g_enableTextures)
            CheckMenuItem(GetMenu(hWnd), MENU_VIEW_TEXTURED, MF_CHECKED);
        else
            CheckMenuItem(GetMenu(hWnd), MENU_VIEW_TEXTURED, MF_UNCHECKED);
        break;

    case MENU_VIEW_WIREFRAME:	//와이어프레임 뷰 메뉴
        if (g_enableWireframe = !g_enableWireframe)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            CheckMenuItem(GetMenu(hWnd), MENU_VIEW_WIREFRAME, MF_CHECKED);
        }
        else
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            CheckMenuItem(GetMenu(hWnd), MENU_VIEW_WIREFRAME, MF_UNCHECKED);
        }
        break;

    default:
        break;
    }
}

//마우스 관련 함수
void ProcessMouseInput(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Use the left mouse button to track the camera.
    // Use the middle mouse button to dolly the camera.
    // Use the right mouse button to orbit the camera.

    enum CameraMode {CAMERA_NONE, CAMERA_TRACK, CAMERA_DOLLY, CAMERA_ORBIT};

    static CameraMode cameraMode = CAMERA_NONE;
    static POINT ptMousePrev = {0};
    static POINT ptMouseCurrent = {0};
    static int mouseButtonsDown = 0;
    static float dx = 0.0f;
    static float dy = 0.0f;

    switch (msg)
    {
    case WM_LBUTTONDOWN:
        cameraMode = CAMERA_TRACK;
        ++mouseButtonsDown;
        SetCapture(hWnd);
        ptMousePrev.x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
        ptMousePrev.y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
        ClientToScreen(hWnd, &ptMousePrev);
        break;

    case WM_RBUTTONDOWN:
        cameraMode = CAMERA_ORBIT;
        ++mouseButtonsDown;
        SetCapture(hWnd);
        ptMousePrev.x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
        ptMousePrev.y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
        ClientToScreen(hWnd, &ptMousePrev);
        break;

    case WM_MBUTTONDOWN:
        cameraMode = CAMERA_DOLLY;
        ++mouseButtonsDown;
        SetCapture(hWnd);
        ptMousePrev.x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
        ptMousePrev.y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
        ClientToScreen(hWnd, &ptMousePrev);
        break;

    case WM_MOUSEMOVE:
        ptMouseCurrent.x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
        ptMouseCurrent.y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
        ClientToScreen(hWnd, &ptMouseCurrent);

        switch (cameraMode)
        {
        case CAMERA_TRACK:
            dx = static_cast<float>(ptMouseCurrent.x - ptMousePrev.x);
            dx *= MOUSE_TRACK_SPEED;

            dy = static_cast<float>(ptMouseCurrent.y - ptMousePrev.y);
            dy *= MOUSE_TRACK_SPEED;

            g_cameraPos[0] -= dx;
            g_cameraPos[1] += dy;

            g_targetPos[0] -= dx;
            g_targetPos[1] += dy;

            break;

        case CAMERA_DOLLY:
            dy = static_cast<float>(ptMouseCurrent.y - ptMousePrev.y);
            dy *= MOUSE_DOLLY_SPEED;

            g_cameraPos[2] -= dy;

            if (g_cameraPos[2] < g_model.getRadius() + CAMERA_ZNEAR)
                g_cameraPos[2] = g_model.getRadius() + CAMERA_ZNEAR;

            if (g_cameraPos[2] > CAMERA_ZFAR - g_model.getRadius())
                g_cameraPos[2] = CAMERA_ZFAR - g_model.getRadius();

            break;

        case CAMERA_ORBIT:
            dx = static_cast<float>(ptMouseCurrent.x - ptMousePrev.x);
            dx *= MOUSE_ORBIT_SPEED;

            dy = static_cast<float>(ptMouseCurrent.y - ptMousePrev.y);
            dy *= MOUSE_ORBIT_SPEED;

            g_heading += dx;
            g_pitch += dy;

            if (g_pitch > 90.0f)
                g_pitch = 90.0f;

            if (g_pitch < -90.0f)
                g_pitch = -90.0f;

            break;
        }

        ptMousePrev.x = ptMouseCurrent.x;
        ptMousePrev.y = ptMouseCurrent.y;
        break;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        if (--mouseButtonsDown <= 0)
        {
            mouseButtonsDown = 0;
            cameraMode = CAMERA_NONE;
            ReleaseCapture();
        }
        else
        {
            if (wParam & MK_LBUTTON)
                cameraMode = CAMERA_TRACK;
            else if (wParam & MK_RBUTTON)
                cameraMode = CAMERA_ORBIT;
            else if (wParam & MK_MBUTTON)
                cameraMode = CAMERA_DOLLY;
        }
        break;

    default:
        break;
    }
}

//텍스쳐 파일 소스 읽어오는 함수
void ReadTextFileFromResource(const char *pResouceId, std::string &buffer)
{
    HMODULE hModule = GetModuleHandle(0);
    HRSRC hResource = FindResource(hModule, pResouceId, RT_RCDATA);

    if (hResource)
    {
        DWORD dwSize = SizeofResource(hModule, hResource);
        HGLOBAL hGlobal = LoadResource(hModule, hResource);

        if (hGlobal)
        {
            if (LPVOID pData = LockResource(hGlobal))
            {
                buffer.assign(reinterpret_cast<const char *>(pData), dwSize);
                UnlockResource(hGlobal);
            }
        }
    }
}

//카메라 리셋 함수
void ResetCamera()
{
    g_model.getCenter(g_targetPos[0], g_targetPos[1], g_targetPos[2]);

    g_cameraPos[0] = g_targetPos[0];
    g_cameraPos[1] = g_targetPos[1];
    g_cameraPos[2] = g_targetPos[2] + g_model.getRadius() + CAMERA_ZNEAR + 0.4f;

    g_pitch = 0.0f;
    g_heading = 0.0f;
}


//현재 스레드를 하나의 프로세서에 할당
void SetProcessorAffinity()
{
    // Assign the current thread to one processor. This ensures that timing
    // code runs on only one processor, and will not suffer any ill effects
    // from power management.
    //
    // Based on DXUTSetProcessorAffinity() function from the DXUT framework.

    DWORD_PTR dwProcessAffinityMask = 0;
    DWORD_PTR dwSystemAffinityMask = 0;
    HANDLE hCurrentProcess = GetCurrentProcess();

    if (!GetProcessAffinityMask(hCurrentProcess, &dwProcessAffinityMask, &dwSystemAffinityMask))
        return;

    if (dwProcessAffinityMask)
    {
        // Find the lowest processor that our process is allowed to run against.

        DWORD_PTR dwAffinityMask = (dwProcessAffinityMask & ((~dwProcessAffinityMask) + 1));

        // Set this as the processor that our thread must always run against.
        // This must be a subset of the process affinity mask.

        HANDLE hCurrentThread = GetCurrentThread();

        if (hCurrentThread != INVALID_HANDLE_VALUE)
        {
            SetThreadAffinityMask(hCurrentThread, dwAffinityMask);
            CloseHandle(hCurrentThread);
        }
    }

    CloseHandle(hCurrentProcess);
}


//전체화면 전환 토글
void ToggleFullScreen()
{
    static DWORD savedExStyle;
    static DWORD savedStyle;
    static RECT rcSaved;

    g_isFullScreen = !g_isFullScreen;

    if (g_isFullScreen)
    {
        // Moving to full screen mode.

        savedExStyle = GetWindowLong(g_hWnd, GWL_EXSTYLE);
        savedStyle = GetWindowLong(g_hWnd, GWL_STYLE);
        GetWindowRect(g_hWnd, &rcSaved);

        SetWindowLong(g_hWnd, GWL_EXSTYLE, 0);
        SetWindowLong(g_hWnd, GWL_STYLE, WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        g_windowWidth = GetSystemMetrics(SM_CXSCREEN);
        g_windowHeight = GetSystemMetrics(SM_CYSCREEN);

        SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0,
            g_windowWidth, g_windowHeight, SWP_SHOWWINDOW);
    }
    else
    {
        // Moving back to windowed mode.

        SetWindowLong(g_hWnd, GWL_EXSTYLE, savedExStyle);
        SetWindowLong(g_hWnd, GWL_STYLE, savedStyle);
        SetWindowPos(g_hWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        g_windowWidth = rcSaved.right - rcSaved.left;
        g_windowHeight = rcSaved.bottom - rcSaved.top;

        SetWindowPos(g_hWnd, HWND_NOTOPMOST, rcSaved.left, rcSaved.top,
            g_windowWidth, g_windowHeight, SWP_SHOWWINDOW);
    }
}


//모델이 로드가 되지 않았을 때
void UnloadModel()
{
    SetCursor(LoadCursor(0, IDC_WAIT));

    ModelTextures::iterator i = g_modelTextures.begin();

    while (i != g_modelTextures.end())
    {
        glDeleteTextures(1, &i->second);
        ++i;
    }

    g_modelTextures.clear();
    g_model.destroy();

    SetCursor(LoadCursor(0, IDC_ARROW));
    SetWindowText(g_hWnd, APP_TITLE);
}

void UpdateFrame(float elapsedTimeSec)
{
    UpdateFrameRate(elapsedTimeSec);
}

void UpdateFrameRate(float elapsedTimeSec)
{
    static float accumTimeSec = 0.0f;
    static int frames = 0;

    accumTimeSec += elapsedTimeSec;

    if (accumTimeSec > 1.0f)
    {
        g_framesPerSecond = frames;

        frames = 0;
        accumTimeSec = 0.0f;
    }
    else
    {
        ++frames;
    }
}