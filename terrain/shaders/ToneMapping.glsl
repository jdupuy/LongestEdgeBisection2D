/* ToneMapping.glsl - public domain tone mapping GLSL code
by Jonathan Dupuy

*/

//////////////////////////////////////////////////////////////////////////////
//
// Tone mapping API
//
vec3 HdrToLdr(vec3 hdr);

//
//
//// end header file /////////////////////////////////////////////////////


/*******************************************************************************
 * Uncharted2Tonemap -- Internal tone mapping function from Uncharted 2
 *
 */
vec3 tm__Uncharted2Tonemap(const vec3 x)
{
    const float A = 0.15f;
    const float B = 0.50f;
    const float C = 0.10f;
    const float D = 0.20f;
    const float E = 0.02f;
    const float F = 0.30f;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

/*******************************************************************************
 * Uncharted2 -- Uncharted 2 tone mapping function
 *
 */
vec3 tm__Uncharted2(const vec3 hdr)
{
    const float W = 11.2;
    const float exposureBias = 2.0;
    vec3 curr = tm__Uncharted2Tonemap(exposureBias * hdr);
    vec3 whiteScale = 1.0 / tm__Uncharted2Tonemap(vec3(W));
    return curr * whiteScale;
}

/*******************************************************************************
 * Filmic -- Uncharted 2 tone mapping function
 *
 * See Filmic Tonemapping Operators http://filmicgames.com/archives/75
 *
 */
vec3 tm__Filmic(const vec3 hdr)
{
    vec3 x = max(vec3(0.0f), hdr - 0.004f);
    return (x * (6.2f * x + 0.5f)) / (x * (6.2f * x + 1.7f) + 0.06f);
}

/*******************************************************************************
 * AcesFilm -- Aces tone mapping function
 *
 * See https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
 *
 */
vec3 tm__AcesFilm(const vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d ) + e), 0.0, 1.0);
}

/*******************************************************************************
 * AcesFilm -- Aces tone mapping function
 *
 * See https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
 *
 */
vec3 tm__Reinhard(const vec3 hdr)
{
    return hdr / (hdr + vec3(1.0));
}

/*******************************************************************************
 * HdrToLdr -- Main tone mapping function
 *
 * User is responsible for defining his tonemapping macro.
 * If none is specified, the function does nothing.
 *
 */
vec3 HdrToLdr(vec3 hdr)
{
#if defined TONEMAP_UNCHARTED2
    return tm__Uncharted2(hdr);
#elif defined TONEMAP_FILMIC
    return tm__Filmic(hdr);
#elif defined TONEMAP_ACES
    return tm__AcesFilm(hdr);
#elif defined TONEMAP_REINHARD
    return tm__Reinhard(hdr);
#else
    return hdr;
#endif
}
