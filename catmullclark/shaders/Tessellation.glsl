layout(std140, column_major, binding = BUFFER_BINDING_XFORM)
uniform PerFrameVariables {
    mat4 u_ModelMatrix;
    mat4 u_ModelViewMatrix;
    mat4 u_ViewMatrix;
    mat4 u_CameraMatrix;
    mat4 u_ViewProjectionMatrix;
    mat4 u_ModelViewProjectionMatrix;
    vec4 u_FrustumPlanes[6];
};

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

uniform float u_LodFactor;


/*******************************************************************************
 * DecodeFaceVertices -- Computes the vertices of the face in mesh space
 *
 */
vec3[3] DecodeFaceVertices(const in cbt_Node node)
{
#if 0
    const cctt_Face face = cctt_DecodeFace(node);
    const int bisectionDepth = cctt_NodeBisectionDepth(node);
    const int ccDepth = 1 + (bisectionDepth >> 1);

    return vec3[3](
        ccs_HalfedgeVertexPoint(face.halfedgeIDs[0], ccDepth),
        ccs_HalfedgeVertexPoint(face.halfedgeIDs[1], ccDepth),
        ccs_HalfedgeVertexPoint(face.halfedgeIDs[2], ccDepth)
    );
#else
    const cct_Bisector bisector = cct_NodeToBisector(node);
    const int ccDepth = 1 + (bisector.depth >> 1);
    const int stride = (ccs_MaxDepth() - ccDepth) << 1;
    const cct_BisectorHalfedgeIDs halfedgeIDs = cct_DecodeHalfedgeIDs(bisector) << stride;

    return vec3[3](
        ccs_HalfedgeVertexPoint(int(halfedgeIDs[0]), ccs_MaxDepth()),
        ccs_HalfedgeVertexPoint(int(halfedgeIDs[1]), ccs_MaxDepth()),
        ccs_HalfedgeVertexPoint(int(halfedgeIDs[2]), ccs_MaxDepth())
    );
#endif
}


/*******************************************************************************
 * FrustumCullingTest -- Checks if the triangle lies inside the view frutsum
 *
 * This function depends on FrustumCulling.glsl
 *
 */
bool FrustumCullingTest(in const vec3[3] patchVertices)
{
    vec3 bmin = min(min(patchVertices[0], patchVertices[1]), patchVertices[2]);
    vec3 bmax = max(max(patchVertices[0], patchVertices[1]), patchVertices[2]);

    return FrustumCullingTest(u_FrustumPlanes, bmin, bmax);
}


/*******************************************************************************
 * BackfaceCullingTest -- Checks if the triangle is backfacing wrt the camera
 *
 */
bool BackfaceCullingTest(in const vec3[3] patchVertices)
{
    const vec3 e0 = patchVertices[1] - patchVertices[0];
    const vec3 e1 = patchVertices[2] - patchVertices[0];
    const vec3 faceNormal = cross(e1, e0);
    const vec3 viewDir = patchVertices[0] + patchVertices[1] + patchVertices[2];

    return dot(viewDir, faceNormal) >= 0.0f;
}


/*******************************************************************************
 * EdgeLevelOfDetail -- Computes the LoD assocaited to an edge
 *
 * This function is used to garantee a user-specific pixel edge length in
 * screen space. The vertices must be expressed in view space.
 * We compute the LoD as:
 *      LoD = 2 * log2(EdgePixelLength / TargetPixelLength)
 */
float EdgeLevelOfDetail_Perspective(in const vec3 v0, in const vec3 v1)
{
#if 0 //  human-readable version
    vec3 edgeCenter = (v1 + v0); // division by 2 was moved to u_LodFactor
    vec3 edgeVector = (v1 - v0);
    float distanceToEdgeSqr = dot(edgeCenter, edgeCenter);
    float edgeLengthSqr = dot(edgeVector, edgeVector);

    return u_LodFactor + log2(edgeLengthSqr / distanceToEdgeSqr);
#else // optimized version
    float sqrMagSum = dot(v0, v0) + dot(v1, v1);
    float twoDotAC = 2.0f * dot(v0, v1);
    float distanceToEdgeSqr = sqrMagSum + twoDotAC;
    float edgeLengthSqr     = sqrMagSum - twoDotAC;

    return u_LodFactor + log2(edgeLengthSqr / distanceToEdgeSqr);
#endif
}

/*******************************************************************************
 * TriangleLevelOfDetail -- Computes the LoD assocaited to a triangle
 *
 * This function is used to garantee a user-specific pixel edge length in
 * screen space. The reference edge length is that of the longest edge of the
 * input triangle.In practice, we compute the LoD as:
 *      LoD = 2 * log2(EdgePixelLength / TargetPixelLength)
 * where the factor 2 is because the number of segments doubles every 2
 * subdivision level.
 */
float TriangleLevelOfDetail_Perspective(in const vec3[3] faceVertices)
{
    const vec3 v[3] = vec3[3] (
        (u_ModelViewMatrix * vec4(faceVertices[0], 1.0f)).xyz,
        (u_ModelViewMatrix * vec4(faceVertices[1], 1.0f)).xyz,
        (u_ModelViewMatrix * vec4(faceVertices[2], 1.0f)).xyz
    );
    const float l0 = EdgeLevelOfDetail_Perspective(v[0], v[1]);
    const float l1 = EdgeLevelOfDetail_Perspective(v[1], v[2]);
    const float l2 = EdgeLevelOfDetail_Perspective(v[2], v[0]);

    if (BackfaceCullingTest(v)) {
        return max(l0, max(l1, l2));
    } else {
        return -1.0f;
    }
}


/*
    In Orthographic Mode, we have
        EdgePixelLength = EdgeViewSpaceLength / ImagePlaneViewSize * ImagePlanePixelResolution
    and so using some identities we get:
        LoD = 2 * (log2(EdgeViewSpaceLength)
            + log2(ImagePlanePixelResolution / ImagePlaneViewSize)
            - log2(TargetPixelLength))

            = log2(EdgeViewSpaceLength^2)
            + 2 * log2(ImagePlanePixelResolution / (ImagePlaneViewSize * TargetPixelLength))
    so we precompute:
    u_LodFactor = 2 * log2(ImagePlanePixelResolution / (ImagePlaneViewSize * TargetPixelLength))
*/
float TriangleLevelOfDetail_Orthographic(in const vec3[3] patchVertices)
{
    vec3 v0 = (u_ModelViewMatrix * vec4(patchVertices[0], 1.0f)).xyz;
    vec3 v2 = (u_ModelViewMatrix * vec4(patchVertices[2], 1.0f)).xyz;
    vec3 edgeVector = (v2 - v0);
    float edgeLengthSqr = dot(edgeVector, edgeVector);

    return u_LodFactor + log2(edgeLengthSqr);
}

float TriangleLevelOfDetail(in const vec3[3] faceVertices)
{
#if defined(PROJECTION_RECTILINEAR)
    return TriangleLevelOfDetail_Perspective(faceVertices);
#elif defined(PROJECTION_ORTHOGRAPHIC)
    return TriangleLevelOfDetail_Orthographic(faceVertices);
#else
    return 0.0;
#endif
}





/*******************************************************************************
 * LevelOfDetail -- Computes the level of detail of associated to a triangle
 *
 * The first component is the actual LoD value. The second value is 0 if the
 * triangle is culled, and one otherwise.
 *
 */
vec2 LevelOfDetail(in const vec3[3] faceVertices)
{
    // culling test
    if (!FrustumCullingTest(faceVertices))
#if FLAG_CULL
        return vec2(0.0f, 0.0f);
#else
        return vec2(0.0f, 1.0f);
#endif


    // compute triangle LOD
    return vec2(TriangleLevelOfDetail(faceVertices), 1.0f);
}



void main(void)
{
    // get threadID
    const int cbtID = 0;
    const uint threadID = gl_GlobalInvocationID.x;
    const int bisectorID = int(threadID);

    if (bisectorID < cct_BisectorCount(cbtID)) {
        // and extract triangle vertices
        const cbt_Node node = cbt_DecodeNode(cbtID, threadID);
        const cct_Bisector bisector = cct_NodeToBisector(node);
        const vec3 faceVertices[3] = DecodeFaceVertices(node);

        // compute target LoD
        const vec2 targetLod = LevelOfDetail(faceVertices);

        // splitting update
#if FLAG_SPLIT
        if (targetLod.x > 1.0) {
            cct_Split(cbtID, bisector);
        }
#endif

        // merging update
#if FLAG_MERGE
        if (true) {
            const cct_DiamondParent diamond = cct_DecodeDiamondParent(node);
            const bool shouldMergeBase = LevelOfDetail(DecodeFaceVertices(diamond.base)).x < 1.0;
            const bool shouldMergeTop = LevelOfDetail(DecodeFaceVertices(diamond.top)).x < 1.0;

            if (shouldMergeBase && shouldMergeTop) {
                cct_MergeNode(cbtID, node, diamond);
            }
        }
#endif
    }
}
