/* FrustumCulling.glsl - public domain frustum culling GLSL code
    (Created by Jonathan Dupuy 2014.11.13)

*/
//////////////////////////////////////////////////////////////////////////////
//
// Frustum Culling API
//

bool FrustumCullingTest(mat4 mvp, vec3 bmin, vec3 bmax);
bool FrustumCullingTest(in const vec4 frustumPlanes[6], vec3 bmin, vec3 bmax);

//
//
//// end header file /////////////////////////////////////////////////////


// *****************************************************************************
// Frustum Implementation

/**
 * Extract Frustum Planes from MVP Matrix
 *
 * Based on "Fast Extraction of Viewing Frustum Planes from the World-
 * View-Projection Matrix", by Gil Gribb and Klaus Hartmann.
 * This procedure computes the planes of the frustum and normalizes
 * them.
 */
vec4[6] LoadFrustum(mat4 mvp)
{
    vec4 planes[6];

    for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 2; ++j) {
        planes[i*2+j].x = mvp[0][3] + (j == 0 ? mvp[0][i] : -mvp[0][i]);
        planes[i*2+j].y = mvp[1][3] + (j == 0 ? mvp[1][i] : -mvp[1][i]);
        planes[i*2+j].z = mvp[2][3] + (j == 0 ? mvp[2][i] : -mvp[2][i]);
        planes[i*2+j].w = mvp[3][3] + (j == 0 ? mvp[3][i] : -mvp[3][i]);
        planes[i*2+j]*= length(planes[i*2+j].xyz);
    }

    return planes;
}

/**
 * Negative Vertex of an AABB
 *
 * This procedure computes the negative vertex of an AABB
 * given a normal.
 * See the View Frustum Culling tutorial @ LightHouse3D.com
 * http://www.lighthouse3d.com/tutorials/view-frustum-culling/geometric-approach-testing-boxes-ii/
 */
vec3 NegativeVertex(vec3 bmin, vec3 bmax, vec3 n)
{
    bvec3 b = greaterThan(n, vec3(0));
    return mix(bmin, bmax, b);
}

/**
 * Frustum-AABB Culling Test
 *
 * This procedure returns true if the AABB is either inside, or in
 * intersection with the frustum, and false otherwise.
 * The test is based on the View Frustum Culling tutorial @ LightHouse3D.com
 * http://www.lighthouse3d.com/tutorials/view-frustum-culling/geometric-approach-testing-boxes-ii/
 */
bool FrustumCullingTest(in const vec4 planes[6], vec3 bmin, vec3 bmax)
{
    float a = 1.0f;

    for (int i = 0; i < 6 && a >= 0.0f; ++i) {
        vec3 n = NegativeVertex(bmin, bmax, planes[i].xyz);

        a = dot(vec4(n, 1.0f), planes[i]);
    }

    return (a >= 0.0);
}

bool FrustumCullingTest(mat4 mvp, vec3 bmin, vec3 bmax)
{
    return FrustumCullingTest(LoadFrustum(mvp), bmin, bmax);
}




