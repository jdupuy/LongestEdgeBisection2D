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

uniform vec2 u_ScreenResolution;
uniform int u_Depth;

void main()
{
    const int meshID = 0;
    const int faceID = gl_PrimitiveIDIn;
    const int firstHalfedgeID = ccm_FaceToHalfedgeID_Quad(faceID);
    const int lastHalfedgeID = firstHalfedgeID + 3;
    vec4 centerVertex = vec4(0, 0, 0, 1);

    for (int halfedgeID = firstHalfedgeID;
             halfedgeID <= lastHalfedgeID;
             ++halfedgeID) {
        centerVertex.xyz+= ccs_HalfedgeVertexPoint(halfedgeID, u_Depth);
    }

    centerVertex.xyz/= 4.0f;

    const vec3 p0 = ccs_HalfedgeVertexPoint(firstHalfedgeID, u_Depth);
    const vec3 p1 = ccs_HalfedgeVertexPoint(lastHalfedgeID, u_Depth);
    const vec3 n = normalize(cross(p0 - centerVertex.xyz, p1 - centerVertex.xyz));
    o_Normal = (u_ModelViewMatrix * vec4(n, 0.0f)).xyz;

    for (int halfedgeID = firstHalfedgeID;
             halfedgeID <= lastHalfedgeID;
             ++halfedgeID) {
        const int nextID = ccm_HalfedgeNextID_Quad(halfedgeID);
        const vec3 vertexPoint = ccs_HalfedgeVertexPoint(halfedgeID, u_Depth);
        const vec3 nextVertexPoint = ccs_HalfedgeVertexPoint(nextID, u_Depth);
        const vec4 v1 = u_ModelViewProjectionMatrix * vec4(vertexPoint, 1.0f);
        const vec4 v2 = u_ModelViewProjectionMatrix * vec4(nextVertexPoint, 1.0f);
        const vec4 v3 = u_ModelViewProjectionMatrix * vec4(centerVertex.xyz, 1.0f);
        const vec2 p1 = u_ScreenResolution * v1.xy / v1.w;
        const vec2 p2 = u_ScreenResolution * v2.xy / v2.w;
        const vec2 p3 = u_ScreenResolution * v3.xy / v3.w;
        const vec2 v[3] = vec2[3](p3 - p2, p3 - p1, p2 - p1);
        const float area = abs(v[1].x * v[2].y - v[1].y * v[2].x);

        o_Distance = vec3(area * inversesqrt(dot(v[0], v[0])), 0.0f, 0.0f);
        gl_Position = v1;
        EmitVertex();

        o_Distance = vec3(0.0f, area * inversesqrt(dot(v[1], v[1])), 0.0f);
        gl_Position = v2;
        EmitVertex();

        o_Distance = vec3(0.0f, 0.0f, area * inversesqrt(dot(v[2], v[2])));
        gl_Position = v3;
        EmitVertex();

        EndPrimitive();
    }
}
#endif

#ifdef FRAGMENT_SHADER
layout(location = 0) in vec3 i_Normal;
layout(location = 1) noperspective in vec3 i_Distance;

layout(location = 0) out vec4 o_FragColor;

void main()
{
    const float wireScale = 1.0; // scale of the wire in pixel
    const vec3 wireColor = vec3(0.40,0.40,0.40);
    const vec3 distanceSquared = i_Distance * i_Distance;
    const float nearestDistance = distanceSquared.z;//min(distanceSquared.y, distanceSquared.z);
    const float blendFactor = exp2(-nearestDistance / wireScale);
    vec3 baseColor = vec3(0.96, 0.96, 0.96);

    const vec3 normal = normalize(i_Normal);
    const float ks = pow(clamp(-normal.z, 0.0, 1.0), 300.0) * 0.5f;
    const float kd = clamp(-normal.z, 0.0, 1.0) * 0.75f;

    baseColor = baseColor * (ks + kd);

#if FLAG_WIRE
    o_FragColor = vec4(mix(baseColor, wireColor, blendFactor), 1.0f);
#else
    o_FragColor = vec4(baseColor, 1.0f);
#endif
}
#endif
