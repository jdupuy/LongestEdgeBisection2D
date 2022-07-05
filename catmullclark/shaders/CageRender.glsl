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


#ifdef VERTEX_SHADER
void main()
{

}
#endif

#ifdef GEOMETRY_SHADER
layout (points) in;
layout (triangle_strip, max_vertices = 24) out;
layout(location = 0) out vec3 o_Normal;
layout(location = 1) noperspective out vec3 o_Distance;
layout(location = 2) flat out int o_IsNonQuad;
layout(location = 3) flat out int o_IsBoundary;
layout(location = 4) flat out int o_IsSharp;
layout(location = 5) out vec3 o_Position;

uniform vec2 u_ScreenResolution;

void main()
{
    const int faceID = gl_PrimitiveIDIn;
    const int firstHalfedgeID = ccm_FaceToHalfedgeID(faceID);
    const int lastHalfedgeID = ccm_HalfedgePrevID(firstHalfedgeID);
    const int vertexCount = lastHalfedgeID - firstHalfedgeID + 1;
    vec4 centerVertex = vec4(0, 0, 0, 1);

    for (int halfEdgeID = firstHalfedgeID;
             halfEdgeID <= lastHalfedgeID;
             ++halfEdgeID) {
        const int vertexID = ccm_HalfedgeVertexID(halfEdgeID);

        centerVertex.xyz+= ccm_VertexPoint(vertexID);
    }

    centerVertex.xyz/= float(lastHalfedgeID - firstHalfedgeID + 1);
    o_IsNonQuad = (vertexCount == 4) ? 0 : 1;

#if 0
    o_IsExtraordinary = 0;
    for (int halfEdgeID = firstHalfedgeID;
             halfEdgeID <= lastHalfedgeID && o_IsExtraordinary == 0;
             ++halfEdgeID) {
        int valence = 1;

        for (int it = ccm_NextVertexHalfedgeID(halfEdgeID);
             it >= 0 && it != halfEdgeID;
             it = ccm_NextVertexHalfedgeID(halfEdgeID)) {
            ++valence;
        }

        o_IsExtraordinary|= (valence == 4) ? 0 : 1;
    }
#endif

    const vec3 p0 = ccm_HalfedgeVertexPoint(firstHalfedgeID);
    const vec3 p1 = ccm_HalfedgeVertexPoint(lastHalfedgeID);
    const vec3 n = normalize(cross(p0 - centerVertex.xyz, p1 - centerVertex.xyz));
    o_Normal = (u_ModelViewMatrix * vec4(n, 0.0f)).xyz;

    for (int halfEdgeID = firstHalfedgeID;
             halfEdgeID <= lastHalfedgeID;
             ++halfEdgeID) {
        const int nextFaceHalfedgeID = ccm_HalfedgeNextID(halfEdgeID);
        const vec3 vertexPoint = ccm_HalfedgeVertexPoint(halfEdgeID);
        const vec3 nextVertexPoint = ccm_HalfedgeVertexPoint(nextFaceHalfedgeID);
        const vec4 v1 = u_ModelViewProjectionMatrix * vec4(vertexPoint, 1.0f);
        const vec4 v2 = u_ModelViewProjectionMatrix * vec4(nextVertexPoint, 1.0f);
        const vec4 v3 = u_ModelViewProjectionMatrix * vec4(centerVertex.xyz, 1.0f);
        const vec2 p1 = u_ScreenResolution * v1.xy / v1.w;
        const vec2 p2 = u_ScreenResolution * v2.xy / v2.w;
        const vec2 p3 = u_ScreenResolution * v3.xy / v3.w;
        const vec2 v[3] = vec2[3](p3 - p2, p3 - p1, p2 - p1);
        const float area = abs(v[1].x * v[2].y - v[1].y * v[2].x);

        o_IsBoundary = ccm_HalfedgeTwinID(halfEdgeID) < 0 ? 1 : 0;
        o_IsSharp = ccm_HalfedgeSharpness(halfEdgeID) > 0.0f ? 1 : 0;
        o_Distance = vec3(area * inversesqrt(dot(v[0], v[0])), 0.0f, 0.0f);
        gl_Position = v1;
        o_Position = vertexPoint;
        EmitVertex();

        o_Distance = vec3(0.0f, area * inversesqrt(dot(v[1], v[1])), 0.0f);
        gl_Position = v2;
        o_Position = nextVertexPoint;
        EmitVertex();

        o_Distance = vec3(0.0f, 0.0f, area * inversesqrt(dot(v[2], v[2])));
        gl_Position = v3;
        o_Position = centerVertex.xyz;
        EmitVertex();

        EndPrimitive();
    }
}
#endif

#ifdef FRAGMENT_SHADER
layout(location = 0) in vec3 i_Normal;
layout(location = 1) noperspective in vec3 i_Distance;
layout(location = 2) flat in int i_IsNonQuad;
layout(location = 3) flat in int i_IsBoundary;
layout(location = 4) flat in int i_IsSharp;
layout(location = 5) in vec3 i_Position;

layout(location = 0) out vec4 o_FragColor;
void main()
{
    vec3 baseColor = vec3(0.96,0.96,0.96);
    vec3 wireColor = vec3(0.40,0.40,0.40);
    float wireScale = 1.0f; // scale of the wire in pixel
    vec3 distanceSquared = i_Distance * i_Distance;
    float nearestDistance = distanceSquared.z;//min(distanceSquared.x, min(distanceSquared.y, distanceSquared.z));
    const vec3 normal = normalize(i_Normal);
    const float ks = pow(clamp(-normal.z, 0.0, 1.0), 300.0) * 0.5f;
    const float kd = clamp(-normal.z, 0.0, 1.0) * 0.75f;

    vec3 P = i_Position;

    baseColor = baseColor * (ks + kd) * 1.0 + 0.1;

#if 1
    if (i_IsNonQuad == 1) {
        baseColor = vec3(0.60f, 1.0f, 0.75f) * (kd * 0.5 + 0.5);
    }

    if (i_IsBoundary == 1) {
        wireColor = vec3(1.0f, 0.0f, 0.1f);
        wireScale = 4.1f;
    }
    else if (i_IsSharp == 1) {
        wireColor = vec3(1.0f, 0.85f, 0.0f);
        wireScale = 4.1f;
    }
#endif


#if FLAG_WIRE
    float blendFactor = exp2(-nearestDistance / wireScale);

    o_FragColor = vec4(mix(baseColor, wireColor, blendFactor), 1.0f);
#else
    o_FragColor = vec4(baseColor, 1.0f);
#endif
}
#endif
