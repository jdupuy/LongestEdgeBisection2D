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

#define LEB_IMPLEMENTATION
#include "leb.h"

#define LOG(fmt, ...)  fprintf(stdout, fmt, ##__VA_ARGS__); fflush(stdout);

////////////////////////////////////////////////////////////////////////////////
// Tweakable Constants
//
////////////////////////////////////////////////////////////////////////////////
#define VIEWER_DEFAULT_WIDTH  1920
#define VIEWER_DEFAULT_HEIGHT 1080

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
    VIEWER_DEFAULT_WIDTH, VIEWER_DEFAULT_HEIGHT, AA_NONE,
    {false},
    {61.0f / 255.0f, 119.0f / 255.0f, 192.0f / 255.0f}
};

// -----------------------------------------------------------------------------
// Camera Manager
#define INIT_POS dja::vec3(1000.0f, +2000.5f, 1000.0f)
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
        int triangleCount;
    } sphere;
} g_camera = {
    80.f, 1.0f, 64000.f,
    PROJECTION_RECTILINEAR,
    TONEMAP_FILMIC,
    INIT_POS,
    dja::mat3(1.0f),
    -0.8f, -0.4f,
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
// Terrain Manager
enum { METHOD_CS, METHOD_TS, METHOD_GS, METHOD_MS };
enum { SHADING_DIFFUSE, SHADING_NORMALS, SHADING_COLOR};
struct TerrainManager {
    struct { bool displace, cull, freeze, wire, topView; } flags;
    struct {
        std::string pathToFile;
        float width, height, zMin, zMax;
        float scale;
    } dmap;
    int method;
    int shading;
    int gpuSubd;
    float primitivePixelLengthTarget;
    float minLodStdev;
    int maxDepth;
    uint32_t nodeCount;
    float size;
} g_terrain = {
    {true, true, false, false, true},
    {std::string(PATH_TO_ASSET_DIRECTORY "./kauai.png"),
     52660.0f, 52660.0f, -14.0f, 1587.0f,
     1.0f},
    METHOD_CS,
    SHADING_DIFFUSE,
    3,
    7.0f,
    0.1f,
    25,
    0,
    52660.0f
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
                    PATH_TO_SRC_DIRECTORY "./terrain/shaders/",
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
    CLOCK_UPDATE,
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
enum { STREAM_TERRAIN_VARIABLES, STREAM_COUNT };
enum {
    VERTEXARRAY_EMPTY,
    VERTEXARRAY_SPHERE,
    VERTEXARRAY_MESHLET,
    VERTEXARRAY_COUNT
};
enum {
    BUFFER_LEB,
    BUFFER_TERRAIN_DRAW,
    BUFFER_TERRAIN_DRAW_MS,
    BUFFER_MESHLET_VERTICES,
    BUFFER_MESHLET_INDEXES,
    BUFFER_TERRAIN_DRAW_CS,     // compute shader path only
    BUFFER_TERRAIN_DISPATCH_CS, // compute shader path only
    BUFFER_SPHERE_VERTICES,
    BUFFER_SPHERE_INDEXES,
    BUFFER_CBT_NODE_COUNT,

    BUFFER_COUNT
};
enum {
    TEXTURE_CBUF,
    TEXTURE_ZBUF,
    TEXTURE_DMAP,
    TEXTURE_SMAP,
    TEXTURE_ATMOSPHERE_IRRADIANCE,
    TEXTURE_ATMOSPHERE_INSCATTER,
    TEXTURE_ATMOSPHERE_TRANSMITTANCE,
    TEXTURE_ROCK_DMAP,
    TEXTURE_ROCK_SMAP,

    TEXTURE_COUNT
};
enum {
    PROGRAM_VIEWER,
    PROGRAM_SPLIT,
    PROGRAM_MERGE,
    PROGRAM_RENDER_ONLY,    // compute shader path only
    PROGRAM_TOPVIEW,
    PROGRAM_LEB_REDUCTION,
    PROGRAM_LEB_REDUCTION_PREPASS,
    PROGRAM_BATCH,
    PROGRAM_SKY,
    PROGRAM_CBT_NODE_COUNT,

    PROGRAM_COUNT
};
enum {
    QUERY_NODE_COUNT,

    QUERY_COUNT
};
enum {
    UNIFORM_VIEWER_FRAMEBUFFER_SAMPLER,
    UNIFORM_VIEWER_GAMMA,

    UNIFORM_TERRAIN_DMAP_SAMPLER,
    UNIFORM_TERRAIN_SMAP_SAMPLER,
    UNIFORM_TERRAIN_DMAP_ROCK_SAMPLER,
    UNIFORM_TERRAIN_SMAP_ROCK_SAMPLER,
    UNIFORM_TERRAIN_DMAP_FACTOR,
    UNIFORM_TERRAIN_TARGET_EDGE_LENGTH,
    UNIFORM_TERRAIN_LOD_FACTOR,
    UNIFORM_TERRAIN_MIN_LOD_VARIANCE,
    UNIFORM_TERRAIN_SCREEN_RESOLUTION,
    UNIFORM_TERRAIN_INSCATTER_SAMPLER,
    UNIFORM_TERRAIN_IRRADIANCE_SAMPLER,
    UNIFORM_TERRAIN_TRANSMITTANCE_SAMPLER,

    UNIFORM_SPLIT_DMAP_SAMPLER,
    UNIFORM_SPLIT_SMAP_SAMPLER,
    UNIFORM_SPLIT_DMAP_ROCK_SAMPLER,
    UNIFORM_SPLIT_SMAP_ROCK_SAMPLER,
    UNIFORM_SPLIT_DMAP_FACTOR,
    UNIFORM_SPLIT_TARGET_EDGE_LENGTH,
    UNIFORM_SPLIT_LOD_FACTOR,
    UNIFORM_SPLIT_MIN_LOD_VARIANCE,
    UNIFORM_SPLIT_SCREEN_RESOLUTION,
    UNIFORM_SPLIT_INSCATTER_SAMPLER,
    UNIFORM_SPLIT_IRRADIANCE_SAMPLER,
    UNIFORM_SPLIT_TRANSMITTANCE_SAMPLER,

    UNIFORM_MERGE_DMAP_SAMPLER,
    UNIFORM_MERGE_SMAP_SAMPLER,
    UNIFORM_MERGE_DMAP_ROCK_SAMPLER,
    UNIFORM_MERGE_SMAP_ROCK_SAMPLER,
    UNIFORM_MERGE_DMAP_FACTOR,
    UNIFORM_MERGE_TARGET_EDGE_LENGTH,
    UNIFORM_MERGE_LOD_FACTOR,
    UNIFORM_MERGE_MIN_LOD_VARIANCE,
    UNIFORM_MERGE_SCREEN_RESOLUTION,
    UNIFORM_MERGE_INSCATTER_SAMPLER,
    UNIFORM_MERGE_IRRADIANCE_SAMPLER,
    UNIFORM_MERGE_TRANSMITTANCE_SAMPLER,

    UNIFORM_RENDER_DMAP_SAMPLER,
    UNIFORM_RENDER_SMAP_SAMPLER,
    UNIFORM_RENDER_DMAP_ROCK_SAMPLER,
    UNIFORM_RENDER_SMAP_ROCK_SAMPLER,
    UNIFORM_RENDER_DMAP_FACTOR,
    UNIFORM_RENDER_TARGET_EDGE_LENGTH,
    UNIFORM_RENDER_LOD_FACTOR,
    UNIFORM_RENDER_MIN_LOD_VARIANCE,
    UNIFORM_RENDER_SCREEN_RESOLUTION,
    UNIFORM_RENDER_INSCATTER_SAMPLER,
    UNIFORM_RENDER_IRRADIANCE_SAMPLER,
    UNIFORM_RENDER_TRANSMITTANCE_SAMPLER,

    UNIFORM_TOPVIEW_DMAP_SAMPLER,
    UNIFORM_TOPVIEW_DMAP_FACTOR,

    UNIFORM_SKY_SUN_DIR,
    UNIFORM_SKY_CAMERA_MATRIX,
    UNIFORM_SKY_VIEW_PROJECTION_MATRIX,
    UNIFORM_SKY_INSCATTER_SAMPLER,
    UNIFORM_SKY_IRRADIANCE_SAMPLER,
    UNIFORM_SKY_TRANSMITTANCE_SAMPLER,

    UNIFORM_COUNT
};
struct OpenGLManager {
    GLuint programs[PROGRAM_COUNT];
    GLuint framebuffers[FRAMEBUFFER_COUNT];
    GLuint textures[TEXTURE_COUNT];
    GLuint vertexArrays[VERTEXARRAY_COUNT];
    GLuint buffers[BUFFER_COUNT];
    GLuint queries[QUERY_COUNT];
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

float radians(float degrees)
{
    return degrees * M_PI / 180.f;
}

char *strcat2(char *dst, const char *src1, const char *src2)
{
    strcpy(dst, src1);

    return strcat(dst, src2);
}

float sqr(float x)
{
    return x * x;
}

static void APIENTRY
debug_output_logger(
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

void log_debug_output(void)
{
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&debug_output_logger, NULL);
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
        TEXTURE_CBUF);
    glProgramUniform1f(g_gl.programs[PROGRAM_VIEWER],
        g_gl.uniforms[UNIFORM_VIEWER_GAMMA],
        g_app.viewer.gamma);
}

// -----------------------------------------------------------------------------
// set Terrain program uniforms
float computeLodFactor()
{
    if (g_camera.projection == PROJECTION_RECTILINEAR) {
        float tmp = 2.0f * tan(radians(g_camera.fovy) / 2.0f)
            / g_framebuffer.h * (1 << g_terrain.gpuSubd)
            * g_terrain.primitivePixelLengthTarget;

        return -2.0f * std::log2(tmp) + 2.0f;
    } else if (g_camera.projection == PROJECTION_ORTHOGRAPHIC) {
        float planeSize = 2.0f * tan(radians(g_camera.fovy / 2.0f));
        float targetSize = planeSize * g_terrain.primitivePixelLengthTarget
                         / g_framebuffer.h * (1 << g_terrain.gpuSubd);

        return -2.0f * std::log2(targetSize);
    } else if (g_camera.projection == PROJECTION_FISHEYE) {
        float tmp = 2.0f * tan(radians(g_camera.fovy) / 2.0f)
            / g_framebuffer.h * (1 << g_terrain.gpuSubd)
            * g_terrain.primitivePixelLengthTarget;

        return -2.0f * std::log2(tmp) + 2.0f;
    }

    return 1.0f;
}

void ConfigureTerrainProgram(GLuint glp, GLuint offset)
{
    float lodFactor = computeLodFactor();

    glProgramUniform1f(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_DMAP_FACTOR + offset],
        g_terrain.dmap.scale);
    glProgramUniform1f(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_LOD_FACTOR + offset],
        lodFactor);
    glProgramUniform1i(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_DMAP_SAMPLER + offset],
        TEXTURE_DMAP);
    glProgramUniform1i(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_SMAP_SAMPLER + offset],
        TEXTURE_SMAP);
    glProgramUniform1i(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_DMAP_ROCK_SAMPLER + offset],
        TEXTURE_ROCK_DMAP);
    glProgramUniform1i(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_SMAP_ROCK_SAMPLER + offset],
        TEXTURE_ROCK_SMAP);
    glProgramUniform1f(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_TARGET_EDGE_LENGTH + offset],
        g_terrain.primitivePixelLengthTarget);
    glProgramUniform1f(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_MIN_LOD_VARIANCE + offset],
        sqr(g_terrain.minLodStdev / 64.0f / g_terrain.dmap.scale));
    glProgramUniform2f(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_SCREEN_RESOLUTION + offset],
        g_framebuffer.w, g_framebuffer.h);
    glProgramUniform1i(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_INSCATTER_SAMPLER + offset],
        TEXTURE_ATMOSPHERE_INSCATTER);
    glProgramUniform1i(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_IRRADIANCE_SAMPLER+ offset],
        TEXTURE_ATMOSPHERE_IRRADIANCE);
    glProgramUniform1i(glp,
        g_gl.uniforms[UNIFORM_TERRAIN_TRANSMITTANCE_SAMPLER + offset],
        TEXTURE_ATMOSPHERE_TRANSMITTANCE);
}

void ConfigureTerrainPrograms()
{
    ConfigureTerrainProgram(g_gl.programs[PROGRAM_SPLIT],
                            UNIFORM_SPLIT_DMAP_SAMPLER - UNIFORM_TERRAIN_DMAP_SAMPLER);
    ConfigureTerrainProgram(g_gl.programs[PROGRAM_MERGE],
                            UNIFORM_MERGE_DMAP_SAMPLER - UNIFORM_TERRAIN_DMAP_SAMPLER);
    ConfigureTerrainProgram(g_gl.programs[PROGRAM_RENDER_ONLY],
                            UNIFORM_RENDER_DMAP_SAMPLER - UNIFORM_TERRAIN_DMAP_SAMPLER);
}

// -----------------------------------------------------------------------------
// set Terrain program uniforms
void ConfigureTopViewProgram()
{
    glProgramUniform1f(g_gl.programs[PROGRAM_TOPVIEW],
        g_gl.uniforms[UNIFORM_TOPVIEW_DMAP_FACTOR],
        g_terrain.dmap.scale);
    glProgramUniform1i(g_gl.programs[PROGRAM_TOPVIEW],
        g_gl.uniforms[UNIFORM_TOPVIEW_DMAP_SAMPLER],
        TEXTURE_DMAP);
}

// -----------------------------------------------------------------------------
// set Atmosphere program uniforms
void ConfigureSkyProgram()
{
    glProgramUniform1i(g_gl.programs[PROGRAM_SKY],
                       g_gl.uniforms[UNIFORM_SKY_INSCATTER_SAMPLER],
                       TEXTURE_ATMOSPHERE_INSCATTER);
    glProgramUniform1i(g_gl.programs[PROGRAM_SKY],
                       g_gl.uniforms[UNIFORM_SKY_IRRADIANCE_SAMPLER],
                       TEXTURE_ATMOSPHERE_IRRADIANCE);
    glProgramUniform1i(g_gl.programs[PROGRAM_SKY],
                       g_gl.uniforms[UNIFORM_SKY_TRANSMITTANCE_SAMPLER],
                       TEXTURE_ATMOSPHERE_TRANSMITTANCE);
}

///////////////////////////////////////////////////////////////////////////////
// Program Loading
//
////////////////////////////////////////////////////////////////////////////////

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
    char buf[1024];

    LOG("Loading {Viewer-Program}\n");
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
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/ToneMapping.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/Viewer.glsl");

    if (!djgp_to_gl(djp, 450, false, true, program)) {
        LOG("=> Failure <=\n");
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    g_gl.uniforms[UNIFORM_VIEWER_FRAMEBUFFER_SAMPLER] =
        glGetUniformLocation(g_gl.programs[PROGRAM_VIEWER], "u_FramebufferSampler");
    g_gl.uniforms[UNIFORM_VIEWER_GAMMA] =
        glGetUniformLocation(g_gl.programs[PROGRAM_VIEWER], "u_Gamma");

    ConfigureViewerProgram();

    return (glGetError() == GL_NO_ERROR);
}

// -----------------------------------------------------------------------------
/**
 * Load the Terrain Rendering Program
 *
 * This program is responsible for updating and rendering the terrain.
 */
bool LoadTerrainProgram(GLuint *glp, const char *flag, GLuint uniformOffset)
{
    djg_program *djp = djgp_create();

    LOG("Loading {Terrain-Program}\n");
    if (!g_terrain.flags.freeze)
        djgp_push_string(djp, flag);
    if (g_terrain.method == METHOD_MS) {
        djgp_push_string(djp, "#ifndef FRAGMENT_SHADER\n#extension GL_NV_mesh_shader : require\n#endif\n");
        djgp_push_string(djp, "#extension GL_NV_shader_thread_group : require\n");
        djgp_push_string(djp, "#extension GL_NV_shader_thread_shuffle : require\n");
        djgp_push_string(djp, "#extension GL_NV_gpu_shader5 : require\n");
    }
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
    djgp_push_string(djp, "#define BUFFER_BINDING_TERRAIN_VARIABLES %i\n", STREAM_TERRAIN_VARIABLES);
    djgp_push_string(djp, "#define BUFFER_BINDING_MESHLET_VERTICES %i\n", BUFFER_MESHLET_VERTICES);
    djgp_push_string(djp, "#define BUFFER_BINDING_MESHLET_INDEXES %i\n", BUFFER_MESHLET_INDEXES);
    djgp_push_string(djp, "#define TERRAIN_PATCH_SUBD_LEVEL %i\n", g_terrain.gpuSubd);
    djgp_push_string(djp, "#define TERRAIN_PATCH_TESS_FACTOR %i\n", 1 << g_terrain.gpuSubd);
    if (g_terrain.shading == SHADING_DIFFUSE)
        djgp_push_string(djp, "#define SHADING_DIFFUSE 1\n");
    else if (g_terrain.shading == SHADING_NORMALS)
        djgp_push_string(djp, "#define SHADING_NORMALS 1\n");
    else if (g_terrain.shading == SHADING_COLOR)
        djgp_push_string(djp, "#define SHADING_COLOR 1\n");
    if (g_terrain.flags.displace)
        djgp_push_string(djp, "#define FLAG_DISPLACE 1\n");
    if (g_terrain.flags.cull)
        djgp_push_string(djp, "#define FLAG_CULL 1\n");
    if (g_terrain.flags.wire)
        djgp_push_string(djp, "#define FLAG_WIRE 1\n");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/FrustumCulling.glsl");
    djgp_push_string(djp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_LEB);
    djgp_push_string(djp, "#define CBT_READ_ONLY\n");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libleb/glsl/leb.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/BrunetonAtmosphere.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainRenderCommon.glsl");
    if (g_terrain.method == METHOD_CS) {
        if (strcmp("/* thisIsAHackForComputePass */\n", flag) == 0) {
            if (g_terrain.flags.wire) {
                djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainRenderCS_Wire.glsl");
            } else {
                djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainRenderCS.glsl");
            }
        } else {
            djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainUpdateCS.glsl");
        }
    } else if (g_terrain.method == METHOD_TS) {
        if (g_terrain.flags.wire) {
            djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainRenderTS_Wire.glsl");
        } else {
            djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainRenderTS.glsl");
        }
    } else if (g_terrain.method == METHOD_GS) {
        int subdLevel = g_terrain.gpuSubd;

        if (g_terrain.flags.wire) {
            int vertexCnt = 3 << (2 * subdLevel);

            djgp_push_string(djp, "#define MAX_VERTICES %i\n", vertexCnt);
            djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainRenderGS_Wire.glsl");
        } else {
            int vertexCnt = subdLevel == 0 ? 3 : 4 << (2 * subdLevel - 1);

            djgp_push_string(djp, "#define MAX_VERTICES %i\n", vertexCnt);
            djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainRenderGS.glsl");
        }
    } else if (g_terrain.method == METHOD_MS) {
        djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainRenderMS.glsl");
    }

    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    g_gl.uniforms[UNIFORM_TERRAIN_DMAP_FACTOR + uniformOffset] =
        glGetUniformLocation(*glp, "u_DmapFactor");
    g_gl.uniforms[UNIFORM_TERRAIN_LOD_FACTOR + uniformOffset] =
        glGetUniformLocation(*glp, "u_LodFactor");
    g_gl.uniforms[UNIFORM_TERRAIN_DMAP_SAMPLER + uniformOffset] =
        glGetUniformLocation(*glp, "u_DmapSampler");
    g_gl.uniforms[UNIFORM_TERRAIN_SMAP_SAMPLER + uniformOffset] =
        glGetUniformLocation(*glp, "u_SmapSampler");
    g_gl.uniforms[UNIFORM_TERRAIN_DMAP_ROCK_SAMPLER + uniformOffset] =
        glGetUniformLocation(*glp, "u_DmapRockSampler");
    g_gl.uniforms[UNIFORM_TERRAIN_SMAP_ROCK_SAMPLER + uniformOffset] =
        glGetUniformLocation(*glp, "u_SmapRockSampler");
    g_gl.uniforms[UNIFORM_TERRAIN_TARGET_EDGE_LENGTH + uniformOffset] =
        glGetUniformLocation(*glp, "u_TargetEdgeLength");
    g_gl.uniforms[UNIFORM_TERRAIN_MIN_LOD_VARIANCE + uniformOffset] =
        glGetUniformLocation(*glp, "u_MinLodVariance");
    g_gl.uniforms[UNIFORM_TERRAIN_SCREEN_RESOLUTION + uniformOffset] =
        glGetUniformLocation(*glp, "u_ScreenResolution");
    g_gl.uniforms[UNIFORM_TERRAIN_IRRADIANCE_SAMPLER + uniformOffset] =
        glGetUniformLocation(*glp, "skyIrradianceSampler");
    g_gl.uniforms[UNIFORM_TERRAIN_INSCATTER_SAMPLER + uniformOffset] =
        glGetUniformLocation(*glp, "inscatterSampler");
    g_gl.uniforms[UNIFORM_TERRAIN_TRANSMITTANCE_SAMPLER + uniformOffset] =
        glGetUniformLocation(*glp, "transmittanceSampler");

    ConfigureTerrainProgram(*glp, uniformOffset);

    return (glGetError() == GL_NO_ERROR);
}

bool LoadTerrainPrograms()
{
    bool v = true;

    if (v) v = v && LoadTerrainProgram(&g_gl.programs[PROGRAM_SPLIT],
                                       "#define FLAG_SPLIT 1\n",
                                       UNIFORM_SPLIT_DMAP_FACTOR - UNIFORM_TERRAIN_DMAP_FACTOR);
    if (v) v = v && LoadTerrainProgram(&g_gl.programs[PROGRAM_MERGE],
                                       "#define FLAG_MERGE 1\n",
                                       UNIFORM_MERGE_DMAP_FACTOR - UNIFORM_TERRAIN_DMAP_FACTOR);
    if (v) v = v && LoadTerrainProgram(&g_gl.programs[PROGRAM_RENDER_ONLY],
                                       "/* thisIsAHackForComputePass */\n",
                                       UNIFORM_RENDER_DMAP_FACTOR - UNIFORM_TERRAIN_DMAP_FACTOR);

    return v;
}

// -----------------------------------------------------------------------------
/**
 * Load the Reduction Program
 *
 * This program is responsible for precomputing a reduction for the
 * subdivision tree. This allows to locate the i-th bit in a bitfield of
 * size N in log(N) operations.
 */
bool LoadLebReductionProgram()
{
    djg_program *djp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_LEB_REDUCTION];

    LOG("Loading {Reduction-Program}\n");
    djgp_push_string(djp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_LEB);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt_SumReduction.glsl");
    djgp_push_string(djp, "#ifdef COMPUTE_SHADER\n#endif");

    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    return (glGetError() == GL_NO_ERROR);
}

bool LoadLebReductionPrepassProgram()
{
    djg_program *djp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_LEB_REDUCTION_PREPASS];

    LOG("Loading {Reduction-Prepass-Program}\n");
    djgp_push_string(djp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_LEB);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt_SumReductionPrepass.glsl");
    djgp_push_string(djp, "#ifdef COMPUTE_SHADER\n#endif");
    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    return (glGetError() == GL_NO_ERROR);
}

// -----------------------------------------------------------------------------
/**
 * Load the Batch Program
 *
 * This program is responsible for preparing an indirect draw call
 */
bool LoadBatchProgram()
{
    djg_program *djp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_BATCH];

    LOG("Loading {Batch-Program}\n");
    if (GLAD_GL_ARB_shader_atomic_counter_ops) {
        djgp_push_string(djp, "#extension GL_ARB_shader_atomic_counter_ops : require\n");
        djgp_push_string(djp, "#define ATOMIC_COUNTER_EXCHANGE_ARB 1\n");
    } else if (GLAD_GL_AMD_shader_atomic_counter_ops) {
        djgp_push_string(djp, "#extension GL_AMD_shader_atomic_counter_ops : require\n");
        djgp_push_string(djp, "#define ATOMIC_COUNTER_EXCHANGE_AMD 1\n");
    }
    if (g_terrain.method == METHOD_MS) {
        djgp_push_string(djp, "#define FLAG_MS 1\n");
        djgp_push_string(djp, "#define BUFFER_BINDING_DRAW_MESH_TASKS_INDIRECT_COMMAND %i\n", BUFFER_TERRAIN_DRAW_MS);
    }
    if (g_terrain.method == METHOD_CS) {
        djgp_push_string(djp, "#define FLAG_CS 1\n");
        djgp_push_string(djp, "#define BUFFER_BINDING_DRAW_ELEMENTS_INDIRECT_COMMAND %i\n", BUFFER_TERRAIN_DRAW_CS);
        djgp_push_string(djp, "#define BUFFER_BINDING_DISPATCH_INDIRECT_COMMAND %i\n", BUFFER_TERRAIN_DISPATCH_CS);
        djgp_push_string(djp, "#define MESHLET_INDEX_COUNT %i\n", 3 << (2 * g_terrain.gpuSubd));
    }
    djgp_push_string(djp, "#define LEB_BUFFER_COUNT 1\n");
    djgp_push_string(djp, "#define BUFFER_BINDING_LEB %i\n", BUFFER_LEB);
    djgp_push_string(djp, "#define BUFFER_BINDING_DRAW_ARRAYS_INDIRECT_COMMAND %i\n", BUFFER_TERRAIN_DRAW);
#if 0
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/LongestEdgeBisection.glsl");
#else
    djgp_push_string(djp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_LEB);
    djgp_push_string(djp, "#define CBT_READ_ONLY\n");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt.glsl");
#endif
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainBatcher.glsl");
    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    return (glGetError() == GL_NO_ERROR);
}

// -----------------------------------------------------------------------------
/**
 * Load the Top View Program
 *
 * This program is responsible for rendering the terrain in a top view fashion
 */
bool LoadTopViewProgram()
{
    djg_program *djp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_TOPVIEW];

    LOG("Loading {Top-View-Program}\n");
    if (g_terrain.flags.displace)
        djgp_push_string(djp, "#define FLAG_DISPLACE 1\n");
    djgp_push_string(djp, "#define TERRAIN_PATCH_SUBD_LEVEL %i\n", g_terrain.gpuSubd);
    djgp_push_string(djp, "#define TERRAIN_PATCH_TESS_FACTOR %i\n", 1 << g_terrain.gpuSubd);
    djgp_push_string(djp, "#define BUFFER_BINDING_TERRAIN_VARIABLES %i\n", STREAM_TERRAIN_VARIABLES);
    djgp_push_string(djp, "#define LEB_BUFFER_COUNT 1\n");
    djgp_push_string(djp, "#define BUFFER_BINDING_LEB %i\n", BUFFER_LEB);
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/FrustumCulling.glsl");
    djgp_push_string(djp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_LEB);
    djgp_push_string(djp, "#define CBT_READ_ONLY\n");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libleb/glsl/leb.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainRenderCommon.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/TerrainTopView.glsl");
    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    g_gl.uniforms[UNIFORM_TOPVIEW_DMAP_FACTOR] =
        glGetUniformLocation(*glp, "u_DmapFactor");
    g_gl.uniforms[UNIFORM_TOPVIEW_DMAP_SAMPLER] =
        glGetUniformLocation(*glp, "u_DmapSampler");

    ConfigureTopViewProgram();

    return (glGetError() == GL_NO_ERROR);
}

// -----------------------------------------------------------------------------
/**
 * Load the Sky Programe
 *
 * This program is responsible for rendering Bruneton's atmosphere
 */
bool LoadSkyProgram()
{
    djg_program *djp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_SKY];

    LOG("Loading {Sky-Program}\n");
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
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/BrunetonAtmosphere.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/Sky.glsl");
    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    g_gl.uniforms[UNIFORM_SKY_CAMERA_MATRIX] =
        glGetUniformLocation(*glp, "u_CameraMatrix");
    g_gl.uniforms[UNIFORM_SKY_VIEW_PROJECTION_MATRIX] =
        glGetUniformLocation(*glp, "u_ViewProjectionMatrix");
    g_gl.uniforms[UNIFORM_SKY_SUN_DIR] =
        glGetUniformLocation(*glp, "u_SunDir");
    g_gl.uniforms[UNIFORM_SKY_IRRADIANCE_SAMPLER] =
        glGetUniformLocation(*glp, "skyIrradianceSampler");
    g_gl.uniforms[UNIFORM_SKY_INSCATTER_SAMPLER] =
        glGetUniformLocation(*glp, "inscatterSampler");
    g_gl.uniforms[UNIFORM_SKY_TRANSMITTANCE_SAMPLER] =
        glGetUniformLocation(*glp, "transmittanceSampler");
    ConfigureSkyProgram();

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------
/**
 * Load the Node Count Program
 *
 * This program is responsible for retrieving the number of nodes in the CBT
 */
bool LoadCbtNodeCountProgram()
{
    djg_program *djp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_CBT_NODE_COUNT];

    LOG("Loading {Cbt-Node-Count-Program}\n");
    djgp_push_string(djp, "#define CBT_NODE_COUNT_BUFFER_BINDING %i\n", BUFFER_CBT_NODE_COUNT);
    djgp_push_string(djp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_LEB);
    djgp_push_string(djp, "#define CBT_READ_ONLY\n");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./submodules/libcbt/glsl/cbt.glsl");
    djgp_push_file(djp, PATH_TO_SRC_DIRECTORY "./terrain/shaders/NodeCount.glsl");
    djgp_push_string(djp, "#ifdef COMPUTE_SHADER\n#endif\n");
    if (!djgp_to_gl(djp, 450, false, true, glp)) {
        djgp_release(djp);

        return false;
    }
    djgp_release(djp);

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------
/**
 * Load All Programs
 *
 */
bool LoadPrograms()
{
    bool v = true;

    if (v) v &= LoadViewerProgram();
    if (v) v &= LoadTerrainPrograms();
    if (v) v &= LoadLebReductionProgram();
    if (v) v &= LoadLebReductionPrepassProgram();
    if (v) v &= LoadBatchProgram();
    if (v) v &= LoadTopViewProgram();
    if (v) v &= LoadSkyProgram();
    if (v) v &= LoadCbtNodeCountProgram();

    return v;
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
    if (glIsTexture(g_gl.textures[TEXTURE_CBUF]))
        glDeleteTextures(1, &g_gl.textures[TEXTURE_CBUF]);
    if (glIsTexture(g_gl.textures[TEXTURE_ZBUF]))
        glDeleteTextures(1, &g_gl.textures[TEXTURE_ZBUF]);
    glGenTextures(1, &g_gl.textures[TEXTURE_ZBUF]);
    glGenTextures(1, &g_gl.textures[TEXTURE_CBUF]);

    switch (g_framebuffer.aa) {
    case AA_NONE:
        LOG("Loading {Z-Buffer-Framebuffer-Texture}\n");
        glActiveTexture(GL_TEXTURE0 + TEXTURE_ZBUF);
        glBindTexture(GL_TEXTURE_2D, g_gl.textures[TEXTURE_ZBUF]);
        glTexStorage2D(GL_TEXTURE_2D,
            1,
            GL_DEPTH24_STENCIL8,
            g_framebuffer.w,
            g_framebuffer.h);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        LOG("Loading {Color-Buffer-Framebuffer-Texture}\n");
        glActiveTexture(GL_TEXTURE0 + TEXTURE_CBUF);
        glBindTexture(GL_TEXTURE_2D, g_gl.textures[TEXTURE_CBUF]);
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
        //glGetIntegerv(GL_MAX_INTEGER_SAMPLES, &maxSamples); //Wrong enum !
        glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &maxSamples);
        glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &maxSamplesDepth);
        maxSamples = maxSamplesDepth < maxSamples ? maxSamplesDepth : maxSamples;

        if (samples > maxSamples) {
            LOG("note: MSAA is %ix\n", maxSamples);
            samples = maxSamples;
        }
        LOG("Loading {Scene-MSAA-Z-Framebuffer-Texture}\n");
        glActiveTexture(GL_TEXTURE0 + TEXTURE_ZBUF);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, g_gl.textures[TEXTURE_ZBUF]);
        glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,
                                  samples,
                                  GL_DEPTH24_STENCIL8,
                                  g_framebuffer.w,
                                  g_framebuffer.h,
                                  g_framebuffer.msaa.fixed);

        LOG("Loading {Scene-MSAA-RGBA-Framebuffer-Texture}\n");
        glActiveTexture(GL_TEXTURE0 + TEXTURE_CBUF);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, g_gl.textures[TEXTURE_CBUF]);
        glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,
                                  samples,
                                  GL_RGBA32F,
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
 * Load the Normal Texture Map
 *
 * This Loads an RG32F texture used as a slope map
 */
void LoadNmapTexture16(int smapID, const djg_texture *dmap)
{
    int w = dmap->next->x;
    int h = dmap->next->y;
    const uint16_t *texels = (const uint16_t *)dmap->next->texels;
    int mipcnt = djgt__mipcnt(w, h, 1);

    std::vector<float> smap(w * h * 2);

    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            int i1 = std::max(0, i - 1);
            int i2 = std::min(w - 1, i + 1);
            int j1 = std::max(0, j - 1);
            int j2 = std::min(h - 1, j + 1);
            uint16_t px_l = texels[i1 + w * j]; // in [0,2^16-1]
            uint16_t px_r = texels[i2 + w * j]; // in [0,2^16-1]
            uint16_t px_b = texels[i + w * j1]; // in [0,2^16-1]
            uint16_t px_t = texels[i + w * j2]; // in [0,2^16-1]
            float z_l = (float)px_l / 65535.0f; // in [0, 1]
            float z_r = (float)px_r / 65535.0f; // in [0, 1]
            float z_b = (float)px_b / 65535.0f; // in [0, 1]
            float z_t = (float)px_t / 65535.0f; // in [0, 1]
            float slope_x = (float)w * 0.5f * (z_r - z_l);
            float slope_y = (float)h * 0.5f * (z_t - z_b);

            smap[    2 * (i + w * j)] = slope_x;
            smap[1 + 2 * (i + w * j)] = slope_y;
        }

    if (glIsTexture(g_gl.textures[smapID]))
        glDeleteTextures(1, &g_gl.textures[smapID]);

    glGenTextures(1, &g_gl.textures[smapID]);
    glActiveTexture(GL_TEXTURE0 + smapID);
    glBindTexture(GL_TEXTURE_2D, g_gl.textures[smapID]);
    glTexStorage2D(GL_TEXTURE_2D, mipcnt, GL_RG32F, w, h);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RG, GL_FLOAT, &smap[0]);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0);
}

void LoadNmapTexture8(int smapID, const djg_texture *dmap)
{
    int w = dmap->next->x;
    int h = dmap->next->y;
    const uint8_t *texels = (const uint8_t *)dmap->next->texels;
    int mipcnt = djgt__mipcnt(w, h, 1);

    std::vector<float> smap(w * h * 2);

    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            int i1 = std::max(0, i - 1);
            int i2 = std::min(w - 1, i + 1);
            int j1 = std::max(0, j - 1);
            int j2 = std::min(h - 1, j + 1);
            uint8_t px_l = texels[i1 + w * j]; // in [0,2^16-1]
            uint8_t px_r = texels[i2 + w * j]; // in [0,2^16-1]
            uint8_t px_b = texels[i + w * j1]; // in [0,2^16-1]
            uint8_t px_t = texels[i + w * j2]; // in [0,2^16-1]
            float z_l = (float)px_l / 255.0f; // in [0, 1]
            float z_r = (float)px_r / 255.0f; // in [0, 1]
            float z_b = (float)px_b / 255.0f; // in [0, 1]
            float z_t = (float)px_t / 255.0f; // in [0, 1]
            float slope_x = (float)w * 0.5f * (z_r - z_l);
            float slope_y = (float)h * 0.5f * (z_t - z_b);

            smap[    2 * (i + w * j)] = slope_x;
            smap[1 + 2 * (i + w * j)] = slope_y;
        }

    if (glIsTexture(g_gl.textures[smapID]))
        glDeleteTextures(1, &g_gl.textures[smapID]);

    glGenTextures(1, &g_gl.textures[smapID]);
    glActiveTexture(GL_TEXTURE0 + smapID);
    glBindTexture(GL_TEXTURE_2D, g_gl.textures[smapID]);
    glTexStorage2D(GL_TEXTURE_2D, mipcnt, GL_RG32F, w, h);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RG, GL_FLOAT, &smap[0]);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_REPEAT);
    glActiveTexture(GL_TEXTURE0);
}

// -----------------------------------------------------------------------------
/**
 * Load the Displacement Texture
 *
 * This Loads an R16 texture used as a displacement map
 */
bool LoadDmapTexture16(int dmapID, int smapID, const char *pathToFile)
{
    djg_texture *djgt = djgt_create(1);

    LOG("Loading {Dmap-Texture}\n");
    djgt_push_image_u16(djgt, pathToFile, 1);

    int w = djgt->next->x;
    int h = djgt->next->y;
    const uint16_t *texels = (const uint16_t *)djgt->next->texels;
    int mipcnt = djgt__mipcnt(w, h, 1);
    std::vector<uint16_t> dmap(w * h * 2);

    for (int j = 0; j < h; ++j)
    for (int i = 0; i < w; ++i) {
        uint16_t z = texels[i + w * j]; // in [0,2^16-1]
        float zf = float(z) / float((1 << 16) - 1);
        uint16_t z2 = zf * zf * ((1 << 16) - 1);

        dmap[    2 * (i + w * j)] = z;
        dmap[1 + 2 * (i + w * j)] = z2;
    }

    // Load nmap from dmap
    LoadNmapTexture16(smapID, djgt);

    glActiveTexture(GL_TEXTURE0 + dmapID);
    if (glIsTexture(g_gl.textures[dmapID]))
        glDeleteTextures(1, &g_gl.textures[dmapID]);

    glGenTextures(1, &g_gl.textures[dmapID]);
    glActiveTexture(GL_TEXTURE0 + dmapID);
    glBindTexture(GL_TEXTURE_2D, g_gl.textures[dmapID]);
    glTexStorage2D(GL_TEXTURE_2D, mipcnt, GL_RG16, w, h);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RG, GL_UNSIGNED_SHORT, &dmap[0]);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MAG_FILTER,
                    GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_WRAP_S,
                    GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_WRAP_T,
                    GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0);
    djgt_release(djgt);

    return (glGetError() == GL_NO_ERROR);
}

bool LoadDmapTexture()
{
    LOG("%s", g_terrain.dmap.pathToFile.c_str());
    if (!g_terrain.dmap.pathToFile.empty()) {
        return LoadDmapTexture16(TEXTURE_DMAP,
                                 TEXTURE_SMAP,
                                 g_terrain.dmap.pathToFile.c_str());
    }

    return (glGetError() == GL_NO_ERROR);
}

// -----------------------------------------------------------------------------
bool LoadBrunetonAtmosphereTextures()
{
    float *data = new float[16*64*3];
    FILE *f = fopen(PATH_TO_ASSET_DIRECTORY "irradiance.raw", "rb");
    fread(data, 1, 16*64*3*sizeof(float), f);
    fclose(f);
    glActiveTexture(GL_TEXTURE0 + TEXTURE_ATMOSPHERE_IRRADIANCE);
    glBindTexture(GL_TEXTURE_2D, g_gl.textures[TEXTURE_ATMOSPHERE_IRRADIANCE]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 64, 16, 0, GL_RGB, GL_FLOAT, data);
    delete[] data;

    int res = 64;
    int nr = res / 2;
    int nv = res * 2;
    int nb = res / 2;
    int na = 8;
    f = fopen(PATH_TO_ASSET_DIRECTORY "inscatter.raw", "rb");
    data = new float[nr*nv*nb*na*4];
    fread(data, 1, nr*nv*nb*na*4*sizeof(float), f);
    fclose(f);
    glActiveTexture(GL_TEXTURE0 + TEXTURE_ATMOSPHERE_INSCATTER);
    glBindTexture(GL_TEXTURE_3D, g_gl.textures[TEXTURE_ATMOSPHERE_INSCATTER]);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, na*nb, nv, nr, 0, GL_RGBA, GL_FLOAT, data);
    delete[] data;

    data = new float[256*64*3];
    f = fopen(PATH_TO_ASSET_DIRECTORY "transmittance.raw", "rb");
    fread(data, 1, 256*64*3*sizeof(float), f);
    fclose(f);
    glActiveTexture(GL_TEXTURE0 + TEXTURE_ATMOSPHERE_TRANSMITTANCE);
    glBindTexture(GL_TEXTURE_2D, g_gl.textures[TEXTURE_ATMOSPHERE_TRANSMITTANCE]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 256, 64, 0, GL_RGB, GL_FLOAT, data);
    delete[] data;

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
    if (v) v &= LoadDmapTexture();
    if (v) v &= LoadBrunetonAtmosphereTextures();

    return v;
}

////////////////////////////////////////////////////////////////////////////////
// Buffer Loading
//
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
/**
 * Load Terrain Variables UBO
 *
 * This procedure updates the transformation matrices; it is updated each frame.
 */
bool LoadTerrainVariables()
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
        g_gl.streams[STREAM_TERRAIN_VARIABLES] = djgb_create(sizeof(variables));
        first = false;
    }

    // extract view and projection matrices
    dja::mat4 projection;
    if (g_camera.projection == PROJECTION_ORTHOGRAPHIC) {
        float ratio = (float)g_framebuffer.w / (float)g_framebuffer.h;
        float planeSize = tan(radians(g_camera.fovy / 2.0f));

        projection = dja::mat4::homogeneous::orthographic(
            -planeSize * ratio, planeSize * ratio, -planeSize, planeSize,
            g_camera.zNear, g_camera.zFar
        );
    } else if (g_camera.projection == PROJECTION_RECTILINEAR) {
        projection = dja::mat4::homogeneous::perspective(
            radians(g_camera.fovy),
            (float)g_framebuffer.w / (float)g_framebuffer.h,
            g_camera.zNear, g_camera.zFar
        );
    } else {
#if 0
        float ratio = (float)g_framebuffer.w / (float)g_framebuffer.h;
        float planeSize = tan(radians(g_camera.fovy / 2.0f));

        projection = dja::mat4::homogeneous::orthographic(
            -planeSize * ratio, planeSize * ratio, -planeSize, planeSize,
            g_camera.zNear, g_camera.zFar
        );
#else
        projection = dja::mat4::homogeneous::perspective(
            radians(g_camera.fovy),
            (float)g_framebuffer.w / (float)g_framebuffer.h,
            g_camera.zNear, g_camera.zFar
        );
#endif
    }

    float width = g_terrain.dmap.width;
    float height = g_terrain.dmap.height;
    float zMin = g_terrain.dmap.zMin;
    float zMax = g_terrain.dmap.zMax;
    dja::vec3 scale = dja::vec3(width, zMax - zMin, height);
    dja::mat4 viewInv = dja::mat4::homogeneous::translation(g_camera.pos)
        * dja::mat4::homogeneous::from_mat3(g_camera.axis);
    dja::mat4 view = dja::inverse(viewInv);
    dja::mat4 model = dja::mat4::homogeneous::translation(dja::vec3(-width / 2.0f, zMin, +height / 2.0f))
            * dja::mat4::homogeneous::scale(dja::vec3(scale))
            * dja::mat4::homogeneous::rotation(dja::vec3(1, 0, 0), M_PI / 2.0f);

    // set transformations (column-major)
    variables.model = dja::transpose(model);
    //variables.normal = dja::transpose(dja::inverse(variables.model));
    variables.view = dja::transpose(view);
    variables.camera = dja::transpose(viewInv);
    variables.modelView = dja::transpose(view * model);
    variables.modelViewProjection = dja::transpose(projection * view * model);
    variables.viewProjection = dja::transpose(projection * view);
    //variables.projection = dja::transpose(projection);

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
    djgb_to_gl(g_gl.streams[STREAM_TERRAIN_VARIABLES], (const void *)&variables, NULL);
    djgb_glbindrange(g_gl.streams[STREAM_TERRAIN_VARIABLES],
                     GL_UNIFORM_BUFFER,
                     STREAM_TERRAIN_VARIABLES);

    return (glGetError() == GL_NO_ERROR);
}

// -----------------------------------------------------------------------------
/**
 * Load LEB Buffer
 *
 * This procedure initializes the subdivision buffer.
 */
bool LoadLebBuffer()
{
    cbt_Tree *cbt = cbt_CreateAtDepth(g_terrain.maxDepth, 1);

    LOG("Loading {Subd-Buffer}\n");
    if (glIsBuffer(g_gl.buffers[BUFFER_LEB]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_LEB]);
    glGenBuffers(1, &g_gl.buffers[BUFFER_LEB]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_gl.buffers[BUFFER_LEB]);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 cbt_HeapByteSize(cbt),
                 cbt_GetHeap(cbt),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_LEB,
                     g_gl.buffers[BUFFER_LEB]);

    cbt_Release(cbt);

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------
/**
 * Load CBT Node Count Buffer
 *
 * This procedure initializes a buffer that stores the number of nodes in the CBT.
 */
bool LoadCbtNodeCountBuffer()
{
    LOG("Loading {Cbt-Node-Count-Buffer}\n");
    if (glIsBuffer(g_gl.buffers[BUFFER_CBT_NODE_COUNT]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_CBT_NODE_COUNT]);
    glGenBuffers(1, &g_gl.buffers[BUFFER_CBT_NODE_COUNT]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_gl.buffers[BUFFER_CBT_NODE_COUNT]);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER,
                    sizeof(int32_t),
                    NULL,
                    GL_MAP_READ_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_CBT_NODE_COUNT,
                     g_gl.buffers[BUFFER_CBT_NODE_COUNT]);

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------
/**
 * Load the indirect command buffer
 *
 * This procedure initializes the subdivision buffer.
 */
bool LoadRenderCmdBuffer()
{
    uint32_t drawArraysCmd[8] = {2, 1, 0, 0, 0, 0, 0, 0};
    uint32_t drawMeshTasksCmd[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    uint32_t drawElementsCmd[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t dispatchCmd[8] = {2, 1, 1, 0, 0, 0, 0, 0};

    if (glIsBuffer(g_gl.buffers[BUFFER_TERRAIN_DRAW]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_TERRAIN_DRAW]);

    if (glIsBuffer(g_gl.buffers[BUFFER_TERRAIN_DRAW_MS]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_TERRAIN_DRAW_MS]);

    if (glIsBuffer(g_gl.buffers[BUFFER_TERRAIN_DRAW_CS]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_TERRAIN_DRAW_CS]);

    glGenBuffers(1, &g_gl.buffers[BUFFER_TERRAIN_DRAW]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_TERRAIN_DRAW]);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(drawArraysCmd), drawArraysCmd, GL_STATIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    glGenBuffers(1, &g_gl.buffers[BUFFER_TERRAIN_DRAW_MS]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_TERRAIN_DRAW_MS]);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(drawMeshTasksCmd), drawMeshTasksCmd, GL_STATIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    glGenBuffers(1, &g_gl.buffers[BUFFER_TERRAIN_DRAW_CS]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_TERRAIN_DRAW_CS]);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(drawElementsCmd), drawElementsCmd, GL_STATIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);


    glGenBuffers(1, &g_gl.buffers[BUFFER_TERRAIN_DISPATCH_CS]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_TERRAIN_DISPATCH_CS]);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(dispatchCmd), dispatchCmd, GL_STATIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------
/**
 * Load Meshlet Buffers
 *
 * This procedure creates a vertex and index buffer that represents a
 * subdividided triangle, which we refer to as a meshlet.
 */
bool LoadMeshletBuffers()
{
    std::vector<uint16_t> indexBuffer;
    std::vector<dja::vec2> vertexBuffer;
    std::map<uint32_t, uint16_t> hashMap;
    int lebDepth = 2 * g_terrain.gpuSubd;
    int triangleCount = 1 << lebDepth;
    int edgeTessellationFactor = 1 << g_terrain.gpuSubd;

    // compute index and vertex buffer
    for (int i = 0; i < triangleCount; ++i) {
        cbt_Node node = {(uint64_t)(triangleCount + i), 2 * g_terrain.gpuSubd};
        float attribArray[][3] = { {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} };

        leb_DecodeNodeAttributeArray(node, 2, attribArray);

        for (int j = 0; j < 3; ++j) {
            uint32_t vertexID = attribArray[0][j] * (edgeTessellationFactor + 1)
                              + attribArray[1][j] * (edgeTessellationFactor + 1) * (edgeTessellationFactor + 1);
            auto it = hashMap.find(vertexID);

            if (it != hashMap.end()) {
                indexBuffer.push_back(it->second);
            } else {
                uint16_t newIndex = (uint16_t)vertexBuffer.size();

                indexBuffer.push_back(newIndex);
                hashMap.insert(std::pair<uint32_t, uint16_t>(vertexID, newIndex));
                vertexBuffer.push_back(dja::vec2(attribArray[0][j], attribArray[1][j]));
            }
        }
    }

    if (glIsBuffer(g_gl.buffers[BUFFER_MESHLET_VERTICES]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_MESHLET_VERTICES]);

    if (glIsBuffer(g_gl.buffers[BUFFER_MESHLET_INDEXES]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_MESHLET_INDEXES]);

    LOG("Loading {Meshlet-Buffers}\n");

    glGenBuffers(1, &g_gl.buffers[BUFFER_MESHLET_INDEXES]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_gl.buffers[BUFFER_MESHLET_INDEXES]);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 sizeof(indexBuffer[0]) * indexBuffer.size(),
                 &indexBuffer[0],
                 GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glGenBuffers(1, &g_gl.buffers[BUFFER_MESHLET_VERTICES]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_gl.buffers[BUFFER_MESHLET_VERTICES]);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 sizeof(vertexBuffer[0]) * vertexBuffer.size(),
                 &vertexBuffer[0],
                 GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return (glGetError() == GL_NO_ERROR);
}


// -----------------------------------------------------------------------------
/**
 * Load Sphere Buffers
 *
 */
bool LoadSphereBuffers()
{
    djg_mesh *mesh = djgm_load_sphere(32, 32);
    int vertexCount, indexCount;
    const djgm_vertex *vertices = djgm_get_vertices(mesh, &vertexCount);
    const uint16_t *indexes = djgm_get_triangles(mesh, &indexCount);
    std::vector<float> vertexBuffer(vertexCount * 4);

    g_camera.sphere.triangleCount = indexCount;

    for (int i = 0; i < vertexCount; ++i) {
        vertexBuffer[4 * i + 0] = vertices[i].p.x;
        vertexBuffer[4 * i + 1] = vertices[i].p.y;
        vertexBuffer[4 * i + 2] = vertices[i].p.z;
        vertexBuffer[4 * i + 3] = vertices[i].p.w;
    }

    if (glIsBuffer(g_gl.buffers[BUFFER_SPHERE_VERTICES]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_SPHERE_VERTICES]);

    if (glIsBuffer(g_gl.buffers[BUFFER_SPHERE_INDEXES]))
        glDeleteBuffers(1, &g_gl.buffers[BUFFER_SPHERE_INDEXES]);

    LOG("Loading {Sphere-VertexBuffer}\n");
    glGenBuffers(1, &g_gl.buffers[BUFFER_SPHERE_VERTICES]);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_SPHERE_VERTICES]);
    glBufferStorage(GL_ARRAY_BUFFER,
                    vertexBuffer.size() *  sizeof(float),
                    &vertexBuffer[0],
                    0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    LOG("Loading {Sphere-IndexBuffer}\n");
    glGenBuffers(1, &g_gl.buffers[BUFFER_SPHERE_INDEXES]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl.buffers[BUFFER_SPHERE_INDEXES]);
    glBufferStorage(GL_ELEMENT_ARRAY_BUFFER,
                    indexCount * sizeof(uint16_t),
                    indexes,
                    0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    djgm_release(mesh);

    return (glGetError() == GL_NO_ERROR);
}

// -----------------------------------------------------------------------------
/**
 * Load All Buffers
 *
 */
bool LoadBuffers()
{
    bool v = true;

    if (v) v &= LoadTerrainVariables();
    if (v) v &= LoadLebBuffer();
    if (v) v &= LoadRenderCmdBuffer();
    if (v) v &= LoadMeshletBuffers();
    if (v) v &= LoadSphereBuffers();
    if (v) v &= LoadCbtNodeCountBuffer();

    return v;
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
    LOG("Loading {Empty-VertexArray}\n");
    if (glIsVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]))
        glDeleteVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_EMPTY]);

    glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
    glBindVertexArray(0);

    return (glGetError() == GL_NO_ERROR);
}

// -----------------------------------------------------------------------------
/**
 * Load Meshlet Vertex Array
 *
 */
bool LoadMeshletVertexArray()
{
    LOG("Loading {Meshlet-VertexArray}\n");
    if (glIsVertexArray(g_gl.vertexArrays[VERTEXARRAY_MESHLET]))
        glDeleteVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_MESHLET]);

    glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_MESHLET]);

    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_MESHLET]);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_MESHLET_VERTICES]);
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, BUFFER_OFFSET(0));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl.buffers[BUFFER_MESHLET_INDEXES]);
    glBindVertexArray(0);

    return (glGetError() == GL_NO_ERROR);
}

// -----------------------------------------------------------------------------
/**
 * Load Sphere Vertex Array
 *
 */
bool LoadSphereVertexArray()
{
    LOG("Loading {Sphere-VertexArray}\n");
    if (glIsVertexArray(g_gl.vertexArrays[VERTEXARRAY_SPHERE]))
        glDeleteVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_SPHERE]);

    glGenVertexArrays(1, &g_gl.vertexArrays[VERTEXARRAY_SPHERE]);

    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_SPHERE]);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.buffers[BUFFER_SPHERE_VERTICES]);
    glVertexAttribPointer(0, 4, GL_FLOAT, 0, 0, BUFFER_OFFSET(0));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl.buffers[BUFFER_SPHERE_INDEXES]);
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
    if (v) v &= LoadMeshletVertexArray();
    if (v) v &= LoadSphereVertexArray();

    return v;
}


// -----------------------------------------------------------------------------
/**
 * Load Node Count Query
 *
 */
bool LoadNodeCountQuery()
{
    GLuint *query = &g_gl.queries[QUERY_NODE_COUNT];

    glGenQueries(1, query);
    glQueryCounter(*query, GL_TIMESTAMP);

    return glGetError() == GL_NO_ERROR;
}


// -----------------------------------------------------------------------------
/**
 * Load All Queries
 *
 */
bool LoadQueries()
{
    bool success = true;

    if (success) success = LoadNodeCountQuery();

    return success;
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
    LOG("Loading {Scene-Framebuffer}\n");
    if (glIsFramebuffer(g_gl.framebuffers[FRAMEBUFFER_SCENE]))
        glDeleteFramebuffers(1, &g_gl.framebuffers[FRAMEBUFFER_SCENE]);

    glGenFramebuffers(1, &g_gl.framebuffers[FRAMEBUFFER_SCENE]);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gl.framebuffers[FRAMEBUFFER_SCENE]);

    if (g_framebuffer.aa >= AA_MSAA2 && g_framebuffer.aa <= AA_MSAA16) {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D_MULTISAMPLE,
            g_gl.textures[TEXTURE_CBUF],
            0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D_MULTISAMPLE,
            g_gl.textures[TEXTURE_ZBUF],
            0);
    }
    else {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            g_gl.textures[TEXTURE_CBUF],
            0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D,
            g_gl.textures[TEXTURE_ZBUF],
            0);
    }

    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    if (GL_FRAMEBUFFER_COMPLETE != glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
        LOG("=> Failure <=\n");

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

void init()
{
    bool v = true;
    int i;

    for (i = 0; i < CLOCK_COUNT; ++i) {
        if (g_gl.clocks[i])
            djgc_release(g_gl.clocks[i]);
        g_gl.clocks[i] = djgc_create();
    }

    if (v) v &= LoadTextures();
    if (v) v &= LoadBuffers();
    if (v) v &= LoadFramebuffers();
    if (v) v &= LoadVertexArrays();
    if (v) v &= LoadPrograms();
    if (v) v &= LoadQueries();


    updateCameraMatrix();

    if (!v) throw std::exception();
}

void release()
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
    for (i = 0; i < QUERY_COUNT; ++i)
        if (glIsQuery(g_gl.queries[i]))
            glDeleteQueries(1, &g_gl.queries[i]);
}

////////////////////////////////////////////////////////////////////////////////
// OpenGL Rendering
//
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
/**
 * Render top view
 *
 * This routine renders the terrain from a top view, which is useful for
 * debugging.
 */
void renderTopView()
{
    if (g_terrain.flags.topView) {
        glDisable(GL_CULL_FACE);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_LEB, g_gl.buffers[BUFFER_LEB]);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_TERRAIN_DRAW]);
        glViewport(10, 10, 512, 512);
        glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
        glPatchParameteri(GL_PATCH_VERTICES, 1);

        glUseProgram(g_gl.programs[PROGRAM_TOPVIEW]);
            glDrawArraysIndirect(GL_PATCHES, 0);

        glBindVertexArray(0);
        glViewport(0, 0, g_framebuffer.w, g_framebuffer.h);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_LEB, 0);
    }
}

// -----------------------------------------------------------------------------
/**
 * Reduction Pass -- Generic
 *
 * The reduction prepass is used for counting the number of nodes and
 * dispatch the threads to the proper node. This routine is entirely
 * generic and isn't tied to a specific pipeline.
 */
// LEB reduction step
void lebReductionPass()
{
    djgc_start(g_gl.clocks[CLOCK_REDUCTION]);
    int it = g_terrain.maxDepth;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_LEB, g_gl.buffers[BUFFER_LEB]);
    glUseProgram(g_gl.programs[PROGRAM_LEB_REDUCTION_PREPASS]);
    if (true) {
        int cnt = ((1 << it) >> 5);// / 2;
        int numGroup = (cnt >= 256) ? (cnt >> 8) : 1;
        int loc = glGetUniformLocation(g_gl.programs[PROGRAM_LEB_REDUCTION_PREPASS],
                                       "u_PassID");

        djgc_start(g_gl.clocks[CLOCK_REDUCTION00 + it - 1]);
        glUniform1i(loc, it);
        glDispatchCompute(numGroup, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        djgc_stop(g_gl.clocks[CLOCK_REDUCTION00 + g_terrain.maxDepth - 1]);

        it-= 5;
    }

    glUseProgram(g_gl.programs[PROGRAM_LEB_REDUCTION]);
    while (--it >= 0) {
        int loc = glGetUniformLocation(g_gl.programs[PROGRAM_LEB_REDUCTION], "u_PassID");
        int cnt = 1 << it;
        int numGroup = (cnt >= 256) ? (cnt >> 8) : 1;

        djgc_start(g_gl.clocks[CLOCK_REDUCTION00 + it]);
        glUniform1i(loc, it);
        glDispatchCompute(numGroup, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        djgc_stop(g_gl.clocks[CLOCK_REDUCTION00 + it]);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_LEB, 0);
    djgc_stop(g_gl.clocks[CLOCK_REDUCTION]);
}

// -----------------------------------------------------------------------------
/**
 * Batching Pass
 *
 * The batching pass prepares indirect draw calls for rendering the terrain.
 * This routine works for the
 */
void lebBatchingPassTsGs()
{
    glUseProgram(g_gl.programs[PROGRAM_BATCH]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_TERRAIN_DRAW,
                     g_gl.buffers[BUFFER_TERRAIN_DRAW]);

    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_TERRAIN_DRAW, 0);
}
void lebBatchingPassMs()
{
    glUseProgram(g_gl.programs[PROGRAM_BATCH]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_TERRAIN_DRAW,
                     g_gl.buffers[BUFFER_TERRAIN_DRAW]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_TERRAIN_DRAW_MS,
                     g_gl.buffers[BUFFER_TERRAIN_DRAW_MS]);

    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_TERRAIN_DRAW, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_TERRAIN_DRAW_MS, 0);
}
void lebBatchingPassCs()
{
    glUseProgram(g_gl.programs[PROGRAM_BATCH]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_TERRAIN_DRAW,
                     g_gl.buffers[BUFFER_TERRAIN_DRAW]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_TERRAIN_DRAW_CS,
                     g_gl.buffers[BUFFER_TERRAIN_DRAW_CS]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     BUFFER_TERRAIN_DISPATCH_CS,
                     g_gl.buffers[BUFFER_TERRAIN_DISPATCH_CS]);

    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_TERRAIN_DRAW, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_TERRAIN_DRAW_CS, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_TERRAIN_DISPATCH_CS, 0);
}
void lebBatchingPass()
{
    djgc_start(g_gl.clocks[CLOCK_BATCH]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_LEB, g_gl.buffers[BUFFER_LEB]);
    switch (g_terrain.method) {
    case METHOD_TS:
    case METHOD_GS:
        lebBatchingPassTsGs();
        break;
    case METHOD_CS:
        lebBatchingPassCs();
        break;
    case METHOD_MS:
        lebBatchingPassMs();
        break;
    default:
        break;
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_LEB, 0);
    djgc_stop(g_gl.clocks[CLOCK_BATCH]);
}

// -----------------------------------------------------------------------------
/**
 * Update Pass
 *
 * The update pass updates the LEB binary tree.
 * All pipelines except that the compute shader pipeline render the
 * terrain at the same time.
 */
void lebUpdateAndRenderTs(int pingPong)
{
    // set GL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);

    // update and render
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_TERRAIN_DRAW]);
    glPatchParameteri(GL_PATCH_VERTICES, 1);
    glUseProgram(g_gl.programs[PROGRAM_SPLIT + pingPong]);
        glDrawArraysIndirect(GL_PATCHES, 0);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // reset GL state
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}
void lebUpdateAndRenderGs(int pingPong)
{
    // set GL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);

    // update and render
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_TERRAIN_DRAW]);
    glUseProgram(g_gl.programs[PROGRAM_SPLIT + pingPong]);
        glDrawArraysIndirect(GL_POINTS, 0);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // reset GL state
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}
void lebUpdateAndRenderMs(int pingPong)
{
    // set GL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);

    // update and render
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_TERRAIN_DRAW_MS]);
    glUseProgram(g_gl.programs[PROGRAM_SPLIT + pingPong]);
        glDrawMeshTasksIndirectNV(0);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // reset GL state
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}
void lebUpdateCs(int pingPong)
{
    // set GL state
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER,
                 g_gl.buffers[BUFFER_TERRAIN_DISPATCH_CS]);

    // update
    glUseProgram(g_gl.programs[PROGRAM_SPLIT + pingPong]);
    glDispatchComputeIndirect(0);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // reset GL state
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
}
void lebUpdate()
{
    static int pingPong = 0;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_LEB, g_gl.buffers[BUFFER_LEB]);

    djgc_start(g_gl.clocks[CLOCK_UPDATE]);
    switch (g_terrain.method) {
    case METHOD_TS:
        lebUpdateAndRenderTs(pingPong);
        break;
    case METHOD_GS:
        lebUpdateAndRenderGs(pingPong);
        break;
    case METHOD_CS:
        lebUpdateCs(pingPong);
        break;
    case METHOD_MS:
        lebUpdateAndRenderMs(pingPong);
        break;
    default:
        break;
    }
    djgc_stop(g_gl.clocks[CLOCK_UPDATE]);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_LEB, 0);
    pingPong = 1 - pingPong;
}

// -----------------------------------------------------------------------------
/**
 * Render Pass (Compute shader pipeline only)
 *
 * The render pass renders the geometry to the framebuffer.
 */
void lebRenderCs()
{
    // set GL state
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_LEB, g_gl.buffers[BUFFER_LEB]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_TERRAIN_DRAW_CS]);
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_MESHLET]);

    // render
    glUseProgram(g_gl.programs[PROGRAM_RENDER_ONLY]);
    glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));

    // reset GL state
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_LEB, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}
void lebRender()
{
    djgc_start(g_gl.clocks[CLOCK_RENDER]);
    if (g_terrain.method == METHOD_CS) {
        lebRenderCs();
    }
    djgc_stop(g_gl.clocks[CLOCK_RENDER]);
}

// -----------------------------------------------------------------------------
void renderTerrain()
{
    djgc_start(g_gl.clocks[CLOCK_ALL]);

    LoadTerrainVariables();
    lebUpdate();
    lebReductionPass();
    lebBatchingPass();
    lebRender(); // render pass (if applicable)

    djgc_stop(g_gl.clocks[CLOCK_ALL]);
}

dja::mat4 cameraProjectionMatrix()
{
    dja::mat4 projection;
    if (g_camera.projection == PROJECTION_ORTHOGRAPHIC) {
        float ratio = (float)g_framebuffer.w / (float)g_framebuffer.h;
        float planeSize = tan(radians(g_camera.fovy / 2.0f));

        projection = dja::mat4::homogeneous::orthographic(
            -planeSize * ratio, planeSize * ratio, -planeSize, planeSize,
            g_camera.zNear, g_camera.zFar
        );
    } else if (g_camera.projection == PROJECTION_RECTILINEAR) {
        projection = dja::mat4::homogeneous::perspective(
            radians(g_camera.fovy),
            (float)g_framebuffer.w / (float)g_framebuffer.h,
            g_camera.zNear, g_camera.zFar
        );
    } else {
#if 0
        float ratio = (float)g_framebuffer.w / (float)g_framebuffer.h;
        float planeSize = tan(radians(g_camera.fovy / 2.0f));

        projection = dja::mat4::homogeneous::orthographic(
            -planeSize * ratio, planeSize * ratio, -planeSize, planeSize,
            g_camera.zNear, g_camera.zFar
        );
#else
        projection = dja::mat4::homogeneous::perspective(
            radians(g_camera.fovy),
            (float)g_framebuffer.w / (float)g_framebuffer.h,
            g_camera.zNear, g_camera.zFar
        );
#endif
    }

    return projection;
}

dja::mat4 cameraFrameMatrix()
{
    return dja::mat4::homogeneous::translation(g_camera.pos)
            * dja::mat4::homogeneous::from_mat3(g_camera.axis);
}


// -----------------------------------------------------------------------------
void renderSky()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_SPHERE]);
    glUseProgram(g_gl.programs[PROGRAM_SKY]);
    dja::mat4 camera = cameraFrameMatrix();
    dja::mat4 view = dja::inverse(camera);
    dja::mat4 viewProj = cameraProjectionMatrix() * view;
    dja::vec3 sunDir = dja::normalize(dja::vec3(1, 1, 1));

    glUniformMatrix4fv(g_gl.uniforms[UNIFORM_SKY_VIEW_PROJECTION_MATRIX], 1, GL_TRUE, &viewProj[0][0]);
    glUniformMatrix4fv(g_gl.uniforms[UNIFORM_SKY_CAMERA_MATRIX], 1, GL_TRUE, &camera[0][0]);
    glUniform3f(g_gl.uniforms[UNIFORM_SKY_SUN_DIR], sunDir.x, sunDir.y, sunDir.z);

    glDrawElements(GL_TRIANGLES,
                   g_camera.sphere.triangleCount,
                   GL_UNSIGNED_SHORT,
                   0);
    glDisable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);
}


// -----------------------------------------------------------------------------
/**
 * Retrieve Node Count
 *
 * Retrieves the number of nodes in the CBT asynchronously.
 */
void RetrieveNodeCount()
{
    static GLint isReady = GL_FALSE;
    const GLuint *query = &g_gl.queries[QUERY_NODE_COUNT];

    glGetQueryObjectiv(*query, GL_QUERY_RESULT_AVAILABLE, &isReady);

    if (isReady) {
        GLuint *buffer = &g_gl.buffers[BUFFER_CBT_NODE_COUNT];

        g_terrain.nodeCount = *(uint32_t *)
            glMapNamedBuffer(*buffer, GL_READ_ONLY | GL_MAP_UNSYNCHRONIZED_BIT);
        glUnmapNamedBuffer(g_gl.buffers[BUFFER_CBT_NODE_COUNT]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                         BUFFER_CBT_NODE_COUNT,
                         g_gl.buffers[BUFFER_CBT_NODE_COUNT]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                         BUFFER_LEB,
                         g_gl.buffers[BUFFER_LEB]);
        glUseProgram(g_gl.programs[PROGRAM_CBT_NODE_COUNT]);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glQueryCounter(*query, GL_TIMESTAMP);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                         BUFFER_CBT_NODE_COUNT,
                         0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                         BUFFER_LEB,
                         0);
    }
}

// -----------------------------------------------------------------------------
/**
 * Render Scene
 *
 * This procedure renders the scene to the back buffer.
 */
void renderScene()
{
    renderTerrain();
    RetrieveNodeCount();
    renderSky();
}


// -----------------------------------------------------------------------------
void renderViewer()
{
    // render framebuffer
    glUseProgram(g_gl.programs[PROGRAM_VIEWER]);
    glBindVertexArray(g_gl.vertexArrays[VERTEXARRAY_EMPTY]);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // render top view
    renderTopView();

    // draw HUD
    if (g_app.viewer.hud) {
        // ImGui
        glUseProgram(0);
        // Viewer Widgets
        //ImGui_ImplGlfw_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // Camera Widget
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(250, 180), ImGuiCond_FirstUseEver);
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
                LoadTerrainPrograms();
                LoadSkyProgram();
            }
            if (ImGui::Combo("Tonemap", &g_camera.tonemap, &eTonemaps[0], BUFFER_SIZE(eTonemaps)))
                LoadViewerProgram();
            if (ImGui::Combo("AA", &g_framebuffer.aa, &eAA[0], BUFFER_SIZE(eAA))) {
                LoadSceneFramebufferTexture();
                LoadSceneFramebuffer();
                LoadViewerProgram();
            }
            if (ImGui::SliderFloat("FOVY", &g_camera.fovy, 1.0f, 179.0f)) {
                ConfigureTerrainPrograms();
                ConfigureTopViewProgram();
            }
            if (ImGui::SliderFloat("zNear", &g_camera.zNear, 0.01f, 1.f)) {
                if (g_camera.zNear >= g_camera.zFar)
                    g_camera.zNear = g_camera.zFar - 0.01f;
            }
            if (ImGui::SliderFloat("zFar", &g_camera.zFar, 16.f, 4096.f)) {
                if (g_camera.zFar <= g_camera.zNear)
                    g_camera.zFar = g_camera.zNear + 0.01f;
            }

        }
        ImGui::End();

        // Performance Widget
        ImGui::SetNextWindowPos(ImVec2(g_framebuffer.w - 310, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 460), ImGuiCond_FirstUseEver);
        ImGui::Begin("Performance Analysis");
        {
            double cpuDt, gpuDt;

            djgc_ticks(g_gl.clocks[CLOCK_ALL], &cpuDt, &gpuDt);
            ImGui::Text("FPS %8.3f(CPU) %8.3f(GPU)", 1.f / cpuDt, 1.f / gpuDt);
            ImGui::NewLine();
            ImGui::Text("Timings:");
            djgc_ticks(g_gl.clocks[CLOCK_UPDATE], &cpuDt, &gpuDt);
            ImGui::Text("Subdivision -- CPU: %.3f%s",
                cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                cpuDt < 1. ? "ms" : " s");
            ImGui::SameLine();
            ImGui::Text("GPU: %.3f%s",
                gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                gpuDt < 1. ? "ms" : " s");
                djgc_ticks(g_gl.clocks[CLOCK_REDUCTION], &cpuDt, &gpuDt);
            ImGui::Text("Reduction   -- CPU: %.3f%s",
                cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                cpuDt < 1. ? "ms" : " s");
            ImGui::SameLine();
            ImGui::Text("GPU: %.3f%s",
                gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                gpuDt < 1. ? "ms" : " s");
            djgc_ticks(g_gl.clocks[CLOCK_BATCH], &cpuDt, &gpuDt);
            ImGui::Text("Dispatch    -- CPU: %.3f%s",
                cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                cpuDt < 1. ? "ms" : " s");
            ImGui::SameLine();
            ImGui::Text("GPU: %.3f%s",
                gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                gpuDt < 1. ? "ms" : " s");
            djgc_ticks(g_gl.clocks[CLOCK_RENDER], &cpuDt, &gpuDt);
            if (g_terrain.method == METHOD_CS) {
                ImGui::Text("Render      -- CPU: %.3f%s",
                    cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                    cpuDt < 1. ? "ms" : " s");
                ImGui::SameLine();
                ImGui::Text("GPU: %.3f%s",
                    gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                    gpuDt < 1. ? "ms" : " s");
            } else {
                ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1.0), "Render      -- CPU: %.3f%s",
                    cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                    cpuDt < 1. ? "ms" : " s");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1.0), "GPU: %.3f%s",
                    gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                    gpuDt < 1. ? "ms" : " s");
            }
            djgc_ticks(g_gl.clocks[CLOCK_ALL], &cpuDt, &gpuDt);
            ImGui::Text("All         -- CPU: %.3f%s",
                cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                cpuDt < 1. ? "ms" : " s");
            ImGui::SameLine();
            ImGui::Text("GPU: %.3f%s",
                gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                gpuDt < 1. ? "ms" : " s");
            djgc_ticks(g_gl.clocks[CLOCK_UPDATE], &cpuDt, &gpuDt);

            ImGui::NewLine();
            ImGui::Text("Reduction Details:");
            for (int i = 0; i < g_terrain.maxDepth; ++i) {
                if (i >= g_terrain.maxDepth - 5 && i < g_terrain.maxDepth - 1)
                    continue;

                djgc_ticks(g_gl.clocks[CLOCK_REDUCTION00 + i], &cpuDt, &gpuDt);
                ImGui::Text("Reduction%02i -- CPU: %.3f%s",
                    i,
                    cpuDt < 1. ? cpuDt * 1e3 : cpuDt,
                    cpuDt < 1. ? "ms" : " s");
                ImGui::SameLine();
                ImGui::Text("GPU: %.3f%s",
                    gpuDt < 1. ? gpuDt * 1e3 : gpuDt,
                    gpuDt < 1. ? "ms" : " s");

            }

#if 0
            static int count = 1;

            if (++count == 100) {
                LOG("Dispatch   : %.3f\n", dispatchGpu * 1e3 / count);
                LOG("Subdivision: %.3f\n", subdivisionGpu * 1e3 / count);
                LOG("Reduction  : %.3f\n", sumReductionGpu * 1e3 / count);
                LOG("Render     : %.3f\n", renderGpu * 1e3 / count);
                LOG("\n");
                dispatchGpu = 0;
                subdivisionGpu = 0;
                sumReductionGpu = 0;
                renderGpu = 0;
                count = 1;
            }
#endif
        }
        ImGui::End();

        // Terrain Parameters
        ImGui::SetNextWindowPos(ImVec2(270, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(380, 235), ImGuiCond_FirstUseEver);
        ImGui::Begin("Terrain Settings");
        {
            const char* eShadings[] = {
                "Diffuse",
                "Normals",
                "Plain Color"
            };
            std::vector<const char *> ePipelines = {
                "Compute Shader",
                "Tessellation Shader",
                "Geometry Shader"
            };
            if (GLAD_GL_NV_mesh_shader)
                ePipelines.push_back("Mesh Shader");

            if (ImGui::Combo("Shading", &g_terrain.shading, &eShadings[0], BUFFER_SIZE(eShadings)))
                LoadTerrainPrograms();
            if (ImGui::Combo("GPU Pipeline", &g_terrain.method, &ePipelines[0], ePipelines.size())) {
                LoadTerrainPrograms();
                LoadBatchProgram();
            } if (ImGui::Checkbox("Cull", &g_terrain.flags.cull))
                LoadPrograms();
            ImGui::SameLine();
            if (ImGui::Checkbox("Wire", &g_terrain.flags.wire))
                LoadTerrainPrograms();
            ImGui::SameLine();
            if (ImGui::Checkbox("Freeze", &g_terrain.flags.freeze)) {
                LoadTerrainPrograms();
            }
            if (!g_terrain.dmap.pathToFile.empty()) {
                ImGui::SameLine();
                if (ImGui::Checkbox("Displace", &g_terrain.flags.displace)) {
                    LoadTerrainPrograms();
                    LoadTopViewProgram();
                }
            }
            ImGui::SameLine();
            ImGui::Checkbox("TopView", &g_terrain.flags.topView);
            if (ImGui::SliderFloat("PixelsPerEdge", &g_terrain.primitivePixelLengthTarget, 1, 32)) {
                ConfigureTerrainPrograms();
            }
            if (ImGui::SliderFloat("DmapScale", &g_terrain.dmap.scale, 0.f, 1.f)) {
                ConfigureTerrainPrograms();
                ConfigureTopViewProgram();
            }
            if (ImGui::SliderFloat("LodStdev", &g_terrain.minLodStdev, 0.f, 1.0f, "%.4f")) {
                ConfigureTerrainPrograms();
            }
            if (ImGui::SliderInt("PatchSubdLevel", &g_terrain.gpuSubd, 0, 6)) {
                LoadMeshletBuffers();
                LoadMeshletVertexArray();
                LoadPrograms();
            }
            if (ImGui::SliderInt("MaxDepth", &g_terrain.maxDepth, 5, 29)) {
                LoadBuffers();
                LoadPrograms();
            }
            ImGui::Text("CBT nodes: %i", g_terrain.nodeCount);
            {
                uint32_t bufSize = cbt__HeapByteSize(g_terrain.maxDepth);

                if (bufSize < (1 << 10)) {
                    ImGui::Text("CBT heap size: %i Bytes", bufSize);
                } else if (bufSize < (1 << 20)) {
                    ImGui::Text("CBT heap size: %i KBytes", bufSize >> 10);
                } else {
                    ImGui::Text("CBT heap size: %i MBytes", bufSize >> 20);
                }
            }
        }
        ImGui::End();

        //ImGui::ShowDemoWindow();

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
        strcat2(path, g_app.dir.output, name);
        djgt_save_glcolorbuffer_bmp(GL_BACK, GL_RGB, path);
        ++g_app.recorder.frame;
    }
}

// -----------------------------------------------------------------------------
/**
 * Render Everything
 *
 */
void render()
{
    glBindFramebuffer(GL_FRAMEBUFFER, g_gl.framebuffers[FRAMEBUFFER_SCENE]);
    glViewport(0, 0, g_framebuffer.w, g_framebuffer.h);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderScene();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_app.viewer.w, g_app.viewer.h);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    renderViewer();


    ++g_app.frame;
}

////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
void
keyboardCallback(
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
            }
            g_app.recorder.on = !g_app.recorder.on;
            break;
        case GLFW_KEY_R:
            LoadBuffers();
            LoadPrograms();
            break;
        default: break;
        }
    }
}

void mouseButtonCallback(GLFWwindow*, int, int, int)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;
}

void mouseMotionCallback(GLFWwindow* window, double x, double y)
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

void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    if (io.WantCaptureMouse)
        return;

    dja::mat3 axis = dja::transpose(g_camera.axis);
    g_camera.pos -= axis[2] * yoffset * 5e-2 * norm(g_camera.pos);
}


void resizeCallback(GLFWwindow* window, int width, int height)
{
    g_framebuffer.w = width;
    g_framebuffer.h = height;
    g_app.viewer.w = width;
    g_app.viewer.h = height;

    LoadSceneFramebufferTexture();
    LoadSceneFramebuffer();
    ConfigureTerrainPrograms();
    ConfigureTopViewProgram();
}

void usage(const char *app)
{
    printf("%s -- OpenGL Terrain Renderer\n", app);
    printf("usage: %s --shader-dir path_to_shader_dir\n", app);
}


// -----------------------------------------------------------------------------
int main(int, char **)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    // Create the Window
    LOG("Loading {Window-Main}\n");
    GLFWwindow* window = glfwCreateWindow(
        VIEWER_DEFAULT_WIDTH, VIEWER_DEFAULT_HEIGHT,
        "Longest Edge Bisection Demo", NULL, NULL
    );
    if (window == NULL) {
        LOG("=> Failure <=\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, &keyboardCallback);
    glfwSetCursorPosCallback(window, &mouseMotionCallback);
    glfwSetMouseButtonCallback(window, &mouseButtonCallback);
    glfwSetScrollCallback(window, &mouseScrollCallback);
    glfwSetWindowSizeCallback(window, &resizeCallback);

    // Load OpenGL functions
    LOG("Loading {OpenGL}\n");
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG("gladLoadGLLoader failed\n");
        return -1;
    }

    LOG("-- Begin -- Demo\n");
    try {
        log_debug_output();
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, false);
        ImGui_ImplOpenGL3_Init("#version 450");
        LOG("-- Begin -- Init\n");
        init();
        LOG("-- End -- Init\n");

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            render();

            glfwSwapBuffers(window);
        }

        release();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
    }
    catch (std::exception& e) {
        LOG("%s", e.what());
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
        LOG("(!) Demo Killed (!)\n");

        return EXIT_FAILURE;
    }
    catch (...) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
        LOG("(!) Demo Killed (!)\n");

        return EXIT_FAILURE;
    }
    LOG("-- End -- Demo\n");

    return 0;
}

