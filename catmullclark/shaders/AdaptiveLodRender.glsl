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
#line 11

vec3 BarycentricInterpolation(in vec3 v[3], in vec2 u)
{
    return v[1] + u.x * (v[2] - v[1]) + u.y * (v[0] - v[1]);
}

vec2 BarycentricInterpolation(in vec2 v[3], in vec2 u)
{
    return v[1] + u.x * (v[2] - v[1]) + u.y * (v[0] - v[1]);
}

vec3[3]
DecodeFaceVertices(
    in const cbt_Node node,
    out vec3[3] faceNormals,
    out vec2[3] faceUvs
) {
    const cct_Bisector bisector = cct_NodeToBisector(node);
    const int ccDepth = 1 + (bisector.depth >> 1);
    const int stride = (ccs_MaxDepth() - ccDepth) << 1;
    const cct_BisectorHalfedgeIDs halfedgeIDs = cct_DecodeHalfedgeIDs(bisector) << stride;

    faceNormals = vec3[3](
        ccs_HalfedgeNormal(int(halfedgeIDs[0])),
        ccs_HalfedgeNormal(int(halfedgeIDs[1])),
        ccs_HalfedgeNormal(int(halfedgeIDs[2]))
    );

    faceUvs = vec2[3](
        ccs_HalfedgeVertexUv(int(halfedgeIDs[0]), ccs_MaxDepth()),
        ccs_HalfedgeVertexUv(int(halfedgeIDs[1]), ccs_MaxDepth()),
        ccs_HalfedgeVertexUv(int(halfedgeIDs[2]), ccs_MaxDepth())
    );

    return vec3[3](
        ccs_HalfedgeVertexPoint(int(halfedgeIDs[0]), ccs_MaxDepth()),
        ccs_HalfedgeVertexPoint(int(halfedgeIDs[1]), ccs_MaxDepth()),
        ccs_HalfedgeVertexPoint(int(halfedgeIDs[2]), ccs_MaxDepth())
    );
}


#ifdef VERTEX_SHADER
layout(location = 0) out vec2 o_Uv;
layout(location = 1) out vec3 o_Normal;
layout(location = 2) out vec3 o_Position;

void main()
{
    const int cbtID = 0;
    const int faceID = gl_VertexID / 3;
    const int vertexID = gl_VertexID % 3;
    const cbt_Node node = cbt_DecodeNode(cbtID, faceID);
    vec3 faceNormals[3];
    vec2 faceUvs[3];
    const vec3[3] faceVertices = DecodeFaceVertices(node, faceNormals, faceUvs);
    const vec2 u = vec2(vertexID & 1, (vertexID >> 1) & 1);
    const vec3 vertexPosition = BarycentricInterpolation(faceVertices, u);
    const vec3 vertexNormal = BarycentricInterpolation(faceNormals, u);
    const vec2 vertexUv = BarycentricInterpolation(faceUvs, u);

    o_Position = (u_ModelMatrix * vec4(vertexPosition, 1.0f)).xyz;
    o_Uv = vertexUv;
    o_Normal = normalize((u_ModelMatrix * vec4(vertexNormal, 0.0f)).xyz);

    gl_Position = u_ModelViewProjectionMatrix * vec4(vertexPosition, 1.0f);
}
#endif

#ifdef FRAGMENT_SHADER
layout(location = 0) in vec2 i_Uv;
layout(location = 1) in vec3 i_Normal;
layout(location = 2) in vec3 i_Position;
layout(location = 0) out vec4 o_FragColor;
void main()
{
    const vec3 position = i_Position;
    const vec3 wo = normalize(u_CameraMatrix[3].xyz - position);
    const vec3 normal = normalize(i_Normal);
    const vec2 uv = i_Uv;

    o_FragColor = ShadeFragment(position, uv, normal, wo);
}
#endif
