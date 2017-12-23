#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define WINDOWS_LEAN_AND_MEAN // Please, not too much windows shenanigans
#endif
#include "glm/glm.hpp"
#include "glm/gtx/string_cast.hpp"
#include "util.h"
#include "util_common.h"
#include "mex.h"
#include <algorithm>

// Limitations
size_t GPU_global_mem;

void trianglesToMemory(const float *mxtriangles, float* triangles, size_t size)
{
	// Loop over all triangles and place them in memory
    memcpy(triangles, mxtriangles, size);
}

extern void voxelize(const voxinfo & v, float* triangle_data, unsigned int* vtable, bool morton_code);

void mexFunction(int nlhs, mxArray *plhs[],
                 int nrhs, mxArray const *prhs[])
{

    if (nrhs!=3) 
    {
        mexErrMsgIdAndTxt("PMLS:cudavox:invalid_input", "cudavoxmex expects 3 input argument");
    }
    size_t size = mxGetNumberOfElements(prhs[0]);
    size_t nfaces = size / 9;
    size *= sizeof(float);
	mexPrintf("Number of faces to voxelize: %llu\n", nfaces);
    const float *mxtriangles = (float *)(mxGetData(prhs[0]));
	float* triangles = reinterpret_cast<float*>(mxMalloc(size));
	if (triangles==0)
		mexErrMsgIdAndTxt("PMLS:cudavox:no_memory", "cudavoxmex no memory");
	trianglesToMemory(mxtriangles, triangles, size);
    int *bb = (int *)(mxGetData(prhs[1]));
    AABox<glm::ivec3> bbox_mesh(glm::ivec3(bb[0],bb[1],bb[2]),glm::ivec3(bb[3],bb[4],bb[5]));
	voxinfo v(bbox_mesh, nfaces);
	size_t vtable_size = size_t(v.gridsizex >> 1) * size_t(v.gridsizey >> 1) * size_t(v.gridsizez >> 1) ;
	unsigned int* vtable = reinterpret_cast<unsigned int*>(mxMalloc(vtable_size));
	memset(vtable, 0, vtable_size);
	mexPrintf("\n## CPU VOXELISATION \n");
	voxelize(v, triangles, vtable, false);
    {
        AABox<glm::ivec3> ioutbox;
		/*
		int minx;
		int miny;
		int minz;
		minx = miny = minz = v.gridsize;
		int maxx;
		int maxy;
		int maxz;
		maxx = maxy = maxz = -1;
		int igrsiz = int(v.gridsize);
		for (int z = 0; z < igrsiz; z++)
		{
			for (int y = 0; y < igrsiz; y++)
			{
				for (int x = 0; x < igrsiz; x++)
				{
					if (checkVoxel(x, y, z, v.gridsize, vtable))
					{
						if (x < minx)
							minx = x;
						if (y < miny)
							miny = y;
						if (z < minz)
							minz = z;
						if (x > maxx)
							maxx = x;
						if (y > maxy)
							maxy = y;
						if (z > maxz)
							maxz = z;
					}
				}
			}
		}
*/
		ioutbox.min = bbox_mesh.min - v.bbox.min;
		ioutbox.min -= glm::ivec3(5, 5, 5);
		ioutbox.max = bbox_mesh.max - v.bbox.min;
		ioutbox.max += glm::ivec3(5, 5, 5);
//		ioutbox.min = v.bbox.min - v.bbox.min;
//		ioutbox.max = v.bbox.max - v.bbox.min;
		ioutbox.min.x = std::max(0, ioutbox.min.x);
		ioutbox.min.y = std::max(0, ioutbox.min.y);
		ioutbox.min.z = std::max(0, ioutbox.min.z);
		ioutbox.max.x = std::min(int(v.gridsizex), ioutbox.max.x);
		ioutbox.max.y = std::min(int(v.gridsizey), ioutbox.max.y);
		ioutbox.max.z = std::min(int(v.gridsizez), ioutbox.max.z);
		glm::ivec3 outgridsize(ioutbox.max - ioutbox.min);
        glm::ivec3 outtranslate(ioutbox.min + v.bbox.min);
		mwSize dims[3];
		dims[0] = outgridsize.x;
		dims[1] = outgridsize.y;
		dims[2] = outgridsize.z;

        plhs[0] = mxCreateLogicalArray(3, dims);
        mxLogical *result = mxGetLogicals(plhs[0]);
        for (size_t z = ioutbox.min.z; z < ioutbox.max.z; z++)
        {
            for (size_t y = ioutbox.min.y; y < ioutbox.max.y; y++)
            {
                for (size_t x = ioutbox.min.x; x < ioutbox.max.x; x++)
                {
					if (checkVoxel(x, y, z, v.gridsizex, v.gridsizey, v.gridsizez, vtable))
                        *result = 1;
                    ++result;
    			}
        	}
        }
        plhs[1] = mxCreateDoubleMatrix(1, 3, mxREAL);
        double  *mxtr = mxGetPr(plhs[1]);
        mxtr[0] = outtranslate.x;
        mxtr[1] = outtranslate.y;
        mxtr[2] = outtranslate.z;
        

    }
	mxFree(triangles); // managed memory
	triangles = 0;

   	mxFree(vtable); // managed memory
    vtable = 0;

 
}
