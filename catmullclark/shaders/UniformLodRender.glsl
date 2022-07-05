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

uniform int u_LevelOfDetail;

void EvenRule(inout ivec3 x, int b)
{
    if (b == 0) {
        x[2] = (x[0] | 2);
        x[1] = (x[0] | 1);
        x[0] = (x[0] | 0);
    } else {
        x[0] = (x[2] | 2);
        x[1] = (x[2] | 3);
        x[2] = (x[2] | 0);
    }
}

void OddRule(inout ivec3 x, int b)
{
    if (b == 0) {
        x[2] = (x[1] << 2) | 0;
        x[1] = (x[0] << 2) | 2;
        x[0] = (x[0] << 2) | 0;
    } else {
        x[0] = (x[1] << 2) | 0;
        x[1] = (x[1] << 2) | 2;
        x[2] = (x[2] << 2) | 0;
    }
}

ivec3 DecodeFace(int faceID, int levelOfDetail)
{
    const int baseHalfedgeID = faceID >> levelOfDetail;
    ivec3 face = ivec3(4 * baseHalfedgeID,
                       4 * baseHalfedgeID + 2,
                       4 * ccm_HalfedgeNextID(baseHalfedgeID));
    int pingPong = 0;

    for (int bitID = levelOfDetail - 1; bitID >= 0; --bitID) {
        const int bitValue = (faceID >> bitID) & 1;

        if ((pingPong & 1) == 0) {
            EvenRule(face, bitValue);
        } else {
            OddRule(face, bitValue);
        }
        pingPong^= 1;
    }

    // swap winding for odd faces
    if ((levelOfDetail & 1) == 0) {
        const int tmp = face[0];

        face[0] = face[2];
        face[2] = tmp;
    }

    return face;
}

#ifdef VERTEX_SHADER
void main()
{
    const int meshID = 0;
    const int faceID = gl_VertexID / 3;
    const int faceVertexID = gl_VertexID % 3;
    const int levelOfDetail = u_LevelOfDetail;
    const int ccDepth = 1 + (levelOfDetail >> 1);
    const ivec3 face = DecodeFace(faceID, levelOfDetail);
    const int halfedgeID = face[faceVertexID];
    const vec3 vertexPoint = ccs_HalfedgeVertexPoint(halfedgeID, ccDepth);

    gl_Position = vec4(vertexPoint, 1.0f);
}
#endif

#ifdef GEOMETRY_SHADER
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;
layout(location = 0) out vec3 o_Normal;
layout(location = 1) noperspective out vec3 o_Distance;

uniform vec2 u_ScreenResolution;

void main()
{
    const vec4 w0 = gl_in[0].gl_Position;
    const vec4 w1 = gl_in[1].gl_Position;
    const vec4 w2 = gl_in[2].gl_Position;
    const vec4 c0 = u_ModelViewProjectionMatrix * w0;
    const vec4 c1 = u_ModelViewProjectionMatrix * w1;
    const vec4 c2 = u_ModelViewProjectionMatrix * w2;
    const vec2 p0 = u_ScreenResolution * c0.xy / c0.w;
    const vec2 p1 = u_ScreenResolution * c1.xy / c1.w;
    const vec2 p2 = u_ScreenResolution * c2.xy / c2.w;
    const vec2 v[3] = vec2[3](p2 - p1, p2 - p0, p1 - p0);
    const float area = abs(v[1].x * v[2].y - v[1].y * v[2].x);
    const vec3 n = normalize(cross(w2.xyz - w0.xyz, w1.xyz - w0.xyz));

    o_Normal = (u_ModelViewMatrix * vec4(n, 0.0f)).xyz;

    for (int i = 0; i < 3; ++i) {
        o_Distance = vec3(0);
        o_Distance[i] = area * inversesqrt(dot(v[i],v[i]));
        gl_Position = u_ModelViewProjectionMatrix * gl_in[i].gl_Position;
        EmitVertex();
    }

    EndPrimitive();
}
#endif

#ifdef FRAGMENT_SHADER
layout(location = 0) in vec3 i_Normal;
layout(location = 1) noperspective in vec3 i_Distance;
layout(location = 0) out vec4 o_FragColor;
void main()
{
    const float wireScale = 1.0; // scale of the wire in pixel
    const vec3 baseColor = vec3(0.96,0.96,0.96);
    const vec3 wireColor = vec3(0.40,0.40,0.40);
    const vec3 distanceSquared = i_Distance * i_Distance;
    const float nearestDistance = min(distanceSquared.x, min(distanceSquared.y, distanceSquared.z));
    const float blendFactor = exp2(-nearestDistance / wireScale);
    const vec3 normal = normalize(i_Normal);
    const float ks = pow(clamp(-normal.z, 0.0, 1.0), 300.0) * 0.5f;
    const float kd = clamp(-normal.z, 0.0, 1.0) * 0.75f;
    const vec3 shading = baseColor * (ks + kd);

#if FLAG_WIRE
    o_FragColor = vec4(mix(shading, wireColor, blendFactor), 1.0f);
#else
    o_FragColor = vec4(shading, 1.0f);
#endif
}
#endif
