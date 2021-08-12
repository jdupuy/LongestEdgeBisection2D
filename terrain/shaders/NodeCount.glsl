// requires cbt.glsl
uniform int u_CbtID = 0;
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(std430, binding = CBT_NODE_COUNT_BUFFER_BINDING)
buffer CbtNodeCount {
    uint u_CbtNodeCount;
};

void main()
{
    const int cbtID = u_CbtID;

    u_CbtNodeCount = cbt_NodeCount(cbtID);
}
