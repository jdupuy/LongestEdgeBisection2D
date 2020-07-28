
/*******************************************************************************
 * Uniform Data -- Global variables for terrain rendering
 *
 */
layout(std140, column_major, binding = BUFFER_BINDING_TERRAIN_VARIABLES)
uniform PerFrameVariables {
    mat4 u_ModelMatrix;
    mat4 u_ModelViewMatrix;
    mat4 u_ViewMatrix;
    mat4 u_CameraMatrix;
    mat4 u_ViewProjectionMatrix;
    mat4 u_ModelViewProjectionMatrix;
    vec4 u_FrustumPlanes[6];
};

uniform float u_TargetEdgeLength;
uniform float u_LodFactor;
#if FLAG_DISPLACE
uniform sampler2D u_DmapSampler;
uniform sampler2D u_SmapSampler;
uniform sampler2D u_DmapRockSampler;
uniform sampler2D u_SmapRockSampler;
uniform float u_DmapFactor;
uniform float u_MinLodVariance;
#endif


/*******************************************************************************
 * DecodeTriangleVertices -- Decodes the triangle vertices in local space
 *
 */
vec4[3] DecodeTriangleVertices(in const cbt_Node node)
{
    vec3 xPos = vec3(0, 0, 1), yPos = vec3(1, 0, 0);
    mat2x3 pos = leb_DecodeNodeAttributeArray_Square(node, mat2x3(xPos, yPos));
    vec4 p1 = vec4(pos[0][0], pos[1][0], 0.0, 1.0);
    vec4 p2 = vec4(pos[0][1], pos[1][1], 0.0, 1.0);
    vec4 p3 = vec4(pos[0][2], pos[1][2], 0.0, 1.0);

#if FLAG_DISPLACE
    p1.z = u_DmapFactor * texture(u_DmapSampler, p1.xy).r;
    p2.z = u_DmapFactor * texture(u_DmapSampler, p2.xy).r;
    p3.z = u_DmapFactor * texture(u_DmapSampler, p3.xy).r;
#endif

    return vec4[3](p1, p2, p3);
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
float TriangleLevelOfDetail_Perspective(in const vec4[3] patchVertices)
{
    vec3 v0 = (u_ModelViewMatrix * patchVertices[0]).xyz;
    vec3 v2 = (u_ModelViewMatrix * patchVertices[2]).xyz;

#if 0 //  human-readable version
    vec3 edgeCenter = (v0 + v2); // division by 2 was moved to u_LodFactor
    vec3 edgeVector = (v2 - v0);
    float distanceToEdgeSqr = dot(edgeCenter, edgeCenter);
    float edgeLengthSqr = dot(edgeVector, edgeVector);

    return u_LodFactor + log2(edgeLengthSqr / distanceToEdgeSqr);
#else // optimized version
    float sqrMagSum = dot(v0, v0) + dot(v2, v2);
    float twoDotAC = 2.0f * dot(v0, v2);
    float distanceToEdgeSqr = sqrMagSum + twoDotAC;
    float edgeLengthSqr     = sqrMagSum - twoDotAC;

    return u_LodFactor + log2(edgeLengthSqr / distanceToEdgeSqr);
#endif
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
float TriangleLevelOfDetail_Orthographic(in const vec4[3] patchVertices)
{
    vec3 v0 = (u_ModelViewMatrix * patchVertices[0]).xyz;
    vec3 v2 = (u_ModelViewMatrix * patchVertices[2]).xyz;
    vec3 edgeVector = (v2 - v0);
    float edgeLengthSqr = dot(edgeVector, edgeVector);

    return u_LodFactor + log2(edgeLengthSqr);
}

vec3 Inverse(vec3 x) {return x / dot(x, x);}
vec3 StereographicProjection(vec3 x, vec3 center) {
    return 2.0f * Inverse(x - center) + center;
}
vec3 StereographicProjection(vec3 x) {
    const vec3 center = vec3(0.0f, 0.0f, 1.0f);

    return StereographicProjection(x, center);
}
vec3 ViewSpaceToScreenSpace(vec3 x)
{
    // project onto unit sphere
    float nrmSqr = dot(x, x);
    float nrm = inversesqrt(nrmSqr);
    vec3 xNrm = x * nrm;

    // project onto screen
    vec2 xNdc = StereographicProjection(xNrm).xy;
    return vec3(xNdc, nrmSqr);
}
vec3 WorldToScreenSpace(vec3 x)
{
    // project onto unit sphere
    vec3 camPos = u_CameraMatrix[3].xyz;
    vec3 xStd = x - camPos;
    float nrm = sqrt(dot(xStd, xStd));
    vec3 xStdNrm = xStd / nrm;

    // project onto screen
    vec3 camX = u_CameraMatrix[0].xyz;
    vec3 camY = u_CameraMatrix[1].xyz;
    vec3 camZ = u_CameraMatrix[2].xyz;
    vec3 tmp = StereographicProjection(xStdNrm, camZ);
    return vec3(dot(camX, tmp), dot(camY, tmp), nrm);
}

float TriangleLevelOfDetail_Fisheye(in const vec4[3] patchVertices)
{
    vec3 v0 = (u_ModelViewMatrix * patchVertices[0]).xyz;
    vec3 v2 = (u_ModelViewMatrix * patchVertices[2]).xyz;
    vec3 edgeVector = (v2 - v0);
    float edgeLengthSqr = dot(edgeVector, edgeVector);

    return u_LodFactor + log2(edgeLengthSqr);
}

float TriangleLevelOfDetail(in const vec4[3] patchVertices)
{
    vec3 v0 = (u_ModelViewMatrix * patchVertices[0]).xyz;
    vec3 v2 = (u_ModelViewMatrix * patchVertices[2]).xyz;
#if defined(PROJECTION_RECTILINEAR)
    return TriangleLevelOfDetail_Perspective(patchVertices);
#elif defined(PROJECTION_ORTHOGRAPHIC)
    return TriangleLevelOfDetail_Orthographic(patchVertices);
#elif defined(PROJECTION_FISHEYE)
    return TriangleLevelOfDetail_Perspective(patchVertices);
#else
    return 0.0;
#endif
}

#if FLAG_DISPLACE
/*******************************************************************************
 * DisplacementVarianceTest -- Checks if the height variance criteria is met
 *
 * Terrains tend to have locally flat regions, which don't need large amounts
 * of polygons to be represented faithfully. This function checks the
 * local flatness of the terrain.
 *
 */
bool DisplacementVarianceTest(in const vec4[3] patchVertices)
{
#define P0 patchVertices[0].xy
#define P1 patchVertices[1].xy
#define P2 patchVertices[2].xy
    vec2 P = (P0 + P1 + P2) / 3.0;
    vec2 dx = (P0 - P1);
    vec2 dy = (P2 - P1);
    vec2 dmap = textureGrad(u_DmapSampler, P, dx, dy).rg;
    float dmapVariance = clamp(dmap.y - dmap.x * dmap.x, 0.0, 1.0);

    return (dmapVariance >= u_MinLodVariance);
#undef P0
#undef P1
#undef P2
}
#endif

/*******************************************************************************
 * FrustumCullingTest -- Checks if the triangle lies inside the view frutsum
 *
 * This function depends on FrustumCulling.glsl
 *
 */
bool FrustumCullingTest(in const vec4[3] patchVertices)
{
    vec3 bmin = min(min(patchVertices[0].xyz, patchVertices[1].xyz), patchVertices[2].xyz);
    vec3 bmax = max(max(patchVertices[0].xyz, patchVertices[1].xyz), patchVertices[2].xyz);

    return FrustumCullingTest(u_FrustumPlanes, bmin, bmax);
}

/*******************************************************************************
 * LevelOfDetail -- Computes the level of detail of associated to a triangle
 *
 * The first component is the actual LoD value. The second value is 0 if the
 * triangle is culled, and one otherwise.
 *
 */
vec2 LevelOfDetail(in const vec4[3] patchVertices)
{
    // culling test
    if (!FrustumCullingTest(patchVertices))
#if FLAG_CULL
        return vec2(0.0f, 0.0f);
#else
        return vec2(0.0f, 1.0f);
#endif

#   if FLAG_DISPLACE
    // variance test
    if (!DisplacementVarianceTest(patchVertices))
        return vec2(0.0f, 1.0f);
#endif

    // compute triangle LOD
    return vec2(TriangleLevelOfDetail(patchVertices), 1.0f);
}


/*******************************************************************************
 * BarycentricInterpolation -- Computes a barycentric interpolation
 *
 */
vec2 BarycentricInterpolation(in vec2 v[3], in vec2 u)
{
    return v[1] + u.x * (v[2] - v[1]) + u.y * (v[0] - v[1]);
}

vec4 BarycentricInterpolation(in vec4 v[3], in vec2 u)
{
    return v[1] + u.x * (v[2] - v[1]) + u.y * (v[0] - v[1]);
}


/*******************************************************************************
 * GenerateVertex -- Computes the final vertex position
 *
 */
struct VertexAttribute {
    vec4 position;
    vec2 texCoord;
};

VertexAttribute TessellateTriangle(
    in const vec2 texCoords[3],
    in vec2 tessCoord
) {
    vec2 texCoord = BarycentricInterpolation(texCoords, tessCoord);
    vec4 position = vec4(texCoord, 0, 1);

#if FLAG_DISPLACE
    position.z = u_DmapFactor * textureLod(u_DmapSampler, texCoord, 0.0).r;
#endif

    return VertexAttribute(position, texCoord);
}


/*******************************************************************************
 * ShadeFragment -- Fragement shading routine
 *
 */
#ifdef FRAGMENT_SHADER
#if FLAG_WIRE
vec4 ShadeFragment(vec2 texCoord, vec3 worldPos, vec3 distance)
#else
vec4 ShadeFragment(vec2 texCoord, vec3 worldPos)
#endif
{
#if FLAG_WIRE
    const float wireScale = 1.1; // scale of the wire in pixel
    vec4 wireColor = vec4(0.0, 0.0, 0.0, 1.0);
    vec3 distanceSquared = distance * distance;
    float nearestDistance = min(min(distanceSquared.x, distanceSquared.y), distanceSquared.z);
    float blendFactor = exp2(-nearestDistance / wireScale);
#endif

#if FLAG_DISPLACE
#if 1
    // slope
    vec2 smap = texture(u_SmapSampler, texCoord).rg * u_DmapFactor * 0.03;
    vec3 n = normalize(vec3(-smap, 1));
#else // compute the slope from the dmap directly
    float filterSize = 1.0f / float(textureSize(u_DmapSampler, 0).x);// sqrt(dot(dFdx(texCoord), dFdy(texCoord)));
    float sx0 = textureLod(u_DmapSampler, texCoord - vec2(filterSize, 0.0), 0.0).r;
    float sx1 = textureLod(u_DmapSampler, texCoord + vec2(filterSize, 0.0), 0.0).r;
    float sy0 = textureLod(u_DmapSampler, texCoord - vec2(0.0, filterSize), 0.0).r;
    float sy1 = textureLod(u_DmapSampler, texCoord + vec2(0.0, filterSize), 0.0).r;
    float sx = sx1 - sx0;
    float sy = sy1 - sy0;

    vec3 n = normalize(vec3(u_DmapFactor * 0.03 / filterSize * 0.5f * vec2(-sx, -sy), 1));
#endif
#else
    vec3 n = vec3(0, 0, 1);
#endif

#if SHADING_SNOWY
    float d = clamp(n.z, 0.0, 1.0);
    float slopeMag = dot(n.xy, n.xy);
    vec3 albedo = slopeMag > 0.5 ? vec3(0.75) : vec3(2);
    float z = 3.0 * gl_FragCoord.z / gl_FragCoord.w;

    return vec4(mix(vec3(albedo * d / 3.14159), vec3(0.5), 1.0 - exp2(-z)), 1);
#elif SHADING_DIFFUSE
    vec3 wi = normalize(vec3(1, 1, 1));
    float d = dot(wi, n) * 0.5 + 0.5;
    vec3 albedo = vec3(252, 197, 150) / 255.0f;
    vec3 camPos = u_CameraMatrix[3].xyz;
    vec3 extinction;
    vec3 inscatter = inScattering(camPos.zxy + earthPos,
                                  worldPos.zxy + earthPos,
                                  wi.zxy,
                                  extinction);
#if FLAG_WIRE
    vec3 shading = mix((d / 3.14159) * albedo, wireColor.xyz, blendFactor);
#else
    vec3 shading = (d / 3.14159) * albedo;
#endif

    return vec4(shading * extinction + inscatter * 0.5, 1);
#elif SHADING_NORMALS

    return vec4(abs(n), 1);
#elif SHADING_COLOR

    return vec4(1, 1, 1, 1);
#else
    return vec4(1, 0, 0, 1);
#endif
}
#endif
