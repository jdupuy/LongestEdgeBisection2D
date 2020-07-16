/* render.glsl - public domain
by Jonathan Dupuy

    This code has dependencies on the following GLSL sources:
    - TerrainRenderCommon.glsl
    - LongestEdgeBisection.glsl
*/

#ifdef COMPUTE_SHADER
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

void main(void)
{
    // get threadID
    const int cbtID = 0;
    uint threadID = gl_GlobalInvocationID.x;

    if (threadID < cbt_NodeCount(cbtID)) {
        // and extract triangle vertices
        cbt_Node node = cbt_DecodeNode(cbtID, threadID);
        vec4 triangleVertices[3] = DecodeTriangleVertices(node);

        // compute target LoD
        vec2 targetLod = LevelOfDetail(triangleVertices);

        // splitting update
#if FLAG_SPLIT
        if (targetLod.x > 1.0) {
            //leb_SplitNodeConforming_Quad(lebID, node);
            leb_SplitNode_Square(cbtID, node);
        }
#endif

#if FLAG_MERGE
        if (true) {
            leb_DiamondParent diamond = leb_DecodeDiamondParent_Square(node);
            bool shouldMergeBase = LevelOfDetail(DecodeTriangleVertices(diamond.base)).x < 1.0;
            bool shouldMergeTop = LevelOfDetail(DecodeTriangleVertices(diamond.top)).x < 1.0;

            if (shouldMergeBase && shouldMergeTop) {
                leb_MergeNode_Square(cbtID, node, diamond);
            }
        }
#endif
    }
}
#endif

