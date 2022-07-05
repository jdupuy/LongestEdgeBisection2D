// requires cbt.glsl
uniform int u_CbtID = 0;
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = CCT_DISPATCHER_BUFFER_BINDING)
buffer DispatchIndirectCommandBuffer {
    uint u_CctDispatchBuffer[];
};

void main()
{
    const int cbtID = u_CbtID;
    uint bisectorCount = cct_BisectorCount(cbtID);

    u_CctDispatchBuffer[0] = bisectorCount * 3u;
}
