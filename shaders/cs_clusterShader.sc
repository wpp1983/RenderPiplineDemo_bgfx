 #include "bgfx_compute.sh"

struct Plane
{
    float3 N;   // Plane normal.
    float  d;   // Distance to origin.
};


BUFFER_WR(ClusterAABBMinPoint, vec4, 0);
BUFFER_WR(ClusterAABBMaxPoint, vec4, 1);

//Shared between all clusters
uniform vec4 cameraPlane;
uniform mat4 inverseProjection;
uniform vec4 ClusterCSize;
uniform vec4 GridDim;
uniform vec4 screenParam;

uint3 ComputeClusterIndex3D( uint clusterIndex1D )
{
    uint i = clusterIndex1D % GridDim.x;
    uint j = clusterIndex1D % ( GridDim.x * GridDim.y ) / GridDim.x;
    uint k = clusterIndex1D / ( GridDim.x * GridDim.y );

    return uint3( i, j, k );
}

// Convert clip space coordinates to view space
vec4 ClipToView( vec4 clip )
{
    // View space position.
    vec4 view = mul( inverseProjection, clip );
    // Perspecitive projection.
    view = view / view.w;

    return view;
}

// Convert screen space coordinates to view space.
vec4 ScreenToView( vec4 screen )
{
    // Convert to normalized texture coordinates in the range [0 .. 1].
    vec2 texCoord = screen.xy / screenParam.xy;

    // Convert to clip space
    vec4 clip = vec4( vec2( texCoord.x, 1.0f - texCoord.y ) * 2.0f - 1.0f, 1.0f, 1.0f );

    return ClipToView( clip );
}

/**
 * Find the intersection of a line segment with a plane.
 * This function will return true if an intersection point
 * was found or false if no intersection could be found.
 * Source: Real-time collision detection, Christer Ericson (2005)
 */
bool IntersectLinePlane( vec3 a, vec3 b, Plane p, out vec3 q )
{
    vec3 ab = b - a;

    float t = ( p.d - dot( p.N, a ) ) / dot( p.N, ab );

    bool intersect = ( t >= 0.0f && t <= 1.0f );

    q = vec3( 0, 0, 0 );
    if ( intersect )
    {
        q = a + t * ab;
    }

    return intersect;
}

NUM_THREADS(1024,1,1)
void main(){
    float ViewNear = cameraPlane[0];
    float NearK = cameraPlane[2];

    uint clusterIndex1D = gl_GlobalInvocationID.x;
    // Convert the 1D cluster index into a 3D index in the cluster grid.
    uint3 clusterIndex3D = ComputeClusterIndex3D( clusterIndex1D );


    // Compute the near and far planes for cluster K.
    Plane nearPlane = { 0.0f, 0.0f, 1.0f, ViewNear * pow( abs( NearK ), clusterIndex3D.z ) };
    Plane farPlane =  { 0.0f, 0.0f, 1.0f, ViewNear * pow( abs( NearK ), clusterIndex3D.z + 1 ) };

    // The top-left point of cluster K in screen space.
    vec4 pMin = vec4( clusterIndex3D.xy * ClusterCSize.xy, 1.0f, 1.0f );
    // The bottom-right point of cluster K in screen space.
    vec4 pMax = vec4( ( clusterIndex3D.xy + 1 ) * ClusterCSize.xy, 1.0f, 1.0f );

    // Transform the screen space points to view space.
    pMin = ScreenToView( pMin );
    pMax = ScreenToView( pMax );

    // Find the min and max points on the near and far planes.
    vec3 nearMin, nearMax, farMin, farMax;
    // Origin (camera eye position)
    vec3 eye = vec3( 0, 0, 0 );
    IntersectLinePlane( eye, (vec3)pMin, nearPlane, nearMin );
    IntersectLinePlane( eye, (vec3)pMax, nearPlane, nearMax );
    IntersectLinePlane( eye, (vec3)pMin, farPlane, farMin );
    IntersectLinePlane( eye, (vec3)pMax, farPlane, farMax );

    vec3 aabbMin = min( nearMin, min( nearMax, min( farMin, farMax ) ) );
    vec3 aabbMax = max( nearMin, max( nearMax, max( farMin, farMax ) ) );


    //Getting the 
    ClusterAABBMinPoint[clusterIndex1D]  = vec4(aabbMin , 0.0);
    ClusterAABBMaxPoint[clusterIndex1D]  = vec4(aabbMax , 0.0);
}
