/* render.glsl - public domain
    (created by Jonathan Dupuy)

    This code has dependencies on the following GLSL sources:
    - leb.glsl
*/

// -----------------------------------------------------------------------------
/**
 * Vertex Shader
 *
 * The vertex shader is empty
 */
#ifdef VERTEX_SHADER
void main()
{ }
#endif

// -----------------------------------------------------------------------------
/**
 * Tessellation Control Shader
 *
 * This tessellaction control shader is responsible for updating the
 * subdivision buffer and sending geometry to the rasterizer.
 */
#ifdef TESS_CONTROL_SHADER
layout (vertices = 1) out;
out Patch {
    vec4 vertices[3];
} o_Patch[];

void main()
{
    // get threadID and decode triangle vertices
    const int cbtID = 0;
    uint threadID = uint(gl_PrimitiveID);
    cbt_Node node = cbt_DecodeNode(cbtID, threadID);
    vec4 v[3] = DecodeTriangleVertices(node);

    // perform frustum culling
    bool isVisible = FrustumCullingTest(v);

    // set tess levels
    gl_TessLevelInner[0] =
    gl_TessLevelInner[1] =
    gl_TessLevelOuter[0] =
    gl_TessLevelOuter[1] =
    gl_TessLevelOuter[2] = 1;

    // set output data
    o_Patch[gl_InvocationID].vertices = v;
    o_Patch[gl_InvocationID].vertices[0].w = isVisible ? 1.0 : 0.0;

}
#endif

// -----------------------------------------------------------------------------
/**
 * Tessellation Evaluation Shader
 *
 * This tessellaction evaluation shader is responsible for placing the
 * geometry properly on the input mesh (here a terrain).
 */
#ifdef TESS_EVALUATION_SHADER
layout (triangles, ccw, equal_spacing) in;
in Patch {
    vec4 vertices[3];
} i_Patch[];

layout(location = 0) flat out int o_IsVisible;

void main()
{
    vec4 v[3] = i_Patch[0].vertices;
    vec4 finalVertex = BarycentricInterpolation(v, gl_TessCoord.xy);

    gl_Position = vec4(finalVertex.xy * 2.0 - 1.0, -1.0, 1.0);
    o_IsVisible = v[0].w > 0.0 ? 1 : 0;
}
#endif

#ifdef GEOMETRY_SHADER
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;
layout (location = 0) flat in int i_IsVisible[];
layout (location = 0) flat out int o_IsVisible;
layout (location = 1) noperspective out vec3 o_Distance;

void main()
{
    vec2 p0 = 800.0 * gl_in[0].gl_Position.xy;
    vec2 p1 = 800.0 * gl_in[1].gl_Position.xy;
    vec2 p2 = 800.0 * gl_in[2].gl_Position.xy;
    vec2 v[3] = vec2[3](p2 - p1, p2 - p0, p1 - p0);
    float area = abs(v[1].x * v[2].y - v[1].y * v[2].x);

    o_IsVisible = i_IsVisible[0];

    for (int i = 0; i < 3; ++i) {
        o_Distance = vec3(0);
        o_Distance[i] = area * inversesqrt(dot(v[i],v[i]));
        gl_Position = gl_in[i].gl_Position;
        //gl_Position.z = -1.0;
        EmitVertex();
    }
    EndPrimitive();
}
#endif

#ifdef FRAGMENT_SHADER
layout (location = 0) flat in int i_IsVisible;
layout (location = 1) noperspective in vec3 i_Distance;
layout (location = 0) out vec4 o_FragColor;

void main()
{
#if 0
    vec4 wire = i_IsVisible > 0 ? vec4(0.00,0.20,0.70, 1.0)
                                : vec4(0.40,0.40,0.40,1.0);
    vec4 color = i_IsVisible > 0 ? vec4(0.75,0.80,0.93, 1.0)
                                 : vec4(0.85,0.85,0.85, 1.0);
#else
    vec4 wire = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 color = vec4(0.85,0.85,0.85, 1.0);
#endif
    const float wirescale = 1.5; // scale of the wire
    vec3 d2 = i_Distance * i_Distance;
    float nearest = min(min(d2.x, d2.y), d2.z);
    float f = exp2(-nearest / wirescale);

    o_FragColor = mix(color, wire, f);
}
#endif
