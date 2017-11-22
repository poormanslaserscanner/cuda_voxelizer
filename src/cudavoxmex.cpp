#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define WINDOWS_LEAN_AND_MEAN // Please, not too much windows shenanigans
#endif
#include "glm/glm.hpp"
#include "glm/gtx/string_cast.hpp"
#include "util.h"
#include "util_cuda.h"
#include "util_common.h"
#include "gpu/mxGPUArray.h"
#include "mex.h"
#include <algorithm>
//double gridsize_mm = 50.0;

// Program data
//float* triangles;
//unsigned int* vtable;

// Limitations
size_t GPU_global_mem;
// Helper function to transfer triangles to automatically managed CUDA memory
void trianglesToMemory(const float *mxtriangles, float* triangles, size_t size)
{
	// Loop over all triangles and place them in memory
   	mexPrintf("Copy %llu to CUDA-managed memory \n", size / 1024);

    memcpy(triangles, mxtriangles, size);

}
// Forward declaration of CUDA calls
extern void voxelize(const voxinfo & v, float* triangle_data, unsigned int* vtable, bool morton_code);

void mexFunction(int nlhs, mxArray *plhs[],
                 int nrhs, mxArray const *prhs[])
{
    mexPrintf("\n## CUDA INIT \n");
    mxInitGPU();
    mexPrintf("\n## CUDA INIT MATLAB DONE\n");

    /* Throw an error if the input is not a GPU array. */
    if (nrhs!=3) 
    {
        mexErrMsgIdAndTxt("PMLS:cudavox:invalid_input", "cudavoxmex expects 3 input argument");
    }
//    int gridsize_mm = (int)(mxGetScalar(prhs[2]));
    size_t size = mxGetNumberOfElements(prhs[0]);
    size_t nfaces = size / 9;
    size *= sizeof(float);
	mexPrintf("Number of faces: %llu\n", nfaces);

    const float *mxtriangles = (float *)(mxGetData(prhs[0]));
	
	checkCudaRequirements();
	mexPrintf("\n## MEMORY PREPARATION \n");
	mexPrintf("Allocating %llu kB of CUDA-managed memory \n", (size_t)(size / 1024.0f));
	float* triangles;
	HANDLE_CUDA_ERROR(cudaMallocManaged((void**) &triangles, size)); // managed memory
	mexPrintf("Copy %llu triangles to CUDA-managed memory \n", nfaces);
	trianglesToMemory(mxtriangles, triangles, size);

	mexPrintf("\n## VOXELISATION SETUP \n");
    int *bb = (int *)(mxGetData(prhs[1]));
    AABox<glm::ivec3> bbox_mesh(glm::ivec3(bb[0],bb[1],bb[2]),glm::ivec3(bb[3],bb[4],bb[5]));
	voxinfo v(createMeshBBCube<glm::ivec3>(bbox_mesh), nfaces);
	v.print();
	size_t vtable_size = (size_t(v.gridsize) >> 1 ) * (size_t(v.gridsize) >> 1) * (size_t(v.gridsize) >> 1);
	mexPrintf("Allocating %llu kB of CUDA-managed memory for voxel table \n", size_t(vtable_size / 1024.0f));
	unsigned int* vtable;
	HANDLE_CUDA_ERROR(cudaMallocManaged((void **)&vtable, vtable_size));
//	HANDLE_CUDA_ERROR(cudaMemset((void *)vtable, 0, vtable_size));
	memset(vtable, 0, vtable_size);

	mexPrintf("\n## GPU VOXELISATION \n");
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
		ioutbox.max.x = std::min(int(v.gridsize), ioutbox.max.x);
		ioutbox.max.y = std::min(int(v.gridsize), ioutbox.max.y);
		ioutbox.max.z = std::min(int(v.gridsize), ioutbox.max.z);
		glm::ivec3 outgridsize(ioutbox.max - ioutbox.min);
        glm::ivec3 outtranslate(ioutbox.min + v.bbox.min);
        mwSize dims[3] = {outgridsize.x, outgridsize.y, outgridsize.z};
        
        plhs[0] = mxCreateLogicalArray(3, dims);
        mxLogical *result = mxGetLogicals(plhs[0]);
        for (size_t z = ioutbox.min.z; z < ioutbox.max.z; z++)
        {
            for (size_t y = ioutbox.min.y; y < ioutbox.max.y; y++)
            {
                for (size_t x = ioutbox.min.x; x < ioutbox.max.x; x++)
                {
                    if ( checkVoxel(x, y, z, v.gridsize, vtable) )
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
	HANDLE_CUDA_ERROR(cudaFree(triangles)); // managed memory
	triangles = 0;

   	HANDLE_CUDA_ERROR(cudaFree(vtable)); // managed memory
    vtable = 0;

 
}
