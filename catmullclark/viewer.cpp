//////////////////////////////////////////////////////////////////////////////
//
// Longest Edge Bisection (LEB) Subdivision Demo
//
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl.h"

#include <cstdio>
#include <cstdlib>
#include <utility>
#include <stdexcept>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <ctime>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define DJ_OPENGL_IMPLEMENTATION 1
#include "dj_opengl.h"

#define DJ_ALGEBRA_IMPLEMENTATION 1
#include "dj_algebra.h"

#define CBT_IMPLEMENTATION
#include "cbt.h"

#define CC_IMPLEMENTATION
#include "CatmullClark.h"

#include "CatmullClarkTessellation.h"

#define SCATTER_IMPLEMENTATION 1

#define LOG(fmt, ...)  fprintf(stdout, fmt "\n", ##__VA_ARGS__); fflush(stdout);


////////////////////////////////////////////////////////////////////////////////
// Tweakable Constants
//
////////////////////////////////////////////////////////////////////////////////
#define VIEWER_DEFAULT_WIDTH  1920
#define VIEWER_DEFAULT_HEIGHT 1080
//#define VIEWER_DEFAULT_WIDTH  (512)
//#define VIEWER_DEFAULT_HEIGHT (512)

// default path to the directory holding the source files
#ifndef PATH_TO_SRC_DIRECTORY
#   define PATH_TO_SRC_DIRECTORY "./"
#endif

#define PATH_TO_ASSET_DIRECTORY PATH_TO_SRC_DIRECTORY "./assets/"

////////////////////////////////////////////////////////////////////////////////
// Global Variables
//
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// Framebuffer Manager
enum { AA_NONE, AA_MSAA2, AA_MSAA4, AA_MSAA8, AA_MSAA16 };
struct FramebufferManager {
    int w, h, aa;
    struct { int fixed; } msaa;
    struct { float r, g, b; } clearColor;
} g_framebuffer = {
    VIEWER_DEFAULT_WIDTH, VIEWER_DEFAULT_HEIGHT, AA_MSAA8,
    {false},
    {61.0f / 255.0f, 119.0f / 255.0f, 192.0f / 255.0f}
};

// -----------------------------------------------------------------------------
// Camera Manager
#define INIT_POS dja::vec3(-4.911597, 2.75, 5.348875) // trex - teaser

enum {
    TONEMAP_UNCHARTED2,
    TONEMAP_FILMIC,
    TONEMAP_ACES,
    TONEMAP_REINHARD,
    TONEMAP_RAW
};
enum {
    PROJECTION_ORTHOGRAPHIC, // no perspective
    PROJECTION_RECTILINEAR,  // preserves straight lines (OpenGL / DirectX)
    PROJECTION_FISHEYE       // conformal (stereographic projection)
};
struct CameraManager {
    float fovy, zNear, zFar;  // perspective settings
    int projection;           // controls the projection
    int tonemap;              // sensor (i.e., tone mapping technique)
    dja::vec3 pos;            // 3D position
    dja::mat3 axis;           // 3D frame @deprecated
    float upAngle, sideAngle; // rotation axis
    struct {
        float x, y, factor;
    } frameZoom;
    struct {
        int triangleCount;
    } sphere;
} g_camera = {
    30.0f, // default
    0.1f, 6400.f,
    PROJECTION_RECTILINEAR,
    TONEMAP_RAW,
    INIT_POS,
    dja::mat3(1.0f),
    -0.780000f, -0.205000f,
    {
        0.0f, 0.0, 0.0f
    },
    {
        0
    }
};
#undef INIT_POS
void updateCameraMatrix()
{
    float c1 = cos(g_camera.upAngle);
    float s1 = sin(g_camera.upAngle);
    float c2 = cos(g_camera.sideAngle);
    float s2 = sin(g_camera.sideAngle);

    g_camera.axis = dja::mat3(
        c1     , s1 * s2, c2 * s1,
        0.0f   ,      c2,     -s2,
        -s1    , c1 * s2, c1 * c2
    );
}

// -----------------------------------------------------------------------------
// Mesh Manager
enum { RENDERER_CAGE, RENDERER_SUBD, RENDERER_ULOD, RENDERER_ALOD };
enum { METHOD_CS, METHOD_TS, METHOD_GS, METHOD_MS };
enum { SHADING_SHADED, SHADING_UVS, SHADING_NORMALS, SHADING_COLOR};
struct MeshManager {
    struct {
        cc_Mesh *cage;
        cc_Subd *subd;
        int maxDepth;
        int subdDepth, uniformLodDepth;
        int computeShaderLocalSize;
    } subd;
    struct {
        cc_Mesh **frames;
        int32_t frameCount;
        float time;
    } animation;
    struct { bool displace, cull, freeze, wire, animate, fixTopology; } flags;
    int renderer;
    int method;
    int shading;
    float primitivePixelLengthTarget;
} g_mesh = {
    {NULL, NULL, 4, 1, 0, 6},
    {NULL, 48, 0.0f},
    {true, true, false, true, false, false},
    RENDERER_CAGE,
    METHOD_CS,
    SHADING_SHADED,
    9.0f
};


// -----------------------------------------------------------------------------
// Application Manager
struct AppManager {
    struct {
        const char *shader;
        const char *output;
    } dir;
    struct {
        int w, h;
        bool hud;
        float gamma, exposure;
    } viewer;
    struct {
        int on, frame, capture;
    } recorder;
    int frame, frameLimit;
} g_app = {
    /*dir*/     {
                    PATH_TO_SRC_DIRECTORY "./shaders/",
                    PATH_TO_SRC_DIRECTORY
                },
    /*viewer*/  {
                   VIEWER_DEFAULT_WIDTH, VIEWER_DEFAULT_HEIGHT,
                   true,
                   2.2f, 0.4f
                },
    /*record*/  {false, 0, 0},
    /*frame*/   0, -1
};

// -----------------------------------------------------------------------------
// OpenGL Manager

enum {
    CLOCK_ALL,
    CLOCK_BATCH,
    CLOCK_TESSELLATION,
    CLOCK_SUBD,
    CLOCK_RENDER,
    CLOCK_REDUCTION,
    CLOCK_REDUCTION00,
    CLOCK_REDUCTION01,
    CLOCK_REDUCTION02,
    CLOCK_REDUCTION03,
    CLOCK_REDUCTION04,
    CLOCK_REDUCTION05,
    CLOCK_REDUCTION06,
    CLOCK_REDUCTION07,
    CLOCK_REDUCTION08,
    CLOCK_REDUCTION09,
    CLOCK_REDUCTION10,
    CLOCK_REDUCTION11,
    CLOCK_REDUCTION12,
    CLOCK_REDUCTION13,
    CLOCK_REDUCTION14,
    CLOCK_REDUCTION15,
    CLOCK_REDUCTION16,
    CLOCK_REDUCTION17,
    CLOCK_REDUCTION18,
    CLOCK_REDUCTION19,
    CLOCK_REDUCTION20,
    CLOCK_REDUCTION21,
    CLOCK_REDUCTION22,
    CLOCK_REDUCTION23,
    CLOCK_REDUCTION24,
    CLOCK_REDUCTION25,
    CLOCK_REDUCTION26,
    CLOCK_REDUCTION27,
    CLOCK_REDUCTION28,
    CLOCK_REDUCTION29,
    CLOCK_COUNT
};
enum { FRAMEBUFFER_BACK, FRAMEBUFFER_SCENE, FRAMEBUFFER_COUNT };
enum { STREAM_XFORM_VARIABLES, STREAM_COUNT };
enum {
    VERTEXARRAY_EMPTY,
    VERTEXARRAY_SPHERE,
    VERTEXARRAY_COUNT
};
enum {
    BUFFER_XFORM,
    BUFFER_CBT,
    BUFFER_CAGE_VERTEX_TO_HALFEDGE,
    BUFFER_CAGE_HALFEDGE_TO_FACE,
    BUFFER_CAGE_EDGE_TO_HALFEDGE,
    BUFFER_CAGE_FACE_TO_HALFEDGE,
    BUFFER_CAGE_HALFEDGES,
    BUFFER_CAGE_CREASES,
    BUFFER_CAGE_VERTEX_POINTS,
    BUFFER_CAGE_VERTEX_UVS,
    BUFFER_CAGE_COUNTERS,
    BUFFER_SUBD_MAXDEPTH,
    BUFFER_SUBD_HALFEDGES,
    BUFFER_SUBD_CREASES,
    BUFFER_SUBD_VERTEX_POINTS,
    BUFFER_CBT_DISPATCH,
    BUFFER_CCT_DRAW,
    BUFFER_CCT_FACE_COUNT,

    BUFFER_COUNT
};
enum {
    TEXTURE_COLOR_BUFFER,
    TEXTURE_DEPTH_BUFFER,
    TEXTURE_MESH_ALBEDO,
    TEXTURE_MESH_DMAP,
    TEXTURE_MESH_NMAP,
    TEXTURE_MESH_AMAP,
    TEXTURE_MESH_AO,
    TEXTURE_CMAP,

    TEXTURE_COUNT
};
enum {
    PROGRAM_VIEWER,

    PROGRAM_SUBD_CAGE_HALFEDGES,
    PROGRAM_SUBD_CAGE_CREASES,
    PROGRAM_SUBD_CAGE_FACE_POINTS,
    PROGRAM_SUBD_CAGE_EDGE_POINTS,
    PROGRAM_SUBD_CAGE_VERTEX_POINTS,
    PROGRAM_SUBD_CAGE_VERTEX_UVS,
    PROGRAM_SUBD_HALFEDGES,
    PROGRAM_SUBD_CREASES,
    PROGRAM_SUBD_FACE_POINTS,
    PROGRAM_SUBD_EDGE_POINTS,
    PROGRAM_SUBD_VERTEX_POINTS,
    PROGRAM_SUBD_VERTEX_UVS,

    PROGRAM_CAGE_RENDER,
    PROGRAM_SUBD_RENDER,
    PROGRAM_LOD_UNIFORM_RENDER,
    PROGRAM_LOD_ADAPTIVE_RENDER,
    PROGRAM_CBT_REDUCTION,
    PROGRAM_CBT_REDUCTION_PREPASS,
    PROGRAM_CBT_DISPATCHER,
    PROGRAM_CCT_DISPATCHER,
    PROGRAM_CCT_SPLIT,
    PROGRAM_CCT_MERGE,
    PROGRAM_SUBD_DISPLACE,

    PROGRAM_COUNT
};
enum {
    UNIFORM_VIEWER_FRAMEBUFFER_SAMPLER,
    UNIFORM_VIEWER_GAMMA,

    UNIFORM_CAGE_RENDER_SCREEN_RESOLUTION,

    UNIFORM_SUBD_RENDER_SCREEN_RESOLUTION,
    UNIFORM_SUBD_RENDER_DEPTH,

    UNIFORM_UNIFORM_LOD_RENDER_SCREEN_RESOLUTION,
    UNIFORM_UNIFORM_LOD_RENDER_LEVEL_OF_DETAIL,

    UNIFORM_ADAPTIVE_LOD_RENDER_SCREEN_RESOLUTION,
    UNIFORM_ADAPTIVE_LOD_ALBEDO_SAMPLER,
    UNIFORM_ADAPTIVE_LOD_NMAP_SAMPLER,
    UNIFORM_ADAPTIVE_LOD_AMAP_SAMPLER,
    UNIFORM_ADAPTIVE_LOD_AO_SAMPLER,

    UNIFORM_CCT_LOD_FACTOR,

    UNIFORM_CCT_SPLIT_LOD_FACTOR,

    UNIFORM_CCT_MERGE_LOD_FACTOR,

    UNIFORM_SUBD_DISPLACE_DMAP,
    UNIFORM_SUBD_DISPLACE_SCALE,

    UNIFORM_COUNT
};
struct OpenGLManager {
    GLuint programs[PROGRAM_COUNT];
    GLuint framebuffers[FRAMEBUFFER_COUNT];
    GLuint textures[TEXTURE_COUNT];
    GLuint vertexArrays[VERTEXARRAY_COUNT];
    GLuint buffers[BUFFER_COUNT];
    GLint uniforms[UNIFORM_COUNT];
    djg_buffer *streams[STREAM_COUNT];
    djg_clock *clocks[CLOCK_COUNT];
} g_gl = {
    {0},
    {0},
    {0},
    {0},
    {0},
    {0},
    {NULL},
    {NULL}
};

////////////////////////////////////////////////////////////////////////////////
// Utility functions
//
////////////////////////////////////////////////////////////////////////////////

#ifndef M_PI
#define M_PI 3.141592654
#endif
#define BUFFER_SIZE(x)    ((int)(sizeof(x)/sizeof(x[0])))
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

float Radians(float degrees)
{
    return degrees * M_PI / 180.f;
}

static void APIENTRY
DebugOutputLogger(
    GLenum source,
    GLenum type,
    GLuint,
    GLenum severity,
    GLsizei,
    const GLchar* message,
    const GLvoid*
) {
    char srcstr[32], typestr[32];

    switch(source) {
        case GL_DEBUG_SOURCE_API: strcpy(srcstr, "OpenGL"); break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: strcpy(srcstr, "Windows"); break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: strcpy(srcstr, "Shader Compiler"); break;
        case GL_DEBUG_SOURCE_THIRD_PARTY: strcpy(srcstr, "Third Party"); break;
        case GL_DEBUG_SOURCE_APPLICATION: strcpy(srcstr, "Application"); break;
        case GL_DEBUG_SOURCE_OTHER: strcpy(srcstr, "Other"); break;
        default: strcpy(srcstr, "???"); break;
    };

    switch(type) {
        case GL_DEBUG_TYPE_ERROR: strcpy(typestr, "Error"); break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: strcpy(typestr, "Deprecated Behavior"); break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: strcpy(typestr, "Undefined Behavior"); break;
        case GL_DEBUG_TYPE_PORTABILITY: strcpy(typestr, "Portability"); break;
        case GL_DEBUG_TYPE_PERFORMANCE: strcpy(typestr, "Performance"); break;
        case GL_DEBUG_TYPE_OTHER: strcpy(typestr, "Message"); break;
        default: strcpy(typestr, "???"); break;
    }

    if(severity == GL_DEBUG_SEVERITY_HIGH) {
        LOG("djg_error: %s %s\n"                \
                "-- Begin -- GL_debug_output\n" \
                "%s\n"                              \
                "-- End -- GL_debug_output\n",
                srcstr, typestr, message);
    } else if(severity == GL_DEBUG_SEVERITY_MEDIUM) {
        LOG("djg_warn: %s %s\n"                 \
                "-- Begin -- GL_debug_output\n" \
                "%s\n"                              \
                "-- End -- GL_debug_output\n",
                srcstr, typestr, message);
    }
}

void InitDebugOutput(void)
{
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&DebugOutputLogger, NULL);
}

////////////////////////////////////////////////////////////////////////////////
// Program Configuration
//
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// set viewer program uniforms
void ConfigureViewerProgram()
{
    glProgramUniform1i(g_gl.programs[PROGRAM_VIEWER],
        g_gl.uniforms[UNIFORM_VIEWER_FRAMEBUFFER_SAMPLER],
        TEXTURE_COLOR_BUFFER);
    glProgramUniform1f(g_gl.programs[PROGRAM_VIEWER],
        g_gl.uniforms[UNIFORM_VIEWER_GAMMA],
        g_app.viewer.gamma);
}

void ConfigureCageRenderProgram()
{
    glProgramUniform2f(g_gl.programs[PROGRAM_CAGE_RENDER],
        g_gl.uniforms[UNIFORM_CAGE_RENDER_SCREEN_RESOLUTION],
        g_framebuffer.w, g_framebuffer.h);
    glProgramUniform1i(g_gl.programs[PROGRAM_CAGE_RENDER],
                       glGetUniformLocation(g_gl.programs[PROGRAM_CAGE_RENDER], "u_CmapSampler"),
                       TEXTURE_CMAP);
}

void ConfigureSubdRenderProgram()
{
    glProgramUniform2f(g_gl.programs[PROGRAM_SUBD_RENDER],
        g_gl.uniforms[UNIFORM_SUBD_RENDER_SCREEN_RESOLUTION],
        g_framebuffer.w, g_framebuffer.h);
    glProgramUniform1i(g_gl.programs[PROGRAM_SUBD_RENDER],
        g_gl.uniforms[UNIFORM_SUBD_RENDER_DEPTH],
        g_mesh.subd.subdDepth);
}

void ConfigureUniformLodRenderProgram()
{
    glProgramUniform2f(g_gl.programs[PROGRAM_LOD_UNIFORM_RENDER],
        g_gl.uniforms[UNIFORM_UNIFORM_LOD_RENDER_SCREEN_RESOLUTION],
        g_framebuffer.w, g_framebuffer.h);
    glProgramUniform1i(g_gl.programs[PROGRAM_LOD_UNIFORM_RENDER],
        g_gl.uniforms[UNIFORM_UNIFORM_LOD_RENDER_LEVEL_OF_DETAIL],
        g_mesh.subd.uniformLodDepth);
}

void ConfigureAdaptiveLodRenderProgram()
{
    glProgramUniform2f(g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER],
        g_gl.uniforms[UNIFORM_ADAPTIVE_LOD_RENDER_SCREEN_RESOLUTION],
        g_framebuffer.w, g_framebuffer.h);
    glProgramUniform1i(g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER],
        g_gl.uniforms[UNIFORM_ADAPTIVE_LOD_ALBEDO_SAMPLER],
        TEXTURE_MESH_ALBEDO);
    glProgramUniform1i(g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER],
        g_gl.uniforms[UNIFORM_ADAPTIVE_LOD_NMAP_SAMPLER],
        TEXTURE_MESH_NMAP);
    glProgramUniform1i(g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER],
        g_gl.uniforms[UNIFORM_ADAPTIVE_LOD_AO_SAMPLER],
        TEXTURE_MESH_AO);
    glProgramUniform1i(g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER],
        g_gl.uniforms[UNIFORM_ADAPTIVE_LOD_AMAP_SAMPLER],
        TEXTURE_MESH_AMAP);
}

void ConfigureSubdDisplaceProgram()
{
    const GLuint program = g_gl.programs[PROGRAM_SUBD_DISPLACE];

    glProgramUniform1i(program,
        g_gl.uniforms[UNIFORM_SUBD_DISPLACE_DMAP],
        TEXTURE_MESH_DMAP);
    glProgramUniform1f(program,
        g_gl.uniforms[UNIFORM_SUBD_DISPLACE_SCALE],
        1.0f);
}

float ComputeLodFactor()
{
    const float zoom = exp2(-g_camera.frameZoom.factor);
    if (g_camera.projection == PROJECTION_RECTILINEAR) {
        float tmp = 2.0f * tan(Radians(g_camera.fovy) / 2.0f)
            / g_framebuffer.h * g_mesh.primitivePixelLengthTarget * zoom;

        return -2.0f * std::log2(tmp) + 2.0f;
    } else if (g_camera.projection == PROJECTION_ORTHOGRAPHIC) {
        float planeSize = 2.0f * tan(Radians(g_camera.fovy / 2.0f));
        float targetSize = planeSize * g_mesh.primitivePixelLengthTarget
                         / g_framebuffer.h;

        return -2.0f * std::log2(targetSize);
    } else if (g_camera.projection == PROJECTION_FISHEYE) {
        float tmp = 2.0f * tan(Radians(g_camera.fovy) / 2.0f)
            / g_framebuffer.h
            * g_mesh.primitivePixelLengthTarget;

        return -2.0f * std::log2(tmp) + 2.0f;
    }

    return 1.0f;
}

void ConfigureTessellationProgram(GLuint glp, GLuint offset)
{
    float lodFactor = ComputeLodFactor();

    glProgramUniform1f(glp,
        g_gl.uniforms[UNIFORM_CCT_LOD_FACTOR + offset],
        lodFactor);
}

void ConfigureTessellationPrograms()
{
    ConfigureTessellationProgram(g_gl.programs[PROGRAM_CCT_SPLIT],
                                 UNIFORM_CCT_SPLIT_LOD_FACTOR - UNIFORM_CCT_LOD_FACTOR);
    ConfigureTessellationProgram(g_gl.programs[PROGRAM_CCT_MERGE],
                                 UNIFORM_CCT_MERGE_LOD_FACTOR - UNIFORM_CCT_LOD_FACTOR);
}

///////////////////////////////////////////////////////////////////////////////
// Program Loading
//
////////////////////////////////////////////////////////////////////////////////

static void
LoadCatmullClarkLibrary(
    djg_program *djgp,
    bool halfedgeWrite,
    bool creaseWrite,
    bool vertexWrite
) {
    djgp_push_string(djgp, "#extension GL_NV_shader_atomic_float: require\n");
    djgp_push_string(djgp, "#extension GL_NV_shader_thread_shuffle: require\n");
#ifdef CC_DISABLE_UV
    djgp_push_string(djgp, "#define CC_DISABLE_UV\n");
#endif

    if (halfedgeWrite) {
        djgp_push_string(djgp, "#define CCS_HALFEDGE_WRITE\n");
    }
    if (creaseWrite) {
        djgp_push_string(djgp, "#define CCS_CREASE_WRITE\n");
    }
    if (vertexWrite) {
        djgp_push_string(djgp, "#define CCS_VERTEX_WRITE\n");
    }

    djgp_push_string(djgp,
                     "#define CC_LOCAL_SIZE_X %i\n",
                     1 << 1 << g_mesh.subd.computeShaderLocalSize);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_CAGE_VERTEX_TO_HALFEDGE %i\n",
                     BUFFER_CAGE_VERTEX_TO_HALFEDGE);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_CAGE_EDGE_TO_HALFEDGE %i\n",
                     BUFFER_CAGE_EDGE_TO_HALFEDGE);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_CAGE_FACE_TO_HALFEDGE %i\n",
                     BUFFER_CAGE_FACE_TO_HALFEDGE);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_CAGE_HALFEDGE %i\n",
                     BUFFER_CAGE_HALFEDGES);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_CAGE_CREASE %i\n",
                     BUFFER_CAGE_CREASES);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_CAGE_VERTEX_POINT %i\n",
                     BUFFER_CAGE_VERTEX_POINTS);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_CAGE_UV %i\n",
                     BUFFER_CAGE_VERTEX_UVS);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_CAGE_COUNTERS %i\n",
                     BUFFER_CAGE_COUNTERS);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_SUBD_MAXDEPTH %i\n",
                     BUFFER_SUBD_MAXDEPTH);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_SUBD_HALFEDGE %i\n",
                     BUFFER_SUBD_HALFEDGES);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_SUBD_CREASE %i\n",
                     BUFFER_SUBD_CREASES);
    djgp_push_string(djgp,
                     "#define CC_BUFFER_BINDING_SUBD_VERTEX_POINT %i\n",
                     BUFFER_SUBD_VERTEX_POINTS);

    djgp_push_file(djgp, PATH_TO_SRC_DIRECTORY "./submodules/HalfedgeCatmullClark/glsl/CatmullClark_Scatter.glsl");
}

static void LoadConcurrentBinaryTreeLibrary(djg_program *djp, bool writeAccess)
{
    if (!writeAccess)
        djgp_push_string(djp, "#define CBT_READ_ONLY\n");

    djgp_push_string(djp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_CBT);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt.glsl");
}

bool LoadCageRenderProgram()
{
    djg_program *djp = djgp_create();
    GLuint *program = &g_gl.programs[PROGRAM_CAGE_RENDER];

    LOG("Loading {Cage-Render-Program}");

    if (g_mesh.flags.wire) {
        djgp_push_string(djp, "#define FLAG_WIRE 1\n");
    }
    LoadCatmullClarkLibrary(djp, false, false, false);
    djgp_push_string(djp, "#define BUFFER_BINDING_XFORM %i\n", STREAM_XFORM_VARIABLES);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/CageRender.glsl");
    if (!djgp_to_gl(djp, 450, false, true, program)) {
        LOG("=> Failure <=\n");
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    g_gl.uniforms[UNIFORM_CAGE_RENDER_SCREEN_RESOLUTION] =
        glGetUniformLocation(g_gl.programs[PROGRAM_CAGE_RENDER], "u_ScreenResolution");

    ConfigureCageRenderProgram();

    return (glGetError() == GL_NO_ERROR);
}

bool LoadSubdRenderProgram()
{
    djg_program *djp = djgp_create();
    GLuint *program = &g_gl.programs[PROGRAM_SUBD_RENDER];

    LOG("Loading {Subd-Render-Program}");

    if (g_mesh.flags.wire) {
        djgp_push_string(djp, "#define FLAG_WIRE 1\n");
    }
    LoadCatmullClarkLibrary(djp, false, false, false);
    djgp_push_string(djp, "#define BUFFER_BINDING_XFORM %i\n", STREAM_XFORM_VARIABLES);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/SubdRender.glsl");
    if (!djgp_to_gl(djp, 450, false, true, program)) {
        LOG("=> Failure <=\n");
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    g_gl.uniforms[UNIFORM_SUBD_RENDER_SCREEN_RESOLUTION] =
        glGetUniformLocation(g_gl.programs[PROGRAM_SUBD_RENDER], "u_ScreenResolution");
    g_gl.uniforms[UNIFORM_SUBD_RENDER_DEPTH] =
        glGetUniformLocation(g_gl.programs[PROGRAM_SUBD_RENDER], "u_Depth");

    ConfigureSubdRenderProgram();

    return (glGetError() == GL_NO_ERROR);
}

bool LoadUniformLodRenderProgram()
{
    djg_program *djp = djgp_create();
    GLuint *program = &g_gl.programs[PROGRAM_LOD_UNIFORM_RENDER];

    LOG("Loading {Uniform-LoD-Render-Program}");

    if (g_mesh.flags.wire) {
        djgp_push_string(djp, "#define FLAG_WIRE 1\n");
    }

    LoadCatmullClarkLibrary(djp, false, false, false);
    djgp_push_string(djp, "#define BUFFER_BINDING_XFORM %i\n", STREAM_XFORM_VARIABLES);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/UniformLodRender.glsl");
    if (!djgp_to_gl(djp, 450, false, true, program)) {
        LOG("=> Failure <=\n");
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    g_gl.uniforms[UNIFORM_UNIFORM_LOD_RENDER_SCREEN_RESOLUTION] =
        glGetUniformLocation(g_gl.programs[PROGRAM_LOD_UNIFORM_RENDER], "u_ScreenResolution");
    g_gl.uniforms[UNIFORM_UNIFORM_LOD_RENDER_LEVEL_OF_DETAIL] =
        glGetUniformLocation(g_gl.programs[PROGRAM_LOD_UNIFORM_RENDER], "u_LevelOfDetail");

    ConfigureUniformLodRenderProgram();

    return (glGetError() == GL_NO_ERROR);
}


bool LoadAdaptiveLodRenderProgram()
{
    djg_program *djp = djgp_create();
    GLuint *program = &g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER];

    LOG("Loading {Adaptive-LoD-Render-Program}");

    LoadCatmullClarkLibrary(djp, false, false, false);
    LoadConcurrentBinaryTreeLibrary(djp, false);

    if (g_mesh.shading == SHADING_SHADED)
        djgp_push_string(djp, "#define SHADING_SHADED 1\n");
    else if (g_mesh.shading == SHADING_UVS)
        djgp_push_string(djp, "#define SHADING_UVS 1\n");
    else if (g_mesh.shading == SHADING_NORMALS)
        djgp_push_string(djp, "#define SHADING_NORMALS 1\n");
    else if (g_mesh.shading == SHADING_COLOR)
        djgp_push_string(djp, "#define SHADING_COLOR 1\n");
    if (g_mesh.flags.wire) {
        djgp_push_string(djp, "#define FLAG_WIRE 1\n");
    }
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/AdaptiveLodRender_Common.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/CatmullClarkTessellation.glsl");
    djgp_push_string(djp, "#define BUFFER_BINDING_XFORM %i\n", STREAM_XFORM_VARIABLES);
    if (g_mesh.flags.wire) {
        djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/AdaptiveLodRender_Wire.glsl");
    } else {
        djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/AdaptiveLodRender.glsl");
    }
    if (!djgp_to_gl(djp, 450, false, true, program)) {
        LOG("=> Failure <=\n");
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    g_gl.uniforms[UNIFORM_ADAPTIVE_LOD_RENDER_SCREEN_RESOLUTION] =
        glGetUniformLocation(g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER], "u_ScreenResolution");
    g_gl.uniforms[UNIFORM_ADAPTIVE_LOD_ALBEDO_SAMPLER] =
        glGetUniformLocation(g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER], "u_Albedo");
    g_gl.uniforms[UNIFORM_ADAPTIVE_LOD_NMAP_SAMPLER] =
        glGetUniformLocation(g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER], "u_Nmap");
    g_gl.uniforms[UNIFORM_ADAPTIVE_LOD_AO_SAMPLER] =
        glGetUniformLocation(g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER], "u_Ao");
    g_gl.uniforms[UNIFORM_ADAPTIVE_LOD_AMAP_SAMPLER] =
        glGetUniformLocation(g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER], "u_Amap");

    ConfigureAdaptiveLodRenderProgram();

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------
/**
 * Load the Reduction Program
 *
 * This program is responsible for precomputing a reduction for the
 * subdivision tree. This allows to locate the i-th bit in a bitfield of
 * size N in log(N) operations.
 */
bool LoadCbtReductionProgram()
{
    djg_program *djp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_CBT_REDUCTION];

    LOG("Loading {Reduction-Program}");
    LoadConcurrentBinaryTreeLibrary(djp, true);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt_SumReduction.glsl");
    djgp_push_string(djp, "#ifdef COMPUTE_SHADER\n#endif");

    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    return (glGetError() == GL_NO_ERROR);
}

bool LoadCbtReductionPrepassProgram()
{
    djg_program *djp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_CBT_REDUCTION_PREPASS];

    LOG("Loading {Reduction-Prepass-Program}");
    LoadConcurrentBinaryTreeLibrary(djp, true);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt_SumReductionPrepass.glsl");
    djgp_push_string(djp, "#ifdef COMPUTE_SHADER\n#endif");
    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    return (glGetError() == GL_NO_ERROR);
}

bool LoadCbtDispatchProgram()
{
    djg_program *djp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_CBT_DISPATCHER];

    LOG("Loading {CBT-Dispatch-Program}");
    LoadConcurrentBinaryTreeLibrary(djp, false);
    djgp_push_string(djp, "#define CBT_DISPATCHER_BUFFER_BINDING %i\n", BUFFER_CBT_DISPATCH);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt_Dispatcher.glsl");
    djgp_push_string(djp, "#ifdef COMPUTE_SHADER\n#endif");
    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    return (glGetError() == GL_NO_ERROR);
}

bool LoadCctDispatchProgram()
{
    djg_program *djp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_CCT_DISPATCHER];

    LOG("Loading {CCT-Dispatch-Program}");
    LoadCatmullClarkLibrary(djp, false, false, false);
    LoadConcurrentBinaryTreeLibrary(djp, false);
    djgp_push_string(djp, "#define CCT_DISPATCHER_BUFFER_BINDING %i\n", BUFFER_CCT_DRAW);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/CatmullClarkTessellation.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/cct_Dispatcher.glsl");
    djgp_push_string(djp, "#ifdef COMPUTE_SHADER\n#endif");
    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    return (glGetError() == GL_NO_ERROR);
}


bool LoadTessellationProgram(GLuint *glp, const char *flag, GLuint uniformOffset)
{
    djg_program *djp = djgp_create();

    LOG("Loading {CCT-Program}");
    LoadCatmullClarkLibrary(djp, false, false, false);
    LoadConcurrentBinaryTreeLibrary(djp, false);

    if (!g_mesh.flags.freeze)
        djgp_push_string(djp, flag);
    switch (g_camera.projection) {
    case PROJECTION_RECTILINEAR:
        djgp_push_string(djp, "#define PROJECTION_RECTILINEAR\n");
        break;
    case PROJECTION_FISHEYE:
        djgp_push_string(djp, "#define PROJECTION_FISHEYE\n");
        break;
    case PROJECTION_ORTHOGRAPHIC:
        djgp_push_string(djp, "#define PROJECTION_ORTHOGRAPHIC\n");
        break;
    default:
        break;
    }
    djgp_push_string(djp, "#define BUFFER_BINDING_XFORM %i\n", STREAM_XFORM_VARIABLES);
    if (g_mesh.flags.cull)
        djgp_push_string(djp, "#define FLAG_CULL 1\n");
    if (g_mesh.flags.wire)
        djgp_push_string(djp, "#define FLAG_WIRE 1\n");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/FrustumCulling.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/CatmullClarkTessellation.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/Tessellation.glsl");
    djgp_push_string(djp, "#ifdef COMPUTE_SHADER\n#endif");

    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    g_gl.uniforms[UNIFORM_CCT_LOD_FACTOR + uniformOffset] =
        glGetUniformLocation(*glp, "u_LodFactor");

    ConfigureTessellationProgram(*glp, uniformOffset);

    return (glGetError() == GL_NO_ERROR);
}


bool LoadTessellationPrograms()
{
    bool v = true;

    if (v) v = v && LoadTessellationProgram(&g_gl.programs[PROGRAM_CCT_SPLIT],
                                            "#define FLAG_SPLIT 1\n",
                                            UNIFORM_CCT_SPLIT_LOD_FACTOR - UNIFORM_CCT_LOD_FACTOR);
    if (v) v = v && LoadTessellationProgram(&g_gl.programs[PROGRAM_CCT_MERGE],
                                            "#define FLAG_MERGE 1\n",
                                            UNIFORM_CCT_MERGE_LOD_FACTOR - UNIFORM_CCT_LOD_FACTOR);

    return v;
}


bool LoadSubdDisplaceProgram()
{
#if 0
    djg_program *djp = djgp_create();
    GLuint *program = &g_gl.programs[PROGRAM_SUBD_DISPLACE];

    LOG("Loading {Subd-Displace-Program}\n");
    LoadCatmullClarkLibrary(djp, false, true);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./shaders/cc_Displace.glsl");
    djgp_push_string(djp, "#ifdef COMPUTE_SHADER\n#endif");

    if (!djgp_to_gl(djp, 450, false, true, program)) {
        LOG("=> Failure <=\n");
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    g_gl.uniforms[UNIFORM_SUBD_DISPLACE_DMAP] =
        glGetUniformLocation(*program, "u_Dmap");
    g_gl.uniforms[UNIFORM_SUBD_DISPLACE_SCALE] =
        glGetUniformLocation(*program, "u_Scale");

    ConfigureSubdDisplaceProgram();
#endif
    return (glGetError() == GL_NO_ERROR);
}



// -----------------------------------------------------------------------------
/**
 * Load the Viewer Program
 *
 * This program is responsible for blitting the scene framebuffer to
 * the back framebuffer, while applying gamma correction and tone mapping to
 * the rendering.
 */
bool LoadViewerProgram()
{
    djg_program *djp = djgp_create();
    GLuint *program = &g_gl.programs[PROGRAM_VIEWER];

    LOG("Loading {Viewer-Program}");
    if (g_framebuffer.aa >= AA_MSAA2 && g_framebuffer.aa <= AA_MSAA16)
        djgp_push_string(djp, "#define MSAA_FACTOR %i\n", 1 << g_framebuffer.aa);
    switch (g_camera.tonemap) {
    case TONEMAP_UNCHARTED2:
        djgp_push_string(djp, "#define TONEMAP_UNCHARTED2\n");
        break;
    case TONEMAP_FILMIC:
        djgp_push_string(djp, "#define TONEMAP_FILMIC\n");
        break;
    case TONEMAP_ACES:
        djgp_push_string(djp, "#define TONEMAP_ACES\n");
        break;
    case TONEMAP_REINHARD:
        djgp_push_string(djp, "#define TONEMAP_REINHARD\n");
        break;
    default:
        break;
    }
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/ToneMapping.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./catmullclark/shaders/Viewer.glsl");

    if (!djgp_to_gl(djp, 450, false, true, program)) {
        LOG("=> Failure <=\n");
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    g_gl.uniforms[UNIFORM_VIEWER_FRAMEBUFFER_SAMPLER] =
        glGetUniformLocation(*program, "u_FramebufferSampler");
    g_gl.uniforms[UNIFORM_VIEWER_GAMMA] =
        glGetUniformLocation(*program, "u_Gamma");

    ConfigureViewerProgram();

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------

static bool
LoadCatmullClarkProgram(
    int32_t programID,
    const char *sourceFile,
    bool halfEdgeWrite,
    bool creaseWrite,
    bool vertexWrite
) {
    djg_program *djgp = djgp_create();
    GLuint *glp = &g_gl.programs[programID];

    LoadCatmullClarkLibrary(djgp, halfEdgeWrite, creaseWrite, vertexWrite);
    djgp_push_file(djgp, sourceFile);
    djgp_push_string(djgp, "#ifdef COMPUTE_SHADER\n#endif");
    if (!djgp_to_gl(djgp, 450, false, true, glp)) {
        djgp_release(djgp);

        return false;
    }

    djgp_release(djgp);

    return glGetError() == GL_NO_ERROR;
}

#define PATH_TO_SHADER_DIRECTORY PATH_TO_SRC_DIRECTORY "submodules/HalfedgeCatmullClark/glsl/"
bool LoadCageFaceRefinementProgram()
{
    LOG("Loading {Program-Cage-Face-Points}");
#ifdef SCATTER_IMPLEMENTATION
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_CageFacePoints_Scatter.glsl";
#else
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_CreasedCageFacePoints_Gather.glsl";
#endif

    return LoadCatmullClarkProgram(PROGRAM_SUBD_CAGE_FACE_POINTS, srcFile, false, false, true);
}

bool LoadCageEdgeRefinementProgram()
{
    LOG("Loading {Program-Cage-Edge-Points}");
#ifdef SCATTER_IMPLEMENTATION
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_CageEdgePoints_Scatter.glsl";
#else
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_CreasedCageEdgePoints_Gather.glsl";
#endif

    return LoadCatmullClarkProgram(PROGRAM_SUBD_CAGE_EDGE_POINTS, srcFile, false, false, true);
}

bool LoadCageVertexRefinementProgram()
{
    LOG("Loading {Program-Cage-Vertex-Points}");
#ifdef SCATTER_IMPLEMENTATION
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_CageVertexPoints_Scatter.glsl";
#else
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_CreasedCageVertexPoints_Gather.glsl";
#endif

    return LoadCatmullClarkProgram(PROGRAM_SUBD_CAGE_VERTEX_POINTS, srcFile, false, false, true);
}

bool LoadCageHalfedgeRefinementProgram()
{
    LOG("Loading {Program-Refine-Cage-Halfedges}");
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_RefineCageHalfedges.glsl";

    return LoadCatmullClarkProgram(PROGRAM_SUBD_CAGE_HALFEDGES, srcFile, true, false, false);
}

bool LoadCageCreaseRefinementProgram()
{
    LOG("Loading {Program-Refine-Cage-Creases}");
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_RefineCageCreases.glsl";

    return LoadCatmullClarkProgram(PROGRAM_SUBD_CAGE_CREASES, srcFile, false, true, false);
}

bool LoadCageVertexUvRefinementProgram()
{
    LOG("Loading {Program-Refine-Cage-Uvs}");
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_RefineCageVertexUvs.glsl";

    return LoadCatmullClarkProgram(PROGRAM_SUBD_CAGE_VERTEX_UVS, srcFile, true, false, false);
}

bool LoadFaceRefinementProgram()
{
    LOG("Loading {Program-Face-Points}");
#ifdef SCATTER_IMPLEMENTATION
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_FacePoints_Scatter.glsl";
#else
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_CreasedFacePoints_Gather.glsl";
#endif

    return LoadCatmullClarkProgram(PROGRAM_SUBD_FACE_POINTS, srcFile, false, false, true);
}

bool LoadEdgeRefinementProgram()
{
    LOG("Loading {Program-Edge-Points}");
#ifdef SCATTER_IMPLEMENTATION
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_EdgePoints_Scatter.glsl";
#else
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_CreasedEdgePoints_Gather.glsl";
#endif

    return LoadCatmullClarkProgram(PROGRAM_SUBD_EDGE_POINTS, srcFile, false, false, true);
}

bool LoadVertexRefinementProgram()
{
    LOG("Loading {Program-Vertex-Points}");
#ifdef SCATTER_IMPLEMENTATION
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_VertexPoints_Scatter.glsl";
#else
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_CreasedVertexPoints_Gather.glsl";
#endif

    return LoadCatmullClarkProgram(PROGRAM_SUBD_VERTEX_POINTS, srcFile, false, false, true);
}

bool LoadHalfedgeRefinementProgram()
{
    LOG("Loading {Program-Refine-Halfedges}");
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_RefineHalfedges.glsl";

    return LoadCatmullClarkProgram(PROGRAM_SUBD_HALFEDGES, srcFile, true, false, false);
}

bool LoadCreaseRefinementProgram()
{
    LOG("Loading {Program-Refine-Creases}");
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_RefineCreases.glsl";

    return LoadCatmullClarkProgram(PROGRAM_SUBD_CREASES, srcFile, false, true, false);
}

bool LoadVertexUvRefinementProgram()
{
    LOG("Loading {Program-Refine-Vertex-Uvs}");
    const char *srcFile = PATH_TO_SHADER_DIRECTORY "cc_RefineVertexUvs.glsl";

    return LoadCatmullClarkProgram(PROGRAM_SUBD_VERTEX_UVS, srcFile, true, false, false);
}
#undef PATH_TO_SHADER_DIRECTORY


// -----------------------------------------------------------------------------
/**
 * Load All Programs
 *
 */
bool LoadPrograms()
{
    bool success = true;

    if (success) success = LoadCageHalfedgeRefinementProgram();
    if (success) success = LoadCageVertexUvRefinementProgram();
    if (success) success = LoadCageCreaseRefinementProgram();
    if (success) success = LoadCageFaceRefinementProgram();
    if (success) success = LoadCageEdgeRefinementProgram();
    if (success) success = LoadCageVertexRefinementProgram();
    if (success) success = LoadFaceRefinementProgram();
    if (success) success = LoadEdgeRefinementProgram();
    if (success) success = LoadHalfedgeRefinementProgram();
    if (success) success = LoadVertexUvRefinementProgram();
    if (success) success = LoadCreaseRefinementProgram();
    if (success) success = LoadVertexRefinementProgram();
    if (success) success = LoadViewerProgram();
    if (success) success = LoadCageRenderProgram();
    if (success) success = LoadSubdRenderProgram();
    if (success) success = LoadUniformLodRenderProgram();
    if (success) success = LoadAdaptiveLodRenderProgram();
    if (success) success = LoadCbtReductionPrepassProgram();
    if (success) success = LoadCbtReductionProgram();
    if (success) success = LoadCbtDispatchProgram();
    if (success) success = LoadCctDispatchProgram();
    if (success) success = LoadTessellationPrograms();
    if (success) success = LoadSubdDisplaceProgram();

    return success;
}

////////////////////////////////////////////////////////////////////////////////
// Texture Loading
//
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
/**
 * Load the Scene Framebuffer Textures
 *
 * Depending on the scene framebuffer AA mode, this function Load 2 or
 * 3 textures. In FSAA mode, two RGBA16F and one DEPTH24_STENCIL8 textures
 * are created. In other modes, one RGBA16F and one DEPTH24_STENCIL8 textures
 * are created.
 */
bool LoadSceneFramebufferTexture()
{
    if (glIsTexture(g_gl.textures[TEXTURE_COLOR_BUFFER]))
        glDeleteTextures(1, &g_gl.textures[TEXTURE_COLOR_BUFFER]);
    if (glIsTexture(g_gl.textures[TEXTURE_DEPTH_BUFFER]))
        glDeleteTextures(1, &g_gl.textures[TEXTURE_DEPTH_BUFFER]);
    glGenTextures(1, &g_gl.textures[TEXTURE_DEPTH_BUFFER]);
    glGenTextures(1, &g_gl.textures[TEXTURE_COLOR_BUFFER]);

    switch (g_framebuffer.aa) {
    case AA_NONE:
        LOG("Loading {Z-Buffer-Framebuffer-Texture}");
        glActiveTexture(GL_TEXTURE0 + TEXTURE_DEPTH_BUFFER);
        glBindTexture(GL_TEXTURE_2D, g_gl.textures[TEXTURE_DEPTH_BUFFER]);
        glTexStorage2D(GL_TEXTURE_2D,
            1,
            GL_DEPTH24_STENCIL8,
            g_framebuffer.w,
            g_framebuffer.h);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        LOG("Loading {Color-Buffer-Framebuffer-Texture}");
        glActiveTexture(GL_TEXTURE0 + TEXTURE_COLOR_BUFFER);
        glBindTexture(GL_TEXTURE_2D, g_gl.textures[TEXTURE_COLOR_BUFFER]);
        glTexStorage2D(GL_TEXTURE_2D,
            1,
            GL_RGBA32F,
            g_framebuffer.w,
            g_framebuffer.h);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        break;
    case AA_MSAA2:
    case AA_MSAA4:
    case AA_MSAA8:
    case AA_MSAA16: {
        int samples = 1 << g_framebuffer.aa;

        int maxSamples;
        int maxSamplesDepth;
        glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &maxSamples);
        glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &maxSamplesDepth);
        maxSamples = maxSamplesDepth < maxSamples ? maxSamplesDepth : maxSamples;

        if (samples > maxSamples) {
            LOG("note: MSAA is %ix\n", maxSamples);
            samples = maxSamples;
        }
        LOG("Loading {Scene-MSAA-Z-Framebuffer-Texture}");
        glActiveTexture(GL_TEXTURE0 + TEXTURE_DEPTH_BUFFER);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, g_gl.textures[TEXTURE_DEPTH_BUFFER]);
        glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,
                                  samples,
                                  GL_DEPTH24_STENCIL8,
                                  g_framebuffer.w,
                                  g_framebuffer.h,
                                  g_framebuffer.msaa.fixed);

        LOG("Loading {Scene-MSAA-RGBA-Framebuffer-Texture}");
        glActiveTexture(GL_TEXTURE0 + TEXTURE_COLOR_BUFFER);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, g_gl.textures[TEXTURE_COLOR_BUFFER]);
        glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,
                                  samples,
                                  GL_RGBA8,
                                  g_framebuffer.w,
                                  g_framebuffer.h,
                                  g_framebuffer.msaa.fixed);
    } break;
    }
    glActiveTexture(GL_TEXTURE0);

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------
/**
 * Load All Textures
 */
bool LoadTextures()
{
    bool v = true;

    if (v) v &= LoadSceneFramebufferTexture();

    return v;
}

////////////////////////////////////////////////////////////////////////////////
// Buffer Loading
//
////////////////////////////////////////////////////////////////////////////////

GLuint
LoadCatmullClarkBuffer(
    int32_t bufferID,
    GLsizeiptr bufferByteSize,
    const void *data,
    GLbitfield flags
) {
    GLuint *buffer = &g_gl.buffers[bufferID];

    if (glIsBuffer(*buffer))
        glDeleteBuffers(1, buffer);
    glGenBuffers(1, buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, *buffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER,
                    bufferByteSize,
                    data,
                    flags);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bufferID, *buffer);

    return glGetError() == GL_NO_ERROR;
}

bool LoadCageVertexToHalfedgeIDsBuffer(const cc_Mesh *cage)
{
    LOG("Loading {Cage-Vertex-To-Halfedge-Buffer}");
    return LoadCatmullClarkBuffer(BUFFER_CAGE_VERTEX_TO_HALFEDGE,
                                  ccm_VertexCount(cage) * sizeof(int32_t),
                                  cage->vertexToHalfedgeIDs,
                                  0);
}

bool LoadCageEdgeToHalfedgeIDsBuffer(const cc_Mesh *cage)
{
    LOG("Loading {Cage-Edge-To-Halfedge-Buffer}");
    return LoadCatmullClarkBuffer(BUFFER_CAGE_EDGE_TO_HALFEDGE,
                                  ccm_EdgeCount(cage) * sizeof(int32_t),
                                  cage->edgeToHalfedgeIDs,
                                  0);
}

bool LoadCageFaceToHalfedgeIDsBuffer(const cc_Mesh *cage)
{
    LOG("Loading {Cage-Face-To-Halfedge-Buffer}");
    return LoadCatmullClarkBuffer(BUFFER_CAGE_FACE_TO_HALFEDGE,
                                  ccm_FaceCount(cage) * sizeof(int32_t),
                                  cage->faceToHalfedgeIDs,
                                  0);
}

bool LoadCageHalfedgeBuffer(const cc_Mesh *cage)
{
    LOG("Loading {Cage-Halfedge-Buffer}");
    return LoadCatmullClarkBuffer(BUFFER_CAGE_HALFEDGES,
                                  sizeof(cc_Halfedge) * ccm_HalfedgeCount(cage),
                                  cage->halfedges,
                                  0);
}

bool LoadCageCreaseBuffer(const cc_Mesh *cage)
{
    LOG("Loading {Cage-Crease-Buffer}");
    return LoadCatmullClarkBuffer(BUFFER_CAGE_CREASES,
                                  sizeof(cc_Crease) * ccm_CreaseCount(cage),
                                  cage->creases,
                                  0);
}

bool LoadCageVertexPointBuffer(const cc_Mesh *cage)
{
    LOG("Loading {Cage-Vertex-Buffer}");
    return LoadCatmullClarkBuffer(BUFFER_CAGE_VERTEX_POINTS,
                                  sizeof(cc_VertexPoint) * ccm_VertexCount(cage),
                                  cage->vertexPoints,
                                  GL_MAP_WRITE_BIT);
}

bool LoadCageVertexUvBuffer(const cc_Mesh *cage)
{
    LOG("Loading {Cage-UV-Buffer}");
    return true;
    return LoadCatmullClarkBuffer(BUFFER_CAGE_VERTEX_UVS,
                                  sizeof(cc_VertexUv) * ccm_UvCount(cage),
                                  cage->uvs,
                                  0);
}

bool LoadSubdVertexPointBuffer(const cc_Subd *subd)
{
    LOG("Loading {Subd-Vertex-Buffer}");
    return LoadCatmullClarkBuffer(BUFFER_SUBD_VERTEX_POINTS,
                                  sizeof(cc_VertexPoint) * ccs_CumulativeVertexCount(subd),
                                  subd->vertexPoints,
                                  0);
}

bool LoadSubdHalfedgeBuffer(const cc_Subd *subd)
{
    LOG("Loading {Subd-Halfedge-Buffer}");
    return LoadCatmullClarkBuffer(BUFFER_SUBD_HALFEDGES,
                                  sizeof(cc_Halfedge_SemiRegular) * ccs_CumulativeHalfedgeCount(subd),
                                  subd->halfedges,
                                  0);
}

bool LoadSubdCreaseBuffer(const cc_Subd *subd)
{
    LOG("Loading {Subd-Crease-Buffer}");
    return LoadCatmullClarkBuffer(BUFFER_SUBD_CREASES,
                                  sizeof(cc_Crease) * ccs_CumulativeCreaseCount(subd),
                                  subd->creases,
                                  0);
}

bool LoadCageCounterBuffer(const cc_Mesh *cage)
{
    const struct {
        int32_t vertexCount;
        int32_t halfedgeCount;
        int32_t edgeCount;
        int32_t faceCount;
        int32_t uvCount;
    } counters = {
        ccm_VertexCount(cage),
        ccm_HalfedgeCount(cage),
        ccm_EdgeCount(cage),
        ccm_FaceCount(cage),
        ccm_UvCount(cage)
    };

    return LoadCatmullClarkBuffer(BUFFER_CAGE_COUNTERS,
                                  sizeof(counters),
                                  &counters,
                                  0);
}

bool LoadSubdMaxDepthBuffer(const cc_Subd *subd)
{
    const int32_t maxDepth = ccs_MaxDepth(subd);

    return LoadCatmullClarkBuffer(BUFFER_SUBD_MAXDEPTH,
                                  sizeof(maxDepth),
                                  &maxDepth,
                                  0);
}

// -----------------------------------------------------------------------------
/**
 * Load CBT Buffer
 *
 * This procedure initializes the CBT buffer.
 */
bool LoadCbtBuffer()
{
    cbt_Tree *cbt = cct_Create(g_mesh.subd.subd);

    LOG("Loading {Subd-Buffer}");
    if (glIsBuffer(g_gl.buffers[BUFFER_CBT]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_CBT]);
    glGenBuffers(1, &g_gl.buffers[BUFFER_CBT]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_gl.buffers[BUFFER_CBT]);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER,
                    cbt_HeapByteSize(cbt),
                    cbt_GetHeap(cbt),
                    0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    cbt_Release(cbt);

    return (glGetError() == GL_NO_ERROR);
}

bool LoadCbtDispatchBuffer()
{
    uint32_t dispatchCmd[8] = {1, 1, 1, 0, 0, 0, 0, 0};

    LOG("Loading {Cbt-Dispatch-Buffer}");

    if (glIsBuffer(g_gl.buffers[BUFFER_CBT_DISPATCH]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_CBT_DISPATCH]);

    glGenBuffers(1, &g_gl.buffers[BUFFER_CBT_DISPATCH]);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, g_gl.buffers[BUFFER_CBT_DISPATCH]);
    glBufferStorage(GL_DISPATCH_INDIRECT_BUFFER,
                    sizeof(dispatchCmd),
                    dispatchCmd,
                    0);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);

    return (glGetError() == GL_NO_ERROR);
}

bool LoadCctDispatchBuffer()
{
    uint32_t drawArraysCmd[8] = {1, 1, 0, 0, 0, 0, 0, 0};

    LOG("Loading {Cct-Draw-Buffer}");

    if (glIsBuffer(g_gl.buffers[BUFFER_CCT_DRAW]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_CCT_DRAW]);

    glGenBuffers(1, &g_gl.buffers[BUFFER_CCT_DRAW]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_CCT_DRAW]);
    glBufferStorage(GL_DRAW_INDIRECT_BUFFER,
                    sizeof(drawArraysCmd),
                    drawArraysCmd,
                    0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    return (glGetError() == GL_NO_ERROR);
}


bool LoadCctFaceCountBuffer()
{
    GLuint *buffer = &g_gl.buffers[BUFFER_CCT_FACE_COUNT];

    LOG("Loading {Cct-Face-Count-Buffer}");
    if (glIsBuffer(*buffer)) glDeleteBuffers(1, buffer);

    glGenBuffers(1, buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, *buffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER,
                    sizeof(int32_t),
                    NULL,
                    GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_CLIENT_STORAGE_BIT);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------
/**
 * Load Transformation buffer UBO
 *
 * This procedure updates the transformation matrices; it is updated each frame.
 */
bool LoadXformVariables()
{
    static bool first = true;
    struct PerFrameVariables {
        dja::mat4 model,                // 16
                  modelView,            // 16
                  view,                 // 16
                  camera,               // 16
                  viewProjection,       // 16
                  modelViewProjection;  // 16
        dja::vec4 frustum[6];           // 24
        dja::vec4 align[2];             // 8
    } variables;

    if (first) {
        g_gl.streams[STREAM_XFORM_VARIABLES] = djgb_create(sizeof(variables));
        first = false;
    }

    // build zoom in projection matrix
    const float zoomFactor = exp2(-g_camera.frameZoom.factor);
    const float x = g_camera.frameZoom.x;
    const float y = g_camera.frameZoom.y;
    dja::mat4 zoom = dja::mat4::homogeneous::tile(x - zoomFactor,
                                                  x + zoomFactor,
                                                  y - zoomFactor,
                                                  y + zoomFactor);

    // extract view and projection matrices
    dja::mat4 projection;
    if (g_camera.projection == PROJECTION_ORTHOGRAPHIC) {
        float ratio = (float)g_framebuffer.w / (float)g_framebuffer.h;
        float planeSize = tan(Radians(g_camera.fovy / 2.0f));

        projection = dja::mat4::homogeneous::orthographic(
            -planeSize * ratio, planeSize * ratio, -planeSize, planeSize,
            g_camera.zNear, g_camera.zFar
        );
    } else if (g_camera.projection == PROJECTION_RECTILINEAR) {
        projection = dja::mat4::homogeneous::perspective(
            Radians(g_camera.fovy),
            (float)g_framebuffer.w / (float)g_framebuffer.h,
            g_camera.zNear, g_camera.zFar
        );
    } else {
        projection = dja::mat4::homogeneous::perspective(
            Radians(g_camera.fovy),
            (float)g_framebuffer.w / (float)g_framebuffer.h,
            g_camera.zNear, g_camera.zFar
        );
    }

    projection = zoom * projection;

    dja::mat4 viewInv = dja::mat4::homogeneous::translation(g_camera.pos)
        * dja::mat4::homogeneous::from_mat3(g_camera.axis);
    dja::mat4 view = dja::inverse(viewInv);
    dja::mat4 model = dja::mat4::homogeneous::rotation(dja::vec3(1, 0, 0), 0.0f);

    // set transformations (column-major)
    variables.model = dja::transpose(model);
    variables.view = dja::transpose(view);
    variables.camera = dja::transpose(viewInv);
    variables.modelView = dja::transpose(view * model);
    variables.modelViewProjection = dja::transpose(projection * view * model);
    variables.viewProjection = dja::transpose(projection * view);

    // extract frustum planes
    dja::mat4 mvp = variables.modelViewProjection;
    for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 2; ++j) {
        variables.frustum[i*2+j].x = mvp[0][3] + (j == 0 ? mvp[0][i] : -mvp[0][i]);
        variables.frustum[i*2+j].y = mvp[1][3] + (j == 0 ? mvp[1][i] : -mvp[1][i]);
        variables.frustum[i*2+j].z = mvp[2][3] + (j == 0 ? mvp[2][i] : -mvp[2][i]);
        variables.frustum[i*2+j].w = mvp[3][3] + (j == 0 ? mvp[3][i] : -mvp[3][i]);
        dja::vec4 tmp = variables.frustum[i*2+j];
        variables.frustum[i*2+j]*= dja::norm(dja::vec3(tmp.x, tmp.y, tmp.z));
    }

    // upLoad to GPU
    djgb_to_gl(g_gl.streams[STREAM_XFORM_VARIABLES], (const void *)&variables, NULL);
    djgb_glbindrange(g_gl.streams[STREAM_XFORM_VARIABLES],
                     GL_UNIFORM_BUFFER,
                     STREAM_XFORM_VARIABLES);

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------
/**
 * Load All Buffers
 *
 */
bool LoadBuffers()
{
    bool success = true;

    if (success) success = LoadXformVariables();
    if (success) success = LoadCbtBuffer();
    if (success) success = LoadCbtDispatchBuffer();
    if (success) success = LoadCctDispatchBuffer();
    if (success) success = LoadCctFaceCountBuffer();

    if (success) success = LoadCageVertexToHalfedgeIDsBuffer(g_mesh.subd.cage);
    if (success) success = LoadCageEdgeToHalfedgeIDsBuffer(g_mesh.subd.cage);
    if (success) success = LoadCageFaceToHalfedgeIDsBuffer(g_mesh.subd.cage);
    if (success) success = LoadCageHalfedgeBuffer(g_mesh.subd.cage);
    if (success) success = LoadCageCreaseBuffer(g_mesh.subd.cage);
    if (success) success = LoadCageVertexPointBuffer(g_mesh.subd.cage);
    if (success) success = LoadCageVertexUvBuffer(g_mesh.subd.cage);
    if (success) success = LoadCageCounterBuffer(g_mesh.subd.cage);
    if (success) success = LoadSubdHalfedgeBuffer(g_mesh.subd.subd);
    if (success) success = LoadSubdVertexPointBuffer(g_mesh.subd.subd);
    if (success) success = LoadSubdCreaseBuffer(g_mesh.subd.subd);
    if (success) success = LoadSubdMaxDepthBuffer(g_mesh.subd.subd);


    return success;
}

////////////////////////////////////////////////////////////////////////////////
// Vertex Array Loading
//
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
/**
 * Load an Empty Vertex Array
 *
 * This will be used to draw procedural geometry, e.g., a fullscreen quad.
 */
bool LoadEmptyVertexArray()
{
    LOG("Loading {Empty-VertexArray}");
    if (glIsVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]))
        glDeleteVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_EMPTY]);

    glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
    glBindVertexArray(0);

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------
/**
 * Load All Vertex Arrays
 *
 */
bool LoadVertexArrays()
{
    bool v = true;

    if (v) v &= LoadEmptyVertexArray();

    return v;
}

// -----------------------------------------------------------------------------
/**
 * Load the Scene Framebuffer
 *
 * This framebuffer is used to draw the 3D scene.
 * A single framebuffer is created, holding a color and Z buffer.
 * The scene writes directly to it.
 */
bool LoadSceneFramebuffer()
{
    LOG("Loading {Scene-Framebuffer}");
    if (glIsFramebuffer(g_gl.framebuffers[FRAMEBUFFER_SCENE]))
        glDeleteFramebuffers(1, &g_gl.framebuffers[FRAMEBUFFER_SCENE]);

    glGenFramebuffers(1, &g_gl.framebuffers[FRAMEBUFFER_SCENE]);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gl.framebuffers[FRAMEBUFFER_SCENE]);

    if (g_framebuffer.aa >= AA_MSAA2 && g_framebuffer.aa <= AA_MSAA16) {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D_MULTISAMPLE,
            g_gl.textures[TEXTURE_COLOR_BUFFER],
            0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D_MULTISAMPLE,
            g_gl.textures[TEXTURE_DEPTH_BUFFER],
            0);
    } else {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            g_gl.textures[TEXTURE_COLOR_BUFFER],
            0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D,
            g_gl.textures[TEXTURE_DEPTH_BUFFER],
            0);
    }

    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    if (GL_FRAMEBUFFER_COMPLETE != glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
        LOG("=> Failure <=");

        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return (glGetError() == GL_NO_ERROR);
}

// -----------------------------------------------------------------------------
/**
 * Load All Framebuffers
 *
 */
bool LoadFramebuffers()
{
    bool v = true;

    if (v) v &= LoadSceneFramebuffer();

    return v;
}

////////////////////////////////////////////////////////////////////////////////
// OpenGL Resource Loading
//
////////////////////////////////////////////////////////////////////////////////

void DisplaceSubd()
{
#if 0
    const int32_t vertexCount = ccs_VertexCount(g_mesh.subd.subd);

    glUseProgram(g_gl.programs[PROGRAM_SUBD_DISPLACE]);
    glDispatchCompute(vertexCount / 256 + 1, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glUseProgram(0);
#endif
}

void LoadKeyframes()
{
    const int32_t frameCount = 48;
    cc_Mesh **frames = (cc_Mesh **)malloc(sizeof(cc_Mesh *) * frameCount);
    char buffer[256];

    for (int32_t frameID = 0; frameID < frameCount; ++frameID) {
        sprintf(buffer,
                PATH_TO_ASSET_DIRECTORY "ch_Trex-walk-%02i.ccm",
                frameID);
        frames[frameID] = ccm_Load(buffer);
    }

    g_mesh.animation.frameCount = frameCount;
    g_mesh.animation.frames = frames;
}

void ReleaseKeyframes()
{
    const int32_t frameCount = g_mesh.animation.frameCount;
    cc_Mesh **frames = g_mesh.animation.frames;

    for (int32_t frameID = 0; frameID < frameCount; ++frameID) {
        ccm_Release(frames[frameID]);
    }

    free(frames);
}

void Load(int argc, char **argv)
{
    bool v = true;
    const char *filename = PATH_TO_ASSET_DIRECTORY "ch_Trex-walk-00.ccm";

    if (argc > 1) {
        filename = argv[1];
        g_mesh.subd.maxDepth = atoi(argv[2]);
    }

    g_mesh.subd.cage = ccm_Load(filename);
    g_mesh.subd.subd = ccs_Create(g_mesh.subd.cage, g_mesh.subd.maxDepth);
    ccs_Refine_Scatter(g_mesh.subd.subd);

    for (int i = 0; i < CLOCK_COUNT; ++i) {
        if (g_gl.clocks[i] != NULL)
            djgc_release(g_gl.clocks[i]);
        g_gl.clocks[i] = djgc_create();
    }

    if (v) v &= LoadTextures();
    if (v) v &= LoadBuffers();
    if (v) v &= LoadFramebuffers();
    if (v) v &= LoadVertexArrays();
    if (v) v &= LoadPrograms();

    LoadKeyframes();
    DisplaceSubd();

    updateCameraMatrix();

    if (!v) throw std::exception();
}

void Release()
{
    int i;

    for (i = 0; i < CLOCK_COUNT; ++i)
        if (g_gl.clocks[i])
            djgc_release(g_gl.clocks[i]);
    for (i = 0; i < STREAM_COUNT; ++i)
        if (g_gl.streams[i])
            djgb_release(g_gl.streams[i]);
    for (i = 0; i < PROGRAM_COUNT; ++i)
        if (glIsProgram(g_gl.programs[i]))
            glDeleteProgram(g_gl.programs[i]);
    for (i = 0; i < TEXTURE_COUNT; ++i)
        if (glIsTexture(g_gl.textures[i]))
            glDeleteTextures(1, &g_gl.textures[i]);
    for (i = 0; i < BUFFER_COUNT; ++i)
        if (glIsBuffer(g_gl.buffers[i]))
            glDeleteBuffers(1, &g_gl.buffers[i]);
    for (i = 0; i < FRAMEBUFFER_COUNT; ++i)
        if (glIsFramebuffer(g_gl.framebuffers[i]))
            glDeleteFramebuffers(1, &g_gl.framebuffers[i]);
    for (i = 0; i < VERTEXARRAY_COUNT; ++i)
        if (glIsVertexArray(g_gl.vertexArrays[i]))
            glDeleteVertexArrays(1, &g_gl.vertexArrays[i]);

    ccs_Release(g_mesh.subd.subd);
    ccm_Release(g_mesh.subd.cage);

    ReleaseKeyframes();
}

////////////////////////////////////////////////////////////////////////////////
// OpenGL Rendering
//
////////////////////////////////////////////////////////////////////////////////


void CageSubdivisionCommand(int32_t programID, int32_t threadCount)
{
    int32_t count = (threadCount >> g_mesh.subd.computeShaderLocalSize) + 1;

    glUseProgram(g_gl.programs[programID]);
    glDispatchCompute(count, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glUseProgram(0);
}

void SubdivisionCommand(int32_t programID, int32_t threadCount, int32_t depth)
{
    int32_t count = (threadCount >> g_mesh.subd.computeShaderLocalSize) + 1;
    int32_t uniformLocation =
            glGetUniformLocation(g_gl.programs[programID], "u_Depth");

    glUseProgram(g_gl.programs[programID]);
    glUniform1i(uniformLocation, depth);
    glDispatchCompute(count, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glUseProgram(0);
}

void RefineCageHalfedgesCommand()
{
    CageSubdivisionCommand(PROGRAM_SUBD_CAGE_HALFEDGES,
                           ccm_HalfedgeCount(g_mesh.subd.cage));
}

void RefineCageCreasesCommand()
{
    CageSubdivisionCommand(PROGRAM_SUBD_CAGE_CREASES,
                           ccm_CreaseCount(g_mesh.subd.cage));
}

void RefineCageVertexUvsCommand()
{
    CageSubdivisionCommand(PROGRAM_SUBD_CAGE_VERTEX_UVS,
                           ccm_HalfedgeCount(g_mesh.subd.cage));
}

void RefineCageFacesCommand()
{
#ifdef SCATTER_IMPLEMENTATION
    CageSubdivisionCommand(PROGRAM_SUBD_CAGE_FACE_POINTS,
                           ccm_HalfedgeCount(g_mesh.subd.cage));
#else
    CageSubdivisionCommand(PROGRAM_SUBD_CAGE_FACE_POINTS,
                           ccm_FaceCount(g_mesh.subd.cage));
#endif
}

void RefineCageEdgesCommand()
{
#ifdef SCATTER_IMPLEMENTATION
    CageSubdivisionCommand(PROGRAM_SUBD_CAGE_EDGE_POINTS,
                           ccm_HalfedgeCount(g_mesh.subd.cage));
#else
    CageSubdivisionCommand(PROGRAM_SUBD_CAGE_EDGE_POINTS,
                           ccm_EdgeCount(g_mesh.subd.cage));
#endif
}

void RefineCageVerticesCommand()
{
#ifdef SCATTER_IMPLEMENTATION
    CageSubdivisionCommand(PROGRAM_SUBD_CAGE_VERTEX_POINTS,
                           ccm_HalfedgeCount(g_mesh.subd.cage));
#else
    CageSubdivisionCommand(PROGRAM_SUBD_CAGE_VERTEX_POINTS,
                           ccm_VertexCount(g_mesh.subd.cage));
#endif
}

void RefineHalfedgesCommand(int32_t depth)
{
    SubdivisionCommand(PROGRAM_SUBD_HALFEDGES,
                       ccm_HalfedgeCountAtDepth(g_mesh.subd.cage, depth),
                       depth);
}

void RefineCreasesCommand(int32_t depth)
{
    SubdivisionCommand(PROGRAM_SUBD_CREASES,
                       ccm_CreaseCountAtDepth(g_mesh.subd.cage, depth),
                       depth);
}

void RefineVertexUvsCommand(int32_t depth)
{
    SubdivisionCommand(PROGRAM_SUBD_VERTEX_UVS,
                       ccm_HalfedgeCountAtDepth(g_mesh.subd.cage, depth),
                       depth);
}

void RefineFacesCommand(int32_t depth)
{
#ifdef SCATTER_IMPLEMENTATION
    SubdivisionCommand(PROGRAM_SUBD_FACE_POINTS,
                       ccm_HalfedgeCountAtDepth(g_mesh.subd.cage, depth),
                       depth);
#else
    SubdivisionCommand(PROGRAM_SUBD_FACE_POINTS,
                       ccm_FaceCountAtDepth_Fast(g_mesh.subd.cage, depth),
                       depth);
#endif
}

void RefineEdgesCommand(int32_t depth)
{
#ifdef SCATTER_IMPLEMENTATION
    SubdivisionCommand(PROGRAM_SUBD_EDGE_POINTS,
                       ccm_HalfedgeCountAtDepth(g_mesh.subd.cage, depth),
                       depth);
#else
    SubdivisionCommand(PROGRAM_SUBD_EDGE_POINTS,
                       ccm_EdgeCountAtDepth_Fast(g_mesh.subd.cage, depth),
                       depth);
#endif
}

void RefineVertexPointsCommand(int32_t depth)
{
#ifdef SCATTER_IMPLEMENTATION
    SubdivisionCommand(PROGRAM_SUBD_VERTEX_POINTS,
                       ccm_HalfedgeCountAtDepth(g_mesh.subd.cage, depth),
                       depth);
#else
    SubdivisionCommand(PROGRAM_SUBD_VERTEX_POINTS,
                       ccm_VertexCountAtDepth_Fast(g_mesh.subd.cage, depth),
                       depth);
#endif
}

void RefineVertexPoints()
{
#ifdef SCATTER_IMPLEMENTATION
    glClearNamedBufferData(g_gl.buffers[BUFFER_SUBD_VERTEX_POINTS],
                           GL_R32F,
                           GL_RED,
                           GL_FLOAT,
                           NULL);
#endif
    RefineCageFacesCommand();
    RefineCageEdgesCommand();
    RefineCageVerticesCommand();

    for (int32_t depth = 1; depth < ccs_MaxDepth(g_mesh.subd.subd); ++depth) {
        RefineFacesCommand(depth);
        RefineEdgesCommand(depth);
        RefineVertexPointsCommand(depth);
    }
}

void RefineHalfedges()
{
    RefineCageHalfedgesCommand();

    for (int32_t depth = 1; depth < ccs_MaxDepth(g_mesh.subd.subd); ++depth) {
        RefineHalfedgesCommand(depth);
    }
}

void RefineCreases()
{
    RefineCageCreasesCommand();

    for (int32_t depth = 1; depth < ccs_MaxDepth(g_mesh.subd.subd); ++depth) {
        RefineCreasesCommand(depth);
    }
}

void RefineVertexUvs()
{
    RefineCageVertexUvsCommand();

    for (int32_t depth = 1; depth < ccs_MaxDepth(g_mesh.subd.subd); ++depth) {
        RefineVertexUvsCommand(depth);
    }
}

void Animate(const float dt)
{
    const float fps = (float)g_mesh.animation.frameCount;
    const float animationLength = 1.0f;
    float u = g_mesh.animation.time;
    float dummy;
    int frameID, nextFrameID;
    cc_VertexPoint *vertices;

    u+= dt / animationLength;
    u = std::modf(u, &dummy);
    frameID = floor(u * fps);
    nextFrameID = (frameID + 1) % g_mesh.animation.frameCount;

    vertices = (cc_VertexPoint *)glMapNamedBuffer(g_gl.buffers[BUFFER_CAGE_VERTEX_POINTS],
                                                  GL_WRITE_ONLY);

    for (int32_t vertexID = 0; vertexID < ccm_VertexCount(g_mesh.subd.cage); ++vertexID) {
        const cc_VertexPoint v0 = ccm_VertexPoint(g_mesh.animation.frames[frameID], vertexID);
        const cc_VertexPoint v1 = ccm_VertexPoint(g_mesh.animation.frames[nextFrameID], vertexID);
        const float start = (float)frameID / fps;
        const float lerp = (u - start) * fps;

        cc__Lerp3f(vertices[vertexID].array, v0.array, v1.array, lerp);
    }
    glUnmapNamedBuffer(g_gl.buffers[BUFFER_CAGE_VERTEX_POINTS]);

    glFinish();
    djgc_start(g_gl.clocks[CLOCK_SUBD]);
    if (!g_mesh.flags.fixTopology) {
        RefineHalfedges();
        RefineCreases();
    }
    RefineVertexPoints();
    djgc_stop(g_gl.clocks[CLOCK_SUBD]);
    glFinish();

    g_mesh.animation.time = u;
}

// -----------------------------------------------------------------------------
/**
 * Reduction Pass -- Generic
 *
 * The reduction prepass is used for counting the number of nodes and
 * dispatch the threads to the proper node. This routine is entirely
 * generic and isn't tied to a specific pipeline.
 */
void CbtReductionPass()
{
    int it = cct__MaxBisectorDepth(g_mesh.subd.subd)
           + cct__MinCbtDepth(g_mesh.subd.subd);

    djgc_start(g_gl.clocks[CLOCK_REDUCTION]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_CBT, g_gl.buffers[BUFFER_CBT]);
    glUseProgram(g_gl.programs[PROGRAM_CBT_REDUCTION_PREPASS]);
    if (true) {
        int cnt = ((1 << it) >> 5);
        int numGroup = (cnt >= 256) ? (cnt >> 8) : 1;
        int loc = glGetUniformLocation(g_gl.programs[PROGRAM_CBT_REDUCTION_PREPASS],
                                       "u_PassID");

        djgc_start(g_gl.clocks[CLOCK_REDUCTION00 + it - 1]);
        glUniform1i(loc, it);
        glDispatchCompute(numGroup, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        djgc_stop(g_gl.clocks[CLOCK_REDUCTION00 + cct__MaxBisectorDepth(g_mesh.subd.subd) - 1]);

        it-= 5;
    }

    glUseProgram(g_gl.programs[PROGRAM_CBT_REDUCTION]);
    while (--it >= 0) {
        int loc = glGetUniformLocation(g_gl.programs[PROGRAM_CBT_REDUCTION], "u_PassID");
        int cnt = 1 << it;
        int numGroup = (cnt >= 256) ? (cnt >> 8) : 1;

        djgc_start(g_gl.clocks[CLOCK_REDUCTION00 + it]);
        glUniform1i(loc, it);
        glDispatchCompute(numGroup, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        djgc_stop(g_gl.clocks[CLOCK_REDUCTION00 + it]);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_CBT, 0);
    djgc_stop(g_gl.clocks[CLOCK_REDUCTION]);
}

void CbtDispatchPass()
{
    djgc_start(g_gl.clocks[CLOCK_BATCH]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_CBT,
                     g_gl.buffers[BUFFER_CBT]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_CBT_DISPATCH,
                     g_gl.buffers[BUFFER_CBT_DISPATCH]);
    glUseProgram(g_gl.programs[PROGRAM_CBT_DISPATCHER]);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glUseProgram(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_CBT, 0);
    djgc_stop(g_gl.clocks[CLOCK_BATCH]);
}

void CctDispatchPass()
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_CBT,
                     g_gl.buffers[BUFFER_CBT]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_CCT_DRAW,
                     g_gl.buffers[BUFFER_CCT_DRAW]);
    glUseProgram(g_gl.programs[PROGRAM_CCT_DISPATCHER]);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glUseProgram(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_CCT_DRAW, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_CBT, 0);
}

void CbtUpdatePass()
{
    static int pingPong = 0;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_CBT, g_gl.buffers[BUFFER_CBT]);

    //djgc_start(g_gl.clocks[CLOCK_TESSELLATION]);
    // set GL state
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER,
                 g_gl.buffers[BUFFER_CBT_DISPATCH]);

    // update
    glUseProgram(g_gl.programs[PROGRAM_CCT_SPLIT + pingPong]);
    glDispatchComputeIndirect(0);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // reset GL state
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
    //djgc_stop(g_gl.clocks[CLOCK_TESSELLATION]);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_CBT, 0);
    pingPong = 1 - pingPong;
}

// -----------------------------------------------------------------------------
void RenderCage()
{
    djgc_start(g_gl.clocks[CLOCK_RENDER]);
    LoadXformVariables();
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(g_gl.programs[PROGRAM_CAGE_RENDER]);
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
        glDrawArrays(GL_POINTS, 0, ccm_FaceCount(g_mesh.subd.cage));
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    djgc_stop(g_gl.clocks[CLOCK_RENDER]);
}


// -----------------------------------------------------------------------------
void RenderSubd()
{
    const int32_t depth = g_mesh.subd.subdDepth;

    djgc_start(g_gl.clocks[CLOCK_RENDER]);
    LoadXformVariables();
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    //glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
    //                 BUFFER_SUBD_HALFEDGES,
    //                 g_gl.buffers[BUFFER_SUBD_HALFEDGES]);
    //glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
    //                 BUFFER_SUBD_VERTEX_POINTS,
    //                 g_gl.buffers[BUFFER_SUBD_HALFEDGES]);
    glUseProgram(g_gl.programs[PROGRAM_SUBD_RENDER]);
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
        glDrawArrays(GL_POINTS, 0, ccm_FaceCountAtDepth_Fast(g_mesh.subd.cage, depth));
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    djgc_stop(g_gl.clocks[CLOCK_RENDER]);
}


// -----------------------------------------------------------------------------
void RenderUniformLod()
{
    const int32_t depth = g_mesh.subd.uniformLodDepth;
    const int32_t faceCount = ccm_HalfedgeCount(g_mesh.subd.cage) << depth;
    const int32_t vertexCount = faceCount * 3;

    djgc_start(g_gl.clocks[CLOCK_RENDER]);
    LoadXformVariables();
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(g_gl.programs[PROGRAM_LOD_UNIFORM_RENDER]);
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    djgc_stop(g_gl.clocks[CLOCK_RENDER]);
}


// -----------------------------------------------------------------------------
void RenderAdaptiveLod()
{
    LoadXformVariables();

    djgc_start(g_gl.clocks[CLOCK_TESSELLATION]);
    CbtDispatchPass();
    CbtUpdatePass();
    CbtReductionPass();
    CctDispatchPass();
    djgc_stop(g_gl.clocks[CLOCK_TESSELLATION]);

    djgc_start(g_gl.clocks[CLOCK_RENDER]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_CBT,
                     g_gl.buffers[BUFFER_CBT]);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(g_gl.programs[PROGRAM_LOD_ADAPTIVE_RENDER]);
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_CCT_DRAW]);
    glDrawArraysIndirect(GL_TRIANGLES, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_CBT, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    djgc_stop(g_gl.clocks[CLOCK_RENDER]);
}


// -----------------------------------------------------------------------------
/**
 * Render Scene
 *
 * This procedure renders the scene to the back buffer.
 */
void RenderScene()
{
    if (g_mesh.flags.animate) {
#if 0
        Animate(1.0f / 60.0f);
#else
        static struct timespec lastTime;
        struct timespec thisTime;
        double delta;

        clock_gettime(CLOCK_MONOTONIC, &thisTime);
        delta = (thisTime.tv_sec - lastTime.tv_sec);
        delta+= (thisTime.tv_nsec - lastTime.tv_nsec) / 1000000000.0;

        Animate(delta);

        lastTime = thisTime;
#endif
    }

    djgc_start(g_gl.clocks[CLOCK_ALL]);
    switch (g_mesh.renderer) {
        case RENDERER_CAGE: RenderCage();           break;
        case RENDERER_SUBD: RenderSubd();           break;
        case RENDERER_ULOD: RenderUniformLod();     break;
        case RENDERER_ALOD: RenderAdaptiveLod();    break;
        default: break;
    }
    djgc_stop(g_gl.clocks[CLOCK_ALL]);
}

// -----------------------------------------------------------------------------
void PrintLargeNumber(const char *label, int32_t value)
{
    const int32_t M = value / 1000000;
    const int32_t K = (value - M * 1000000) / 1000;
    const int32_t r = value - M * 1000000 - K * 1000;

    if (value >= 1000000) {
        ImGui::Text("%s: %i,%03i,%03i", label, M, K, r);
    } else if (value >= 1000) {
        ImGui::Text("%s: %i,%03i", label, K, r);
    } else {
        ImGui::Text("%s: %i", label, value);
    }
}

void RenderViewer()
{
    // render framebuffer
    glUseProgram(g_gl.programs[PROGRAM_VIEWER]);
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // draw HUD
    if (g_app.viewer.hud) {
        // ImGui
        glUseProgram(0);
        // Viewer Widgets
        //ImGui_ImplGlfw_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        // Performance Widget
        ImGui::SetNextWindowPos(ImVec2(g_framebuffer.w - 310, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 240), ImGuiCond_FirstUseEver);
        ImGui::Begin("Performances");
        {
            double cpuDt, gpuDt;

            djgc_ticks(g_gl.clocks[CLOCK_RENDER], &cpuDt, &gpuDt);
            ImGui::Text("GPU : NVIDIA RTX 2080");
            ImGui::Text("FPS : %.1f (%.3f ms)", 1.f / gpuDt, gpuDt * 1e3);
            djgc_ticks(g_gl.clocks[CLOCK_SUBD], &cpuDt, &gpuDt);
            ImGui::Text("Subdivision : (%.3f ms)", gpuDt * 1e3);
            djgc_ticks(g_gl.clocks[CLOCK_TESSELLATION], &cpuDt, &gpuDt);
            ImGui::Text("Tessellation: (%.3f ms)", gpuDt * 1e3);
            //ImGui::Text("FPS %8.3f", 1.f / gpuDt);
#if 1
            ImGui::NewLine();
            ImGui::Text("Timings:");
            djgc_ticks(g_gl.clocks[CLOCK_RENDER], &cpuDt, &gpuDt);
            ImGui::Text("Render       -- CPU: %.3f%s",
                cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                cpuDt < 1. ? "ms" : " s");
            ImGui::SameLine();
            ImGui::Text("GPU: %.3f%s",
                gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                gpuDt < 1. ? "ms" : " s");
            djgc_ticks(g_gl.clocks[CLOCK_TESSELLATION], &cpuDt, &gpuDt);
            ImGui::Text("Tessellation -- CPU: %.3f%s",
                cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                cpuDt < 1. ? "ms" : " s");
            ImGui::SameLine();
            ImGui::Text("GPU: %.3f%s",
                gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                gpuDt < 1. ? "ms" : " s");
            djgc_ticks(g_gl.clocks[CLOCK_SUBD], &cpuDt, &gpuDt);
            ImGui::Text("Subdivision  -- CPU: %.3f%s",
                cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                cpuDt < 1. ? "ms" : " s");
            ImGui::SameLine();
            ImGui::Text("GPU: %.3f%s",
                gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                gpuDt < 1. ? "ms" : " s");
            djgc_ticks(g_gl.clocks[CLOCK_REDUCTION], &cpuDt, &gpuDt);
            ImGui::Text("Reduction    -- CPU: %.3f%s",
                cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                cpuDt < 1. ? "ms" : " s");
            ImGui::SameLine();
            ImGui::Text("GPU: %.3f%s",
                gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                gpuDt < 1. ? "ms" : " s");
            djgc_ticks(g_gl.clocks[CLOCK_BATCH], &cpuDt, &gpuDt);
            ImGui::Text("Dispatch     -- CPU: %.3f%s",
                cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                cpuDt < 1. ? "ms" : " s");
            ImGui::SameLine();
            ImGui::Text("GPU: %.3f%s",
                gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                gpuDt < 1. ? "ms" : " s");
#endif
        }
        ImGui::End();

#if 1
        // Camera Widget
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(250, 240), ImGuiCond_FirstUseEver);
        ImGui::Begin("Camera Settings");
        {
            const char* eProjections[] = {
                "Orthographic",
                "Rectilinear",
                "Fisheye"
            };
            const char* eTonemaps[] = {
                "Uncharted2",
                "Filmic",
                "Aces",
                "Reinhard",
                "Raw"
            };
            const char* eAA[] = {
                "None",
                "MSAAx2",
                "MSAAx4",
                "MSAAx8",
                "MSAAx16"
            };
            if (ImGui::Combo("Projection", &g_camera.projection, &eProjections[0], BUFFER_SIZE(eProjections))) {
            }
            if (ImGui::Combo("Tonemap", &g_camera.tonemap, &eTonemaps[0], BUFFER_SIZE(eTonemaps))) {
                LoadViewerProgram();
            }
            if (ImGui::Combo("AA", &g_framebuffer.aa, &eAA[0], BUFFER_SIZE(eAA))) {
                LoadSceneFramebufferTexture();
                LoadSceneFramebuffer();
                LoadViewerProgram();
            }
            if (ImGui::SliderFloat("FOVY", &g_camera.fovy, 1.0f, 179.0f)) {
                ConfigureTessellationPrograms();
            }
            if (ImGui::SliderFloat("zNear", &g_camera.zNear, 0.01f, 1.f)) {
                if (g_camera.zNear >= g_camera.zFar)
                    g_camera.zNear = g_camera.zFar - 0.01f;
            }
            if (ImGui::SliderFloat("zFar", &g_camera.zFar, 16.f, 4096.f)) {
                if (g_camera.zFar <= g_camera.zNear)
                    g_camera.zFar = g_camera.zNear + 0.01f;
            }
            if (ImGui::SliderFloat("frameZoom", &g_camera.frameZoom.factor, 0.f, 8.f)) {
                ConfigureTessellationPrograms();
            }
            ImGui::SliderFloat("frameX", &g_camera.frameZoom.x, -1.f, 1.f);
            ImGui::SliderFloat("frameY", &g_camera.frameZoom.y, -1.f, 1.f);

        }
        ImGui::End();

        // Mesh Parameters
        ImGui::SetNextWindowPos(ImVec2(270, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(380, 235), ImGuiCond_FirstUseEver);
        ImGui::Begin("Mesh Settings");
        {
            const char* renderers[] = {
                "Cage",
                "Subd",
                "LoD (uniform)",
                "LoD (adaptive)"
            };
            const char* shadings[] = {
                "Shaded",
                "UVs",
                "Normals",
                "Plain Color"
            };
            ImGui::Combo("Renderer", &g_mesh.renderer, &renderers[0], BUFFER_SIZE(renderers));

            if (ImGui::Checkbox("Wire", &g_mesh.flags.wire)) {
                LoadPrograms();
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Animate", &g_mesh.flags.animate)) {
                LoadAdaptiveLodRenderProgram();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Fix Topology", &g_mesh.flags.fixTopology);

            if (g_mesh.renderer == RENDERER_CAGE) {

                PrintLargeNumber("Faces", ccm_FaceCount(g_mesh.subd.cage));
                PrintLargeNumber("Verts", ccm_VertexCount(g_mesh.subd.cage));

            }
            if (g_mesh.renderer == RENDERER_SUBD) {
                if (ImGui::SliderInt("Depth", &g_mesh.subd.subdDepth, 1, g_mesh.subd.maxDepth)) {
                    ConfigureSubdRenderProgram();
                }
                PrintLargeNumber("Quads", ccm_FaceCountAtDepth_Fast(g_mesh.subd.cage, g_mesh.subd.subdDepth));
                PrintLargeNumber("Verts", ccm_VertexCountAtDepth_Fast(g_mesh.subd.cage, g_mesh.subd.subdDepth));
            }

            if (g_mesh.renderer == RENDERER_ULOD) {
                if (ImGui::SliderInt("LoD", &g_mesh.subd.uniformLodDepth, 0,  2 * g_mesh.subd.maxDepth - 1)) {
                    ConfigureUniformLodRenderProgram();
                }
                ImGui::Text("Triangles: %i", ccm_HalfedgeCount(g_mesh.subd.cage) << g_mesh.subd.uniformLodDepth);
            }
            if (g_mesh.renderer == RENDERER_ALOD) {
                if (ImGui::Checkbox("Cull", &g_mesh.flags.cull)) {
                    LoadPrograms();
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("Freeze", &g_mesh.flags.freeze)) {
                    LoadTessellationPrograms();
                }
                if (ImGui::Combo("Shading", &g_mesh.shading, &shadings[0], BUFFER_SIZE(shadings))) {
                    LoadAdaptiveLodRenderProgram();
                }
                if (ImGui::SliderFloat("PixelsPerEdge", &g_mesh.primitivePixelLengthTarget, 1.0f, 16.0f)) {
                    ConfigureTessellationPrograms();
                }

                {
                    const int32_t *faceCount;

                    glCopyNamedBufferSubData(g_gl.buffers[BUFFER_CCT_DRAW],
                                             g_gl.buffers[BUFFER_CCT_FACE_COUNT],
                                             0, 0, sizeof(int32_t));
                    faceCount = (const int32_t *)glMapNamedBuffer(
                        g_gl.buffers[BUFFER_CCT_FACE_COUNT], GL_READ_ONLY
                    );

                    PrintLargeNumber("Faces", *faceCount / 3);

                    glUnmapNamedBuffer(g_gl.buffers[BUFFER_CCT_FACE_COUNT]);
                }
                {
                    const int32_t minDepth = cct__MinCbtDepth(g_mesh.subd.subd);
                    const int32_t cbtDepth = minDepth + cct__MaxBisectorDepth(g_mesh.subd.subd);
                    const int32_t bufSize = cbt__HeapByteSize(cbtDepth);

                    if (bufSize < (1 << 10)) {
                        ImGui::Text("CBT buffer: %i Bytes", bufSize);
                    } else if (bufSize < (1 << 20)) {
                        ImGui::Text("CBT buffer: %i KBytes", bufSize >> 10);
                    } else {
                        ImGui::Text("CBT buffer: %i MBytes", bufSize >> 20);
                    }
                }
                {
                    const int32_t topologyBufferSize = ccs_CumulativeEdgeCount(g_mesh.subd.subd);
                    const int32_t bufSize = topologyBufferSize * sizeof(cc_Halfedge);

                    if (bufSize < (1 << 10)) {
                        ImGui::Text("Topology buffer: %i Bytes", bufSize);
                    } else if (bufSize < (1 << 20)) {
                        ImGui::Text("Topology buffer: %i KBytes", bufSize >> 10);
                    } else {
                        ImGui::Text("Topology buffer: %i MBytes", bufSize >> 20);
                    }
                }
                {
                    const int32_t vertexBufferSize = ccs_CumulativeVertexCount(g_mesh.subd.subd);
                    const int32_t bufSize = vertexBufferSize * sizeof(cc_VertexPoint);

                    if (bufSize < (1 << 10)) {
                        ImGui::Text("Vertex buffer: %i Bytes", bufSize);
                    } else if (bufSize < (1 << 20)) {
                        ImGui::Text("Vertex buffer: %i KBytes", bufSize >> 10);
                    } else {
                        ImGui::Text("Vertex buffer: %i MBytes", bufSize >> 20);
                    }
                }
            }
        }
        ImGui::End();
#endif

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // screen recording
    if (g_app.recorder.on) {
        char name[64], path[1024];

        glBindFramebuffer(GL_READ_FRAMEBUFFER, g_gl.framebuffers[FRAMEBUFFER_BACK]);
        sprintf(name, "capture_%02i_%09i",
                g_app.recorder.capture,
                g_app.recorder.frame);
        sprintf(path, "%s/%s", g_app.dir.output, name);
        djgt_save_glcolorbuffer_bmp(GL_BACK, GL_RGB, path);
        ++g_app.recorder.frame;
    }
}

// -----------------------------------------------------------------------------
/**
 * Render Everything
 *
 */
void Render()
{
    glBindFramebuffer(GL_FRAMEBUFFER, g_gl.framebuffers[FRAMEBUFFER_SCENE]);
    glViewport(0, 0, g_framebuffer.w, g_framebuffer.h);
    glClearColor(100.0, 100.0, 100.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderScene();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_app.viewer.w, g_app.viewer.h);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    RenderViewer();

    ++g_app.frame;
}

////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
void
KeyboardCallback(
    GLFWwindow*,
    int key, int, int action, int
) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
        return;

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            g_app.viewer.hud = !g_app.viewer.hud;
            break;
        case GLFW_KEY_C:
            if (g_app.recorder.on) {
                g_app.recorder.frame = 0;
                ++g_app.recorder.capture;
            } else {
                g_mesh.animation.time = 0.0f; // reset animation
                g_mesh.flags.animate = true;
            }
            g_app.recorder.on = !g_app.recorder.on;
            break;
        case GLFW_KEY_R:
            LoadBuffers();
            LoadPrograms();
            break;
        case GLFW_KEY_T: {
            char name[64], path[1024];
            static int frame = 0;

            glBindFramebuffer(GL_READ_FRAMEBUFFER,
                              g_gl.framebuffers[FRAMEBUFFER_BACK]);
            sprintf(name, "screenshot_%02i", frame);
            sprintf(path, "%s/%s", g_app.dir.output, name);
            djgt_save_glcolorbuffer_bmp(GL_FRONT, GL_RGB, path);
            ++frame;
        }
        default: break;
        }
    }
}

void MouseButtonCallback(GLFWwindow*, int, int, int)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;
}

void MouseMotionCallback(GLFWwindow* window, double x, double y)
{
    static double x0 = 0, y0 = 0;
    double dx = x - x0, dy = y - y0;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        g_camera.upAngle-= dx * 5e-3;
        g_camera.sideAngle-= dy * 5e-3;
        updateCameraMatrix();
    } else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        dja::mat3 axis = dja::transpose(g_camera.axis);
        g_camera.pos -= axis[0] * dx * 5e-3 * dja::norm(g_camera.pos);
        g_camera.pos += axis[1] * dy * 5e-3 * dja::norm(g_camera.pos);
    }

    x0 = x;
    y0 = y;
}

void MouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    if (io.WantCaptureMouse)
        return;

    dja::mat3 axis = dja::transpose(g_camera.axis);
    g_camera.pos -= axis[2] * yoffset * 5e-2 * norm(g_camera.pos);
}


void ResizeCallback(GLFWwindow* window, int width, int height)
{
    (void)window;

    g_framebuffer.w = width;
    g_framebuffer.h = height;
    g_app.viewer.w = width;
    g_app.viewer.h = height;

    LoadSceneFramebufferTexture();
    LoadSceneFramebuffer();
    ConfigureCageRenderProgram();
    ConfigureSubdRenderProgram();
    ConfigureUniformLodRenderProgram();
    ConfigureAdaptiveLodRenderProgram();
    ConfigureTessellationPrograms();
}


// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    // Create the Window
    LOG("Loading {Window-Main}");
    GLFWwindow* window = glfwCreateWindow(
        VIEWER_DEFAULT_WIDTH, VIEWER_DEFAULT_HEIGHT,
        "Catmull Clark Demo", NULL/*glfwGetPrimaryMonitor()*/, NULL
    );
    if (window == NULL) {
        LOG("=> Failure <=");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, &KeyboardCallback);
    glfwSetCursorPosCallback(window, &MouseMotionCallback);
    glfwSetMouseButtonCallback(window, &MouseButtonCallback);
    glfwSetScrollCallback(window, &MouseScrollCallback);
    glfwSetWindowSizeCallback(window, &ResizeCallback);

    // Load OpenGL functions
    LOG("Loading {OpenGL}");
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG("gladLoadGLLoader failed");
        return -1;
    }

    LOG("-- Begin -- Demo");
    try {
        InitDebugOutput();
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, false);
        ImGui_ImplOpenGL3_Init("#version 450");
        LOG("-- Begin -- Init");
        Load(argc, argv);
        LOG("-- End -- Init");

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            Render();
            glfwSwapBuffers(window);
        }

        Release();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
    }
    catch (std::exception& e) {
        LOG("%s", e.what());
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
        LOG("(!) Demo Killed (!)");

        return EXIT_FAILURE;
    }
    catch (...) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
        LOG("(!) Demo Killed (!)");

        return EXIT_FAILURE;
    }
    LOG("-- End -- Demo");

    return 0;
}

