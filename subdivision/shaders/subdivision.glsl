// requires cbt.glsl and leb.glsl
#ifndef CBT_LOCAL_SIZE_X
#   define CBT_LOCAL_SIZE_X 256
#endif
uniform int u_CbtID = 0;
uniform vec2 u_TargetPosition;

layout (local_size_x = CBT_LOCAL_SIZE_X,
        local_size_y = 1,
        local_size_z = 1) in;

float Wedge(vec2 a, vec2 b)
{
    return a.x * b.y - a.y * b.x;
}

bool IsInside(mat2x3 faceVertices)
{
    vec2 v1 = vec2(faceVertices[0][0], faceVertices[1][0]);
    vec2 v2 = vec2(faceVertices[0][1], faceVertices[1][1]);
    vec2 v3 = vec2(faceVertices[0][2], faceVertices[1][2]);
    float w1 = Wedge(v2 - v1, u_TargetPosition - v1);
    float w2 = Wedge(v3 - v2, u_TargetPosition - v2);
    float w3 = Wedge(v1 - v3, u_TargetPosition - v3);
    vec3 w = vec3(w1, w2, w3);
    bvec3 wb = greaterThanEqual(w, vec3(0.0f));

    return all(wb);
}

mat2x3 DecodeFaceVertices(cbt_Node node)
{
    mat2x3 faceVertices = mat2x3(vec3(0, 0, 1), vec3(1, 0, 0));

#if defined(MODE_TRIANGLE)
    faceVertices = leb_DecodeNodeAttributeArray       (node, faceVertices);
#elif defined(MODE_SQUARE)
    faceVertices = leb_DecodeNodeAttributeArray_Square(node, faceVertices);
#endif

    return faceVertices;
}

void main()
{
    const int cbtID = u_CbtID;
    uint threadID = gl_GlobalInvocationID.x;

    if (threadID < cbt_NodeCount(cbtID)) {
        cbt_Node node = cbt_DecodeNode(cbtID, threadID);

#if FLAG_SPLIT
        mat2x3 faceVertices = DecodeFaceVertices(node);

        if (IsInside(faceVertices)) {
#if defined(MODE_TRIANGLE)
            leb_SplitNode(cbtID, node);
#elif defined(MODE_SQUARE)
            leb_SplitNode_Square(cbtID, node);
#endif
        }
#endif

#if FLAG_MERGE
#if defined(MODE_TRIANGLE)
        leb_DiamondParent diamondParent = leb_DecodeDiamondParent(node);
#elif defined(MODE_SQUARE)
        leb_DiamondParent diamondParent = leb_DecodeDiamondParent_Square(node);
#endif
        mat2x3 baseFaceVertices = DecodeFaceVertices(diamondParent.base);
        mat2x3 topFaceVertices = DecodeFaceVertices(diamondParent.top);

        if (!IsInside(baseFaceVertices) && !IsInside(topFaceVertices)) {
#if defined(MODE_TRIANGLE)
            leb_MergeNode(cbtID, node, diamondParent);
#elif defined(MODE_SQUARE)
            leb_MergeNode_Square(cbtID, node, diamondParent);
#endif
        }
#endif
    }
}
