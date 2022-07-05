#ifndef CC_LOCAL_SIZE_X
#   define CC_LOCAL_SIZE_X 256
#endif

uniform sampler2D u_Dmap;
uniform sampler2DArray u_PtexSampler;
uniform float u_Scale = 0.0f;

layout (local_size_x = CC_LOCAL_SIZE_X,
        local_size_y = 1,
        local_size_z = 1) in;

void
DisplaceVertex(const int cageID, int vertexID, in const vec3 displacement)
{
    const int depth = ccs_MaxDepth(cageID);
    const int stride = ccs_CumulativeVertexCountAtDepth(cageID, depth-1);
    const int tmp = stride + vertexID;

    ccsu_VertexPointBuffers[cageID].vertexPoints[3*tmp+0]+= displacement.x;
    ccsu_VertexPointBuffers[cageID].vertexPoints[3*tmp+1]+= displacement.y;
    ccsu_VertexPointBuffers[cageID].vertexPoints[3*tmp+2]+= displacement.z;
}

void main()
{
    const int cageID = 0;
    const int depth = ccs_MaxDepth(cageID);
    const int vertexCount = ccm_VertexCountAtDepth(cageID, depth);
    const uint threadID = gl_GlobalInvocationID.x;
    const int vertexID = int(threadID);

    if (vertexID < vertexCount) {
        const int halfedgeID = ccs_VertexToHalfedgeID(cageID, vertexID, depth);
        const vec3 normal = ccs_HalfedgeNormal(cageID, halfedgeID);
        const int faceID = ccs_HalfedgeFaceID(cageID, halfedgeID, depth);
        const int ptexFaceID = faceID >> (2 * depth);
#if 1
        const vec2 uv = ccs_HalfedgeVertexUv(cageID, halfedgeID, depth);
        //const float dmap = (textureLod(u_Dmap, uv, 0.0).r) * 0.035f;
        const float dmap = (textureLod(u_PtexSampler, vec3(uv, ptexFaceID), 0.0).r) * 0.1f;
#else
        const vec3 pos = ccs_VertexPosition(cageID, vertexID) * 5.0f;
        const float dmap = (cos(pos.x) * cos(pos.y) * cos(pos.z) * 0.5f + 0.5f) * 0.2f;
#endif

        DisplaceVertex(cageID, vertexID, dmap * normal);
    }
}
