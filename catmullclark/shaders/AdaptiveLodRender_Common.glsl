uniform sampler2D u_Albedo;
uniform sampler2D u_Nmap;
uniform sampler2D u_Ao;
uniform sampler2D u_Amap;


/*******************************************************************************
 * ShadeFragment -- Fragement shading routine
 *
 */
#ifdef FRAGMENT_SHADER
vec4
#if FLAG_WIRE
ShadeFragment(
    in const vec3 position,
    in const vec2 uv,
    in const vec3 normal,
    in const vec3 wo,
    in const vec3 distance
)
#else
ShadeFragment(
    in const vec3 position,
    in const vec2 uv,
    in const vec3 normal,
    in const vec3 wo
)
#endif
{
#if FLAG_WIRE
#if SHADING_SHADED
    const vec3 wireColor = vec3(0.0f);
#else
    const vec3 wireColor = vec3(0.40f, 0.40f, 0.40f);
#endif
    const float wireScale = 1.0f; // scale of the wire in pixel
    const vec3 distanceSquared = distance * distance;
    const float nearestDistance = min(distanceSquared.x, min(distanceSquared.y, distanceSquared.z));
    const float blendFactor = exp2(-nearestDistance / wireScale);
#endif

#if SHADING_COLOR
    const vec3 color = vec3(0.96,0.96,0.96);

#   if FLAG_WIRE
    return vec4(mix(color, wireColor, blendFactor), 1.0f);
#   else
    return vec4(color, 1.0f);
#   endif

#elif SHADING_UVS
    const vec3 color = clamp(vec3(uv, 0.0f), 0.0f, 1.0f);

#   if FLAG_WIRE
    return vec4(mix(color, wireColor, blendFactor), 1.0f);
#   else
    return vec4(color, 1.0f);
#   endif

#elif SHADING_NORMALS
    const vec3 color = clamp(normal, 0.0f, 1.0f);

#   if FLAG_WIRE
    return vec4(mix(color, wireColor, blendFactor), 1.0f);
#   else
    return vec4(color, 1.0f);
#   endif

#elif SHADING_SHADED
    const vec3 baseColor = vec3(0.96,0.96,0.96);
    const float ks = pow(clamp(normal.z, 0.0, 1.0), 300.0) * 0.5f;
    const float kd = clamp(normal.z, 0.0, 1.0) * 0.75f;
    const vec3 shading = baseColor * (ks + kd);

#   if FLAG_WIRE
    return vec4(mix(shading, wireColor, blendFactor), 1.0f);
#   else
    return vec4(shading, 1.0f);
#   endif
#else
    return vec4(1, 0, 0, 1);
#endif
}
#endif
