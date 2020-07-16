#include <cstdio>
#include <cstdlib>
#include <utility>

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl.h"

#define LOG(fmt, ...) fprintf(stdout, fmt "\n", ##__VA_ARGS__); fflush(stdout);

#define CBT_IMPLEMENTATION
#include "cbt.h"

#define LEB_IMPLEMENTATION
#include "leb.h"

#define DJ_OPENGL_IMPLEMENTATION
#include "dj_opengl.h"

#define CBT_INIT_MAX_DEPTH 1

#ifndef PATH_TO_SRC_DIRECTORY
#   define PATH_TO_SRC_DIRECTORY "./"
#endif

struct Window {
    const char *name;
    int32_t width, height;
    struct {
        int32_t major, minor;
    } glversion;
    GLFWwindow* handle;
} g_window = {
    "Longest Edge Bisection",
    720, 720,
    {4, 5},
    NULL
};

#define CBT_MAX_DEPTH 20
enum {MODE_TRIANGLE, MODE_SQUARE};
enum {BACKEND_CPU, BACKEND_GPU};
struct LongestEdgeBisection {
    cbt_Tree *cbt;
    struct {
        int mode;
        int backend;
        struct {
            float x, y;
        } target;
    } params;
    int32_t triangleCount;
} g_leb = {
    cbt_CreateAtDepth(CBT_MAX_DEPTH, CBT_INIT_MAX_DEPTH),
    {
        MODE_TRIANGLE,
        BACKEND_GPU,
        {0.49951f, 0.41204f}
    },
    0
};
#undef CBT_MAX_DEPTH

enum {
    PROGRAM_TRIANGLES,
    PROGRAM_TARGET,
    PROGRAM_CBT_SUM_REDUCTION_PREPASS,
    PROGRAM_CBT_SUM_REDUCTION,
    PROGRAM_CBT_DISPATCH,
    PROGRAM_LEB_DISPATCH,
    PROGRAM_LEB_SPLIT,
    PROGRAM_LEB_MERGE,

    PROGRAM_COUNT
};
enum {
    BUFFER_CBT,
    BUFFER_CBT_DISPATCHER,
    BUFFER_LEB_DISPATCHER,
    BUFFER_TRIANGLE_COUNT,

    BUFFER_COUNT
};
enum {
    VERTEXARRAY_EMPTY,

    VERTEXARRAY_COUNT
};
enum {
    QUERY_NODE_COUNT,

    QUERY_COUNT
};
enum {
    CLOCK_DISPATCHER,
    CLOCK_SUBDIVISION_SPLIT,
    CLOCK_SUBDIVISION_MERGE,
    CLOCK_SUM_REDUCTION,

    CLOCK_COUNT
};
struct OpenGL {
    GLuint programs[PROGRAM_COUNT];
    GLuint vertexarrays[VERTEXARRAY_COUNT];
    GLuint buffers[BUFFER_COUNT];
    GLuint queries[QUERY_COUNT];
    djg_clock *clocks[CLOCK_COUNT];
} g_gl = {
    {0},
    {0},
    {0},
    {0},
    {NULL}
};

#define PATH_TO_SHADER_DIRECTORY PATH_TO_SRC_DIRECTORY "subdivision/shaders/"
#define PATH_TO_CBT_DIRECTORY PATH_TO_SRC_DIRECTORY "submodules/libcbt/"
#define PATH_TO_LEB_DIRECTORY PATH_TO_SRC_DIRECTORY "submodules/libleb/"

bool LoadTargetProgram()
{
    LOG("Loading {Target Program}")
    djg_program *djgp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_TARGET];

    djgp_push_file(djgp, PATH_TO_SHADER_DIRECTORY "target.glsl");
    if (!djgp_to_gl(djgp, 450, false, true, glp)) {
        djgp_release(djgp);

        return false;
    }

    djgp_release(djgp);

    return glGetError() == GL_NO_ERROR;
}

bool LoadCbtSumReductionPrepassProgram()
{
    LOG("Loading {CBT-Sum-Reduction-Prepass Program}");
    djg_program *djgp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_CBT_SUM_REDUCTION_PREPASS];

    djgp_push_string(djgp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_CBT);
    djgp_push_file(djgp, PATH_TO_CBT_DIRECTORY "glsl/cbt.glsl");
    djgp_push_file(djgp, PATH_TO_CBT_DIRECTORY "glsl/cbt_SumReductionPrepass.glsl");
    djgp_push_string(djgp, "#ifdef COMPUTE_SHADER\n#endif");
    if (!djgp_to_gl(djgp, 450, false, true, glp)) {
        djgp_release(djgp);

        return false;
    }

    djgp_release(djgp);

    return glGetError() == GL_NO_ERROR;
}

bool LoadCbtSumReductionProgram()
{
    LOG("Loading {CBT-Sum-Reduction Program}");
    djg_program *djgp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_CBT_SUM_REDUCTION];

    djgp_push_string(djgp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_CBT);
    djgp_push_file(djgp, PATH_TO_CBT_DIRECTORY "glsl/cbt.glsl");
    djgp_push_file(djgp, PATH_TO_CBT_DIRECTORY "glsl/cbt_SumReduction.glsl");
    djgp_push_string(djgp, "#ifdef COMPUTE_SHADER\n#endif");
    if (!djgp_to_gl(djgp, 450, false, true, glp)) {
        djgp_release(djgp);

        return false;
    }

    djgp_release(djgp);

    return glGetError() == GL_NO_ERROR;
}

bool LoadCbtDispatcherProgram()
{
    LOG("Loading {CBT-Dispatcher Program}");
    djg_program *djgp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_CBT_DISPATCH];

    djgp_push_string(djgp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_CBT);
    djgp_push_string(djgp, "#define CBT_DISPATCHER_BUFFER_BINDING %i\n", BUFFER_CBT_DISPATCHER);
    djgp_push_file(djgp, PATH_TO_CBT_DIRECTORY "glsl/cbt.glsl");
    djgp_push_file(djgp, PATH_TO_CBT_DIRECTORY "glsl/cbt_Dispatcher.glsl");
    djgp_push_string(djgp, "#ifdef COMPUTE_SHADER\n#endif");
    if (!djgp_to_gl(djgp, 450, false, true, glp)) {
        djgp_release(djgp);

        return false;
    }

    djgp_release(djgp);

    return glGetError() == GL_NO_ERROR;
}

bool LoadLebDispatcherProgram()
{
    LOG("Loading {CBT-Dispatcher Program}");
    djg_program *djgp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_LEB_DISPATCH];

    djgp_push_string(djgp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_CBT);
    djgp_push_string(djgp, "#define LEB_DISPATCHER_BUFFER_BINDING %i\n", BUFFER_LEB_DISPATCHER);
    djgp_push_file(djgp, PATH_TO_CBT_DIRECTORY "glsl/cbt.glsl");
    djgp_push_file(djgp, PATH_TO_SHADER_DIRECTORY "leb_dispatcher.glsl");
    djgp_push_string(djgp, "#ifdef COMPUTE_SHADER\n#endif");
    if (!djgp_to_gl(djgp, 450, false, true, glp)) {
        djgp_release(djgp);

        return false;
    }

    djgp_release(djgp);

    return glGetError() == GL_NO_ERROR;
}

bool LoadSubdivisionProgram(int programID, const char *flags)
{
    LOG("Loading {Subdivision Program}")
    djg_program *djgp = djgp_create();
    GLuint *glp = &g_gl.programs[programID];

    if (g_leb.params.mode == MODE_SQUARE)
        djgp_push_string(djgp, "#define MODE_SQUARE\n");
    else
        djgp_push_string(djgp, "#define MODE_TRIANGLE\n");

    djgp_push_string(djgp, flags);
    djgp_push_string(djgp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_CBT);
    djgp_push_file(djgp, PATH_TO_CBT_DIRECTORY "glsl/cbt.glsl");
    djgp_push_file(djgp, PATH_TO_LEB_DIRECTORY "glsl/leb.glsl");
    djgp_push_file(djgp, PATH_TO_SHADER_DIRECTORY "subdivision.glsl");
    djgp_push_string(djgp, "#ifdef COMPUTE_SHADER\n#endif");
    if (!djgp_to_gl(djgp, 450, false, true, glp)) {
        djgp_release(djgp);

        return false;
    }

    djgp_release(djgp);

    return glGetError() == GL_NO_ERROR;
}

bool LoadSubdivisionSplitProgram()
{
    return LoadSubdivisionProgram(PROGRAM_LEB_SPLIT, "#define FLAG_SPLIT 1\n");
}

bool LoadSubdivisionMergeProgram()
{
    return LoadSubdivisionProgram(PROGRAM_LEB_MERGE, "#define FLAG_MERGE 1\n");
}

bool LoadSubdivisionPrograms()
{
    return LoadSubdivisionMergeProgram() && LoadSubdivisionSplitProgram();
}

bool LoadTrianglesProgram()
{
    LOG("Loading {Triangles Program}")
    djg_program *djgp = djgp_create();
    GLuint *glp = &g_gl.programs[PROGRAM_TRIANGLES];

    if (g_leb.params.mode == MODE_SQUARE)
        djgp_push_string(djgp, "#define MODE_SQUARE\n");
    else
        djgp_push_string(djgp, "#define MODE_TRIANGLE\n");

    djgp_push_string(djgp, "#define CBT_HEAP_BUFFER_BINDING %i\n", BUFFER_CBT);
    djgp_push_file(djgp, PATH_TO_CBT_DIRECTORY "glsl/cbt.glsl");
    djgp_push_file(djgp, PATH_TO_LEB_DIRECTORY "glsl/leb.glsl");
    djgp_push_file(djgp, PATH_TO_SHADER_DIRECTORY "triangles.glsl");
    if (!djgp_to_gl(djgp, 450, false, true, glp)) {
        djgp_release(djgp);

        return false;
    }

    djgp_release(djgp);

    return glGetError() == GL_NO_ERROR;
}


bool LoadPrograms()
{
    bool success = true;

    if (success) success = LoadTrianglesProgram();
    if (success) success = LoadTargetProgram();
    if (success) success = LoadCbtSumReductionPrepassProgram();
    if (success) success = LoadCbtSumReductionProgram();
    if (success) success = LoadCbtDispatcherProgram();
    if (success) success = LoadLebDispatcherProgram();
    if (success) success = LoadSubdivisionPrograms();

    return success;
}

#undef PATH_TO_LEB_DIRECTORY
#undef PATH_TO_CBT_DIRECTORY
#undef PATH_TO_SHADER_DIRECTORY

bool LoadEmptyVertexArray()
{
    GLuint *glv = &g_gl.vertexarrays[VERTEXARRAY_EMPTY];

    glGenVertexArrays(1, glv);
    glBindVertexArray(*glv);
    glBindVertexArray(0);

    return glGetError() == GL_NO_ERROR;
}

bool LoadVertexArrays()
{
    bool success = true;

    if (success) success = LoadEmptyVertexArray();

    return success;
}

bool LoadTriangleCountBuffer()
{
    GLuint *buffer = &g_gl.buffers[BUFFER_TRIANGLE_COUNT];

    glGenBuffers(1, buffer);
    glBindBuffer(GL_ARRAY_BUFFER, *buffer);
    glBufferStorage(GL_ARRAY_BUFFER,
                    sizeof(int32_t),
                    NULL,
                    GL_MAP_READ_BIT);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return glGetError() == GL_NO_ERROR;
}

bool LoadCbtBuffer()
{
    GLuint *buffer = &g_gl.buffers[BUFFER_CBT];

    if (glIsBuffer(*buffer))
        glDeleteBuffers(1, buffer);

    glGenBuffers(1, buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, *buffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER,
                    cbt_HeapByteSize(g_leb.cbt),
                    cbt_GetHeap(g_leb.cbt),
                    0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_CBT, *buffer);

    return glGetError() == GL_NO_ERROR;
}

bool LoadCbtDispatcherBuffer()
{
    GLuint *buffer = &g_gl.buffers[BUFFER_CBT_DISPATCHER];
    uint32_t dispatchCmd[8] = {CBT_INIT_MAX_DEPTH, 1, 1, 0, 0, 0, 0, 0};

    glGenBuffers(1, buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, *buffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER,
                    sizeof(dispatchCmd),
                    dispatchCmd,
                    0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_CBT_DISPATCHER, *buffer);

    return glGetError() == GL_NO_ERROR;
}

bool LoadLebDispatcherBuffer()
{
    GLuint *buffer = &g_gl.buffers[BUFFER_LEB_DISPATCHER];
    uint32_t drawArraysCmd[8] = {3, CBT_INIT_MAX_DEPTH, 0, 0, 0, 0, 0, 0};

    glGenBuffers(1, buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, *buffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER,
                    sizeof(drawArraysCmd),
                    drawArraysCmd,
                    0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_LEB_DISPATCHER, *buffer);

    return glGetError() == GL_NO_ERROR;
}

bool LoadBuffers()
{
    bool success = true;

    if (success) success = LoadCbtBuffer();
    if (success) success = LoadCbtDispatcherBuffer();
    if (success) success = LoadLebDispatcherBuffer();
    if (success) success = LoadTriangleCountBuffer();

    return success;
}

bool LoadNodeCountQuery()
{
    GLuint *query = &g_gl.queries[QUERY_NODE_COUNT];

    glGenQueries(1, query);
    glQueryCounter(*query, GL_TIMESTAMP);

    return glGetError() == GL_NO_ERROR;
}

bool LoadQueries()
{
    bool success = true;

    if (success) success = LoadNodeCountQuery();

    return success;
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

bool Load()
{
    bool success = true;

    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(&DebugOutputLogger, NULL);

    for (int i = 0; i < CLOCK_COUNT; ++i)
        g_gl.clocks[i] = djgc_create();

    if (success) success = LoadPrograms();
    if (success) success = LoadVertexArrays();
    if (success) success = LoadBuffers();
    if (success) success = LoadQueries();

    return success;
}

void Release()
{
    glDeleteBuffers(BUFFER_COUNT, g_gl.buffers);
    glDeleteVertexArrays(VERTEXARRAY_COUNT, g_gl.vertexarrays);

    for (int i = 0; i < CLOCK_COUNT; ++i)
        djgc_release(g_gl.clocks[i]);
    for (int i = 0; i < PROGRAM_COUNT; ++i)
        glDeleteProgram(g_gl.programs[i]);
}

void ReductionKernel()
{
    int it = cbt_MaxDepth(g_leb.cbt);

    glUseProgram(g_gl.programs[PROGRAM_CBT_SUM_REDUCTION_PREPASS]);
    if (true) {
        int cnt = ((1 << it) >> 5);
        int numGroup = (cnt >= 256) ? (cnt >> 8) : 1;
        int loc = glGetUniformLocation(g_gl.programs[PROGRAM_CBT_SUM_REDUCTION_PREPASS],
                                       "u_PassID");

        glUniform1i(loc, it);
        glDispatchCompute(numGroup, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        it-= 5;
    }

    glUseProgram(g_gl.programs[PROGRAM_CBT_SUM_REDUCTION]);
    while (--it >= 0) {
        int loc = glGetUniformLocation(g_gl.programs[PROGRAM_CBT_SUM_REDUCTION], "u_PassID");
        int cnt = 1 << it;
        int numGroup = (cnt >= 256) ? (cnt >> 8) : 1;

        glUniform1i(loc, it);
        glDispatchCompute(numGroup, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
}

void DispatcherKernel()
{
    glUseProgram(g_gl.programs[PROGRAM_CBT_DISPATCH]);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glUseProgram(0);
}

void SubdivisionKernel(int pingPong)
{
    const GLuint *program = &g_gl.programs[PROGRAM_LEB_SPLIT + pingPong];

    glUseProgram(*program);
    int loc = glGetUniformLocation(*program, "u_TargetPosition");
    glUniform2f(loc, g_leb.params.target.x, g_leb.params.target.y);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER,
                 g_gl.buffers[BUFFER_CBT_DISPATCHER]);
    glDispatchComputeIndirect(0);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glUseProgram(0);
}

void LebDispatcherKernel()
{
    glUseProgram(g_gl.programs[PROGRAM_LEB_DISPATCH]);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glUseProgram(0);
}

void InitGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(g_window.handle, false);
    ImGui_ImplOpenGL3_Init("#version 450");

    ImGuiStyle& style = ImGui::GetStyle();

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.04f, 0.04f, 0.04f, 0.4f);

}

void ReleaseGui()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

float Wedge(const float *a, const float *b)
{
    return a[0] * b[1] - a[1] * b[0];
}

bool IsInside(const float faceVertices[][3])
{
    float target[2] = {g_leb.params.target.x, g_leb.params.target.y};
    float v1[2] = {faceVertices[0][0], faceVertices[1][0]};
    float v2[2] = {faceVertices[0][1], faceVertices[1][1]};
    float v3[2] = {faceVertices[0][2], faceVertices[1][2]};
    float x1[2] = {v2[0] - v1[0], v2[1] - v1[1]};
    float x2[2] = {v3[0] - v2[0], v3[1] - v2[1]};
    float x3[2] = {v1[0] - v3[0], v1[1] - v3[1]};
    float y1[2] = {target[0] - v1[0], target[1] - v1[1]};
    float y2[2] = {target[0] - v2[0], target[1] - v2[1]};
    float y3[2] = {target[0] - v3[0], target[1] - v3[1]};
    float w1 = Wedge(x1, y1);
    float w2 = Wedge(x2, y2);
    float w3 = Wedge(x3, y3);

    return (w1 >= 0.0f) && (w2 >= 0.0f) && (w3 >= 0.0f);
}

void
UpdateSubdivisionCpuCallback_Split(
    cbt_Tree *cbt,
    const cbt_Node node,
    const void *userData
) {
    (void)userData;
    float faceVertices[][3] = {
        {0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f}
    };

    if (g_leb.params.mode == MODE_TRIANGLE) {
        leb_DecodeNodeAttributeArray(node, 2, faceVertices);

        if (IsInside(faceVertices)) {
            leb_SplitNode(cbt, node);
        }
    } else {
        leb_DecodeNodeAttributeArray_Square(node, 2, faceVertices);

        if (IsInside(faceVertices)) {
            leb_SplitNode_Square(cbt, node);
        }
    }
}

void
UpdateSubdivisionCpuCallback_Merge(
    cbt_Tree *cbt,
    const cbt_Node node,
    const void *userData
) {
    (void)userData;
    float baseFaceVertices[][3] = {
        {0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f}
    };
    float topFaceVertices[][3] = {
        {0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f}
    };

    if (g_leb.params.mode == MODE_TRIANGLE) {
        leb_DiamondParent diamondParent = leb_DecodeDiamondParent(node);

        leb_DecodeNodeAttributeArray(diamondParent.base, 2, baseFaceVertices);
        leb_DecodeNodeAttributeArray(diamondParent.top, 2, topFaceVertices);

        if (!IsInside(baseFaceVertices) && !IsInside(topFaceVertices)) {
            leb_MergeNode(cbt, node, diamondParent);
        }
    } else {
        leb_DiamondParent diamondParent = leb_DecodeDiamondParent_Square(node);

        leb_DecodeNodeAttributeArray_Square(diamondParent.base, 2, baseFaceVertices);
        leb_DecodeNodeAttributeArray_Square(diamondParent.top, 2, topFaceVertices);

        if (!IsInside(baseFaceVertices) && !IsInside(topFaceVertices)) {
            leb_MergeNode_Square(cbt, node, diamondParent);
        }
    }
}

void UpdateSubdivision()
{
    static int pingPong = 0;

    if (g_leb.params.backend == BACKEND_CPU) {

        djgc_start(g_gl.clocks[CLOCK_SUBDIVISION_SPLIT + pingPong]);
        if (pingPong == 0) {
            cbt_Update(g_leb.cbt, &UpdateSubdivisionCpuCallback_Split, NULL);
        } else {
            cbt_Update(g_leb.cbt, &UpdateSubdivisionCpuCallback_Merge, NULL);
        }
        djgc_stop(g_gl.clocks[CLOCK_SUBDIVISION_SPLIT + pingPong]);

        LoadCbtBuffer();

    } else {
        djgc_start(g_gl.clocks[CLOCK_DISPATCHER]);
        DispatcherKernel();
        djgc_stop(g_gl.clocks[CLOCK_DISPATCHER]);

        djgc_start(g_gl.clocks[CLOCK_SUBDIVISION_SPLIT + pingPong]);
        SubdivisionKernel(pingPong);
        djgc_stop(g_gl.clocks[CLOCK_SUBDIVISION_SPLIT + pingPong]);

        djgc_start(g_gl.clocks[CLOCK_SUM_REDUCTION]);
        ReductionKernel();
        djgc_stop(g_gl.clocks[CLOCK_SUM_REDUCTION]);
    }

    pingPong = 1 - pingPong;
}

void DrawTarget()
{
    // target helper
    glUseProgram(g_gl.programs[PROGRAM_TARGET]);
    glUniform2f(
        glGetUniformLocation(g_gl.programs[PROGRAM_TARGET], "u_Target"),
        g_leb.params.target.x, g_leb.params.target.y
    );
    glPointSize(11.f);
    glBindVertexArray(g_gl.vertexarrays[VERTEXARRAY_EMPTY]);
        glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
    glUseProgram(0);
}

void RetrieveNodeCount()
{
    static GLint isReady = GL_FALSE;
    const GLuint *query = &g_gl.queries[QUERY_NODE_COUNT];

    glGetQueryObjectiv(*query, GL_QUERY_RESULT_AVAILABLE, &isReady);

    if (isReady) {
        GLuint *buffer = &g_gl.buffers[BUFFER_TRIANGLE_COUNT];

        g_leb.triangleCount = *(int *)
            glMapNamedBuffer(*buffer, GL_READ_ONLY | GL_MAP_UNSYNCHRONIZED_BIT);
        glUnmapNamedBuffer(g_gl.buffers[BUFFER_TRIANGLE_COUNT]);
        glCopyNamedBufferSubData(g_gl.buffers[BUFFER_LEB_DISPATCHER],
                                 g_gl.buffers[BUFFER_TRIANGLE_COUNT],
                                 sizeof(int32_t),
                                 0,
                                 sizeof(int32_t));
        glQueryCounter(*query, GL_TIMESTAMP);
    }
}

void DrawLeb()
{
    // prepare indirect draw command
    glUseProgram(g_gl.programs[PROGRAM_LEB_DISPATCH]);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // draw
    glEnable(GL_CULL_FACE);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_gl.buffers[BUFFER_LEB_DISPATCHER]);
    glUseProgram(g_gl.programs[PROGRAM_TRIANGLES]);
    glUniform2f(glGetUniformLocation(g_gl.programs[PROGRAM_TRIANGLES],
                                     "u_ScreenResolution"),
                g_window.width, g_window.height);
    glBindVertexArray(g_gl.vertexarrays[VERTEXARRAY_EMPTY]);
        glDrawArraysIndirect(GL_TRIANGLES, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_CULL_FACE);

    RetrieveNodeCount();
}

void Draw()
{
    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, g_window.width, g_window.height);
    DrawLeb();
    DrawTarget();
}

void DrawGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::Begin("Window");
    {
        const char* eModes[] = {"Triangle", "Square"};
        const char* eBackends[] = {"CPU", "GPU"};
        int32_t cbtByteSize = cbt_HeapByteSize(g_leb.cbt);
        int32_t maxDepth = cbt_MaxDepth(g_leb.cbt);
        double cpuDt, gpuDt;

        if (ImGui::Combo("Mode", &g_leb.params.mode, &eModes[0], 2)) {
            cbt_ResetToDepth(g_leb.cbt, CBT_INIT_MAX_DEPTH);
            LoadCbtBuffer();
            LoadPrograms();
        }
        if (ImGui::Combo("Backend", &g_leb.params.backend, &eBackends[0], 2)) {
            cbt_ResetToDepth(g_leb.cbt, CBT_INIT_MAX_DEPTH);
            LoadCbtBuffer();
        }
        ImGui::SliderFloat("TargetX", &g_leb.params.target.x, -0.1, 1.1);
        ImGui::SliderFloat("TargetY", &g_leb.params.target.y, -0.1, 1.1);
        if (ImGui::SliderInt("MaxDepth", &maxDepth, 6, 30)) {
            cbt_Release(g_leb.cbt);
            g_leb.cbt = cbt_CreateAtDepth(maxDepth, CBT_INIT_MAX_DEPTH);
            LoadCbtBuffer();
            LoadPrograms();
        }
        if (ImGui::Button("Reset")) {
            cbt_ResetToDepth(g_leb.cbt, CBT_INIT_MAX_DEPTH);
            LoadCbtBuffer();
        }
        ImGui::Separator();
        ImGui::Text("Nodes: %i", g_leb.triangleCount);
        ImGui::Text("Mem Usage: %u %s",
                    cbtByteSize >= (1 << 20) ? (cbtByteSize >> 20) : (cbtByteSize >= (1 << 10) ? cbtByteSize >> 10 : cbtByteSize),
                    cbtByteSize >= (1 << 20) ? "MiB" : (cbtByteSize > (1 << 10) ? "KiB" : "B"));
        ImGui::Text("Timings (ms)");
        if (g_leb.params.backend == BACKEND_CPU) {
            djgc_ticks(g_gl.clocks[CLOCK_SUBDIVISION_SPLIT], &cpuDt, &gpuDt);
            ImGui::Text("Subdivision (Split): %.3f", cpuDt * 1e3);
            djgc_ticks(g_gl.clocks[CLOCK_SUBDIVISION_MERGE], &cpuDt, &gpuDt);
            ImGui::Text("Subdivision (Merge): %.3f", cpuDt * 1e3);
        } else {
            djgc_ticks(g_gl.clocks[CLOCK_DISPATCHER], &cpuDt, &gpuDt);
            ImGui::Text("Dispatcher  :        %.3f (CPU) %.3f (GPU)", cpuDt * 1e3, gpuDt * 1e3);
            djgc_ticks(g_gl.clocks[CLOCK_SUBDIVISION_SPLIT], &cpuDt, &gpuDt);
            ImGui::Text("Subdivision (Split): %.3f (CPU) %.3f (GPU)", cpuDt * 1e3, gpuDt * 1e3);
            djgc_ticks(g_gl.clocks[CLOCK_SUBDIVISION_MERGE], &cpuDt, &gpuDt);
            ImGui::Text("Subdivision (Merge): %.3f (CPU) %.3f (GPU)", cpuDt * 1e3, gpuDt * 1e3);
            djgc_ticks(g_gl.clocks[CLOCK_SUM_REDUCTION], &cpuDt, &gpuDt);
            ImGui::Text("SumReduction:        %.3f (CPU) %.3f (GPU)", cpuDt * 1e3, gpuDt * 1e3);
        }
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


void resizeCallback(GLFWwindow* window, int width, int height)
{
    (void)window;

    g_window.width = width;
    g_window.height = height;
}


int main(int argc, char **argv)
{
    LOG("Loading {OpenGL Window}");
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, g_window.glversion.major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, g_window.glversion.minor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif
    g_window.handle = glfwCreateWindow(g_window.width,
                                       g_window.height,
                                       g_window.name,
                                       NULL, NULL);
    if (g_window.handle == NULL) {
        LOG("=> Failure <=");
        glfwTerminate();

        return -1;
    }
    glfwMakeContextCurrent(g_window.handle);

    // setup callbacks
    glfwSetWindowSizeCallback(g_window.handle, &resizeCallback);

    // load OpenGL functions
    LOG("Loading {OpenGL Functions}");
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG("=> Failure <=");
        glfwTerminate();

        return -1;
    }

    // initialize
    LOG("Loading {Demo}");
    if (!Load()) {
        LOG("=> Failure <=");
        glfwTerminate();

        return -1;
    }
    InitGui();

    while (!glfwWindowShouldClose(g_window.handle)) {
        glfwPollEvents();
        UpdateSubdivision();
        Draw();
        DrawGui();
        glfwSwapBuffers(g_window.handle);
    }

    Release();
    cbt_Release(g_leb.cbt);
    ReleaseGui();
    glfwTerminate();

    return 0;
}
