uniform mat4 u_CameraMatrix;
uniform mat4 u_ViewProjectionMatrix;
uniform vec3 u_SunDir;

vec3 Inverse(vec3 x) {return x / dot(x, x);}
vec3 StereographicProjection(vec3 x, vec3 center) {
    return 2.0f * Inverse(x - center) + center;
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

#ifdef VERTEX_SHADER
layout(location = 0) in vec4 i_VertexPostion;
layout(location = 0) out vec3 o_ViewDir;
void main()
{
    vec3 pos = i_VertexPostion.xyz * 50000.0;
    vec3 worldPos = (u_CameraMatrix * vec4(pos, 1)).xyz;

#if defined(PROJECTION_FISHEYE)
    vec3 screenPos = WorldToScreenSpace(worldPos);

    screenPos.xy*= 1.0f * vec2(1.0f, 1920.0f / 720.0f);
    gl_Position = vec4(screenPos, 1.0f);
    gl_Position.z = 0.99;
#else
    gl_Position = u_ViewProjectionMatrix * vec4(worldPos, 1.0);
    //gl_Position.z = gl_Position.w * 0.999999;
#endif

    o_ViewDir = normalize(worldPos - u_CameraMatrix[3].xyz);
}
#endif

#ifdef FRAGMENT_SHADER
layout(location = 0) in vec3 i_ViewDir;
layout(location = 0) out vec4 o_FragColor;

void main()
{
    vec3 worldSunDir = u_SunDir.zxy;
    vec3 worldCamera = u_CameraMatrix[3].zxy;
    vec3 v = normalize(i_ViewDir).zxy;

    vec3 sunColor = step(cos(3.1415926f / 180.0f), dot(v, worldSunDir))
                  * vec3(SUN_INTENSITY);
    vec3 extinction;
    vec3 inscatter = skyRadiance(worldCamera + earthPos, v, worldSunDir, extinction);
    vec3 finalColor = sunColor * extinction + inscatter;

    o_FragColor.rgb = finalColor;
    o_FragColor.a = 1.0;
    //o_FragColor = vec4(0, 0, 0, 1);
}
#endif
