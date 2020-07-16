// requires cbt.glsl
uniform int u_CbtID = 0;
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(std430, binding = LEB_DISPATCHER_BUFFER_BINDING)
buffer DrawArraysIndirectCommandBuffer {
    uint u_DrawArraysIndirectBuffer[];
};

void main()
{
    const int cbtID = u_CbtID;
    uint nodeCount = cbt_NodeCount(cbtID);

    u_DrawArraysIndirectBuffer[1] = nodeCount;
}
