/* render.glsl - public domain
by Jonathan Dupuy

    This code has dependencies on the following GLSL sources:
    - TerrainRenderCommon.glsl
    - leb.glsl
*/

#define THREAD_COUNT 32

layout(std430, binding = BUFFER_BINDING_MESHLET_VERTICES)
readonly buffer MeshletVertexBuffer {
    vec2 u_MeshletVertexBuffer[];
};

layout(std430, binding = BUFFER_BINDING_MESHLET_INDEXES)
readonly buffer MeshletIndexBuffer {
    uint16_t u_MeshletIndexBuffer[];
};

/*******************************************************************************
 * Task Shader
 *
 * This task shader is responsible for updating the
 * LEB buffer and sending geometry to the mesh shader.
 */
#ifdef TASK_SHADER
layout(local_size_x = THREAD_COUNT) in;

taskNV out Patch{
    vec4 packedData[4 * THREAD_COUNT];
} o_Patch;

void main()
{
    uint nodeID = gl_GlobalInvocationID.x;
    bool isVisible = false;
    vec4 triangleVertices[3];
    leb_Node node;

    if (nodeID < leb_NodeCount()) {
        // get threadID (each triangle is associated to a thread)
        // and extract triangle vertices
        node = leb_DecodeNode(nodeID);
        triangleVertices = DecodeTriangleVertices(node);

        // compute target LoD
        vec2 targetLod = LevelOfDetail(triangleVertices);

        // splitting pass
#if FLAG_SPLIT
        if (targetLod.x > 1.0)
            leb_SplitNodeConforming(node);
#endif

        // merging pass
#if FLAG_MERGE
        if (true) {
            leb_NodeDiamond diamond = leb_DecodeNodeDiamond(node);
            bool shouldMergeBase = LevelOfDetail(DecodeTriangleVertices(diamond.base)).x < 1.0;
            bool shouldMergeTop = LevelOfDetail(DecodeTriangleVertices(diamond.top)).x < 1.0;

            if (shouldMergeBase && shouldMergeTop)
                leb_MergeNodeConforming(node, diamond);
        }
#endif

        // schedule at most 32 mesh shaders from first thread
        isVisible = (targetLod.y > 0.0);
    }

    uint voteVisible = ballotThreadNV(isVisible);

    // write varying data
    if (isVisible) {
        uint patchID = 4u * bitCount(voteVisible & gl_ThreadLtMaskNV);

        // set output data
        o_Patch.packedData[patchID    ] =
            u_ModelViewProjectionMatrix * vec4(triangleVertices[0].xy, 0.0f, 1.0f);
        o_Patch.packedData[patchID + 1] =
            u_ModelViewProjectionMatrix * vec4(triangleVertices[1].xy, 0.0f, 1.0f);
        o_Patch.packedData[patchID + 2] =
            u_ModelViewProjectionMatrix * vec4(triangleVertices[2].xy, 0.0f, 1.0f);
        o_Patch.packedData[patchID + 3] =
            vec4(triangleVertices[0].xy, triangleVertices[1].xy);

        // reverse triangle winding depending on the subdivision level
        if ((node.depth & 1) == 0) {
            vec4 tmp = o_Patch.packedData[patchID];

            o_Patch.packedData[patchID] = o_Patch.packedData[patchID + 2];
            o_Patch.packedData[patchID + 2] = tmp;
            o_Patch.packedData[patchID + 3].xy = triangleVertices[2].xy;
        }
    }

    // schedule mesh shaders
    if (gl_LocalInvocationID.x == 0u) {
        gl_TaskCountNV = bitCount(voteVisible);
    }
}
#endif


/*******************************************************************************
 * Mesh Shader
 *
 * This mesh shader is responsible for placing the geometry properly on
 * the input mesh.
 *
 */
#ifdef MESH_SHADER
layout(local_size_x = THREAD_COUNT) in;
layout(max_vertices = MAX_VERTICES, max_primitives = MAX_PRIMITIVES) out;
layout(triangles) out;

taskNV in Patch{
    vec4 packedData[4 * THREAD_COUNT];
} i_Patch;

out FragmentData {
    vec2 texCoord;
    vec2 slope;
} o_FragmentData[];

int EdgeVertexCount()
{
    return (1 << TERRAIN_PATCH_SUBD_LEVEL) + 1;
}

int PrimitiveCount()
{
    return 1 << (TERRAIN_PATCH_SUBD_LEVEL << 1);
}

int VertexCount()
{
    return (EdgeVertexCount() * (EdgeVertexCount() + 1)) / 2;
}


void main()
{
    // unpack triangle attributes
    uint patchID = 4u * gl_WorkGroupID.x;
    vec4 trianglePositions[3] = vec4[3](
        i_Patch.packedData[patchID    ],
        i_Patch.packedData[patchID + 1],
        i_Patch.packedData[patchID + 2]
    );
    vec2 tmp = i_Patch.packedData[patchID + 3].xy
             - i_Patch.packedData[patchID + 3].zw;
    vec2 triangleTexCoords[3] = vec2[3](
        i_Patch.packedData[patchID + 3].xy,
        i_Patch.packedData[patchID + 3].zw,
        i_Patch.packedData[patchID + 3].zw + vec2(tmp.y, -tmp.x)
    );

#if 0
    // output triangle
    gl_PrimitiveCountNV = 1;
    vec2 tessCoords[3] = vec2[3](vec2(0, 1), vec2(0, 0), vec2(1, 0));

    // vertex buffer
    for (int i = 0; i < 3; ++i) {
        int vertexID = i;
        // compute final vertex attributes
        ClipSpaceAttribute attrib = TessellateClipSpaceTriangle(
            trianglePositions,
            triangleTexCoords,
            tessCoords[i]
        );

        // set outputs
        o_FragmentData[vertexID].texCoord = attrib.texCoord;
        o_FragmentData[vertexID].slope    = attrib.slope;
        gl_MeshVerticesNV[vertexID].gl_Position = attrib.position;

        for (int j = 0; j < 6; ++j)
            gl_MeshVerticesNV[vertexID].gl_ClipDistance[j] = 1.0f;
    }

    // index buffer
    for (int i = 0; i < 3; ++i) {
        gl_PrimitiveIndicesNV[i] = i;
    }
#else

    // output triangle
    int edgeVertexCount = EdgeVertexCount();
    int edgeCount = edgeVertexCount - 1;
    gl_PrimitiveCountNV = PrimitiveCount();

    // push vertices
    for (uint i = 0; i < edgeVertexCount; ++i) {
        for (uint j = 0; j < i + 1; ++j) {
            uint vertexID = i * (i + 1) / 2 + j;
            vec2 tessCoord = vec2(j, edgeCount - i) / float(edgeCount);

            // compute final vertex attributes
            ClipSpaceAttribute attrib = TessellateClipSpaceTriangle(
                trianglePositions,
                triangleTexCoords,
                tessCoord
            );

            // set outputs
            o_FragmentData[vertexID].texCoord = attrib.texCoord;
            o_FragmentData[vertexID].slope    = attrib.slope;
            gl_MeshVerticesNV[vertexID].gl_Position = attrib.position;

            for (int j = 0; j < 6; ++j)
                gl_MeshVerticesNV[vertexID].gl_ClipDistance[j] = 1.0f;
        }
    }

    // push indexes
    for (uint i = 0; i < edgeVertexCount - 1; ++i) {
        uint indexID = 3 * i * i;

        for (int j = 0; j < i; ++j) {
            gl_PrimitiveIndicesNV[indexID    ] = (i + 1) * (i + 2) / 2 + j;
            gl_PrimitiveIndicesNV[indexID + 1] = (i + 1) * (i + 2) / 2 + j + 1;
            gl_PrimitiveIndicesNV[indexID + 2] = (i    ) * (i + 1) / 2 + j;
            gl_PrimitiveIndicesNV[indexID + 3] = gl_PrimitiveIndicesNV[indexID + 2];
            gl_PrimitiveIndicesNV[indexID + 4] = gl_PrimitiveIndicesNV[indexID + 1];
            gl_PrimitiveIndicesNV[indexID + 5] = gl_PrimitiveIndicesNV[indexID + 2] + 1;
            indexID+= 6;
        }

        gl_PrimitiveIndicesNV[indexID    ] = (i + 1) * (i + 2) / 2 + i;
        gl_PrimitiveIndicesNV[indexID + 1] = (i + 2) * (i + 3) / 2 - 1;
        gl_PrimitiveIndicesNV[indexID + 2] = (i + 1) * (i + 2) / 2 - 1;
    }
#endif
}

#endif


/*******************************************************************************
 * Fragment Shader -- Shades the terrain
 *
 */
#ifdef FRAGMENT_SHADER
in FragmentData {
    vec2 texCoord;
    vec2 slope;
} i_FragmentData;

layout(location = 0) out vec4 o_FragColor;

void main()
{
    o_FragColor = ShadeFragment(i_FragmentData.texCoord, i_FragmentData.slope);
}
#endif
