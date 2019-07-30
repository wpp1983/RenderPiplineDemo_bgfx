 #include "bgfx_compute.sh"

BUFFER_RO(ClusterAABBMinPoint, vec4, 0);
BUFFER_RO(ClusterAABBMaxPoint, vec4, 1);
BUFFER_RW(lightGridOffset, uint, 2);
BUFFER_RW(lightGridCount, uint, 3);
BUFFER_RW(globalLightIndexList, uint, 4);
BUFFER_RO(pointLight_position, vec4, 5);
BUFFER_RO(pointLight_color, vec4, 6);
BUFFER_RW(globalIndexCount, uint, 8);


uniform vec4 lightCount;

bool testSphereAABB(uint light, uint tile);
float sqDistPointAABB(vec3 pointLightInfo, uint tile);

NUM_THREADS(1,1,1)
void main(){


if ( gl_GlobalInvocationID.x == 0 )
{
    globalIndexCount[0] = 0;
}


    uint clusterIndex1D = gl_GlobalInvocationID.x;
    
    uint visibleLightCount = 0;
    uint visibleLightIndices[1024];


        for( uint lightIndex = 0; lightIndex < lightCount[0]; ++lightIndex)
        {
                if( testSphereAABB(lightIndex, clusterIndex1D))
                {
                   visibleLightIndices[visibleLightCount] = lightIndex;
                   visibleLightCount += 1;
                }
        }


     uint offset = 0;
     atomicFetchAndAdd(globalIndexCount[0], visibleLightCount, offset);

    for(uint i = 0; i < visibleLightCount; ++i){
        globalLightIndexList[offset + i] = visibleLightIndices[i];
    }

    lightGridOffset[clusterIndex1D] = offset;
    lightGridCount[clusterIndex1D] = visibleLightCount;
}

bool testSphereAABB(uint light, uint tile){
    float radius = pointLight_position[light].w;
    vec3 lightWorldPos = pointLight_position[light].xyz;
    vec3 lightViewPos = mul(u_view, vec4(lightWorldPos, 1.0)).xyz;
    float squaredDistance =  sqDistPointAABB(lightViewPos, tile);

    return squaredDistance <= (radius * radius);
}

float sqDistPointAABB(vec3 pointLightInfo, uint tile){

    float sqDist = 0.0;
    vec4 minPoint = ClusterAABBMinPoint[tile];
    vec4 maxPoint = ClusterAABBMaxPoint[tile];
    for(int i = 0; i < 3; ++i){
        float v = pointLightInfo[i];
        if(v < minPoint[i]){
            sqDist += (minPoint[i] - v) * (minPoint[i] - v);
        }
        if(v > maxPoint[i]){
            sqDist += (v - maxPoint[i]) * (v - maxPoint[i]);
        }
    }

    return sqDist;
}