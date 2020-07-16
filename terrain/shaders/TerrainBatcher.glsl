// this shader sets the indirect drawing commands
#ifdef COMPUTE_SHADER

//#pragma optionNV(unroll none)

uniform int u_CbtID = 0;

layout(std430, binding = BUFFER_BINDING_DRAW_ARRAYS_INDIRECT_COMMAND)
buffer DrawArraysIndirectCommandBuffer {
    uint u_DrawArraysIndirectCommand[];
};

#if FLAG_MS
layout(std430, binding = BUFFER_BINDING_DRAW_MESH_TASKS_INDIRECT_COMMAND)
buffer DrawMeshTasksIndirectCommandBuffer {
    uint u_DrawMeshTasksIndirectCommand[];
};
#endif

#if FLAG_CS
layout(std430, binding = BUFFER_BINDING_DRAW_ELEMENTS_INDIRECT_COMMAND)
buffer DrawElementsIndirectCommandBuffer {
    uint u_DrawElementsIndirectCommand[];
};
layout(std430, binding = BUFFER_BINDING_DISPATCH_INDIRECT_COMMAND)
buffer DispatchIndirectCommandBuffer {
    uint u_DispatchIndirectCommand[];
};
#endif

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
    const int cbtID = u_CbtID;
    uint nodeCount = cbt_NodeCount(cbtID);

    u_DrawArraysIndirectCommand[0] = nodeCount;

#if FLAG_MS
    u_DrawMeshTasksIndirectCommand[0] = max(1, (nodeCount >> 5) + 1);
#endif

#if FLAG_CS
    u_DispatchIndirectCommand[0] = nodeCount / 256u + 1u;
    u_DrawElementsIndirectCommand[0] = MESHLET_INDEX_COUNT;
    u_DrawElementsIndirectCommand[1] = nodeCount;
#endif
}

#endif
