#include "cuda.h"
#include "cuda_runtime.h"
#include "device_launch_parameters.h"

#define GLM_FORCE_CUDA
#include <glm/glm.hpp>
#include <iostream>
#include "util_cuda.h"
#include "util_common.h"

// CUDA Global Memory variables
//__device__ size_t voxel_count = 0; // How many voxels did we count
//__device__ size_t triangles_seen_count = 0; // Sanity check

__constant__ uint32_t morton256_x[256];
__constant__ uint32_t morton256_y[256];
__constant__ uint32_t morton256_z[256];

// Encode morton code using LUT table
__device__ inline uint64_t mortonEncode_LUT(unsigned int x, unsigned int y, unsigned int z){
	uint64_t answer = 0;
	answer = morton256_z[(z >> 16) & 0xFF] |
		morton256_y[(y >> 16) & 0xFF] |
		morton256_x[(x >> 16) & 0xFF];
	answer = answer << 48 |
		morton256_z[(z >> 8) & 0xFF] |
		morton256_y[(y >> 8) & 0xFF] |
		morton256_x[(x >> 8) & 0xFF];
	answer = answer << 24 |
		morton256_z[(z)& 0xFF] |
		morton256_y[(y)& 0xFF] |
		morton256_x[(x)& 0xFF];
	return answer;
}

// Possible optimization: buffer bitsets (for now: too much overhead)
struct bufferedBitSetter{
	unsigned int* voxel_table;
	size_t current_int_location;
	unsigned int current_mask;

	__device__ __inline__ bufferedBitSetter(unsigned int* voxel_table, size_t index) :
		voxel_table(voxel_table), current_mask(0) {
		current_int_location = index / size_t(32);
	}

	__device__ __inline__ void setBit(size_t index){
		size_t new_int_location = index / size_t(32);
		if (current_int_location != new_int_location){
			flush();
			current_int_location = new_int_location;
		}
		unsigned int bit_pos = 31 - (unsigned int)(int(index) % 32);
		current_mask = current_mask | (1 << bit_pos);
	}

	__device__ __inline__ void flush(){
		if (current_mask != 0){
			atomicOr(&(voxel_table[current_int_location]), current_mask);
		}
	}
};

__device__ __inline__ bool checkBit(unsigned int* voxel_table, size_t index){
	size_t int_location = index / size_t(32);
	unsigned int bit_pos = size_t(31) - (index % size_t(32)); // we count bit positions RtL, but array indices LtR
	return ((voxel_table[int_location]) & (1 << bit_pos));
}

__device__ __inline__ void setBit(unsigned int* voxel_table, size_t index){
	size_t int_location = index / size_t(32);
	unsigned int bit_pos = size_t(31) - (index % size_t(32)); // we count bit positions RtL, but array indices LtR
	unsigned int mask = 1 << bit_pos;
	atomicOr(&(voxel_table[int_location]), mask);
}

// Main triangle voxelization method
__global__ void voxelize_triangle(voxinfo info, float* triangle_data, unsigned int* voxel_table, bool morton_order){
	size_t thread_id = threadIdx.x + blockIdx.x * blockDim.x;
	size_t stride = blockDim.x * gridDim.x;

	// Common variables
	glm::vec3 delta_p = glm::vec3(1.0, 1.0, 1.0);
	glm::vec3 c(0.0f, 0.0f, 0.0f); // critical point
	glm::vec3 world_base(glm::vec3(info.bbox.min));
	size_t gridsiz2 = info.gridsize*info.gridsize;
	while (thread_id < info.n_triangles){ // every thread works on specific triangles in its stride
		size_t t = thread_id * 9; // triangle contains 9 vertices

		// COMPUTE COMMON TRIANGLE PROPERTIES
		glm::vec3 v0 = glm::vec3(triangle_data[t + 0], triangle_data[t + 1], triangle_data[t + 2]) - world_base; // get v0 and move to origin
		glm::vec3 v1 = glm::vec3(triangle_data[t + 3], triangle_data[t + 4], triangle_data[t + 5]) - world_base; // get v1 and move to origin
		glm::vec3 v2 = glm::vec3(triangle_data[t + 6], triangle_data[t + 7], triangle_data[t + 8]) - world_base; // get v2 and move to origin
		glm::vec3 e0 = v1 - v0;
		glm::vec3 e1 = v2 - v1;
		glm::vec3 e2 = v0 - v2;
		glm::vec3 n = glm::normalize(glm::cross(e0, e1));

		//COMPUTE TRIANGLE BBOX IN GRID
		AABox<glm::vec3> t_bbox_world(glm::min(v0, glm::min(v1, v2)), glm::max(v0, glm::max(v1, v2)));
		AABox<glm::ivec3> t_bbox_grid;
		t_bbox_grid.min = glm::clamp(glm::ivec3(floor(t_bbox_world.min)),
			glm::ivec3(1, 1, 1), 
			glm::ivec3(int(info.gridsize-1), int(info.gridsize-1), int(info.gridsize-1)));
		t_bbox_grid.max = glm::clamp(glm::ivec3(ceil(t_bbox_world.max)),
			glm::ivec3(1, 1, 1), 
			glm::ivec3(int(info.gridsize-1), int(info.gridsize-1), int(info.gridsize-1)));
//		t_bbox_grid.min = floor(t_bbox_world.min / info.unit);
//		t_bbox_grid.max = ceil(t_bbox_world.max / info.unit);

		// PREPARE PLANE TEST PROPERTIES
		if (n.x > 0.0f) { c.x = 1.0; }
		if (n.y > 0.0f) { c.y = 1.0; }
		if (n.z > 0.0f) { c.z = 1.0; }
		float d1 = glm::dot(n, (c - v0));
		float d2 = glm::dot(n, ((delta_p - c) - v0));

		// PREPARE PROJECTION TEST PROPERTIES
		// XY plane
		glm::vec2 n_xy_e0(-1.0f*e0.y, e0.x);
		glm::vec2 n_xy_e1(-1.0f*e1.y, e1.x);
		glm::vec2 n_xy_e2(-1.0f*e2.y, e2.x);
		if (n.z < 0.0f) {
			n_xy_e0 = -n_xy_e0;
			n_xy_e1 = -n_xy_e1;
			n_xy_e2 = -n_xy_e2;
		}
		float d_xy_e0 = (-1.0f * glm::dot(n_xy_e0, glm::vec2(v0.x, v0.y))) + glm::max(0.0f, n_xy_e0[0]) + glm::max(0.0f, n_xy_e0[1]);
		float d_xy_e1 = (-1.0f * glm::dot(n_xy_e1, glm::vec2(v1.x, v1.y))) + glm::max(0.0f, n_xy_e1[0]) + glm::max(0.0f, n_xy_e1[1]);
		float d_xy_e2 = (-1.0f * glm::dot(n_xy_e2, glm::vec2(v2.x, v2.y))) + glm::max(0.0f, n_xy_e2[0]) + glm::max(0.0f, n_xy_e2[1]);
		// YZ plane
		glm::vec2 n_yz_e0(-1.0f*e0.z, e0.y);
		glm::vec2 n_yz_e1(-1.0f*e1.z, e1.y);
		glm::vec2 n_yz_e2(-1.0f*e2.z, e2.y);
		if (n.x < 0.0f) {
			n_yz_e0 = -n_yz_e0;
			n_yz_e1 = -n_yz_e1;
			n_yz_e2 = -n_yz_e2;
		}
		float d_yz_e0 = (-1.0f * glm::dot(n_yz_e0, glm::vec2(v0.y, v0.z))) + glm::max(0.0f, n_yz_e0[0]) + glm::max(0.0f, n_yz_e0[1]);
		float d_yz_e1 = (-1.0f * glm::dot(n_yz_e1, glm::vec2(v1.y, v1.z))) + glm::max(0.0f, n_yz_e1[0]) + glm::max(0.0f, n_yz_e1[1]);
		float d_yz_e2 = (-1.0f * glm::dot(n_yz_e2, glm::vec2(v2.y, v2.z))) + glm::max(0.0f, n_yz_e2[0]) + glm::max(0.0f, n_yz_e2[1]);
		// ZX plane
		glm::vec2 n_zx_e0(-1.0f*e0.x, e0.z);
		glm::vec2 n_zx_e1(-1.0f*e1.x, e1.z);
		glm::vec2 n_zx_e2(-1.0f*e2.x, e2.z);
		if (n.y < 0.0f) {
			n_zx_e0 = -n_zx_e0;
			n_zx_e1 = -n_zx_e1;
			n_zx_e2 = -n_zx_e2;
		}
		float d_xz_e0 = (-1.0f * glm::dot(n_zx_e0, glm::vec2(v0.z, v0.x))) + glm::max(0.0f, n_zx_e0[0]) + glm::max(0.0f, n_zx_e0[1]);
		float d_xz_e1 = (-1.0f * glm::dot(n_zx_e1, glm::vec2(v1.z, v1.x))) + glm::max(0.0f, n_zx_e1[0]) + glm::max(0.0f, n_zx_e1[1]);
		float d_xz_e2 = (-1.0f * glm::dot(n_zx_e2, glm::vec2(v2.z, v2.x))) + glm::max(0.0f, n_zx_e2[0]) + glm::max(0.0f, n_zx_e2[1]);

		// test possible grid boxes for overlap
		for (int z = t_bbox_grid.min.z - 1; z <= t_bbox_grid.max.z; z++){
			for (int y = t_bbox_grid.min.y - 1; y <= t_bbox_grid.max.y; y++){
				for (int x = t_bbox_grid.min.x - 1; x <= t_bbox_grid.max.x; x++){
					// size_t location = x + (y*info.gridsize) + (z*info.gridsize*info.gridsize);
					// if (checkBit(voxel_table, location)){ continue; }
					// TRIANGLE PLANE THROUGH BOX TEST
					glm::vec3 p((x + 0.5f), (y + 0.5f), (z + 0.5f));
					float nDOTp = glm::dot(n, p);
					if ((nDOTp + d1) * (nDOTp + d2) > 0.0f){ continue; }

					// PROJECTION TESTS
					// XY
					glm::vec2 p_xy(p.x, p.y);
					if ((glm::dot(n_xy_e0, p_xy) + d_xy_e0) < 0.0f){ continue; }
					if ((glm::dot(n_xy_e1, p_xy) + d_xy_e1) < 0.0f){ continue; }
					if ((glm::dot(n_xy_e2, p_xy) + d_xy_e2) < 0.0f){ continue; }

					// YZ
					glm::vec2 p_yz(p.y, p.z);
					if ((glm::dot(n_yz_e0, p_yz) + d_yz_e0) < 0.0f){ continue; }
					if ((glm::dot(n_yz_e1, p_yz) + d_yz_e1) < 0.0f){ continue; }
					if ((glm::dot(n_yz_e2, p_yz) + d_yz_e2) < 0.0f){ continue; }

					// XZ	
					glm::vec2 p_zx(p.z, p.x);
					if ((glm::dot(n_zx_e0, p_zx) + d_xz_e0) < 0.0f){ continue; }
					if ((glm::dot(n_zx_e1, p_zx) + d_xz_e1) < 0.0f){ continue; }
					if ((glm::dot(n_zx_e2, p_zx) + d_xz_e2) < 0.0f){ continue; }

					//atomicAdd(&voxel_count, 1);
					size_t location = size_t(x + (y*info.gridsize)) + (size_t(z)*gridsiz2);
					setBit(voxel_table, location);
					continue;
				}
			}
		}
		// sanity check: atomically count triangles
		//atomicAdd(&triangles_seen_count, 1);
		thread_id += stride;
	}
}

void voxelize(const voxinfo& v, float* triangle_data, unsigned int* vtable, bool morton_code){
	float   elapsedTime;

	// Create timers, set start time
	cudaEvent_t start_total, stop_total, start_vox, stop_vox;
	HANDLE_CUDA_ERROR(cudaEventCreate(&start_total));
	HANDLE_CUDA_ERROR(cudaEventCreate(&stop_total));
	HANDLE_CUDA_ERROR(cudaEventCreate(&start_vox));
	HANDLE_CUDA_ERROR(cudaEventCreate(&stop_vox));
	HANDLE_CUDA_ERROR(cudaEventRecord(start_total, 0));

	// Copy morton LUT if we're encoding to morton
	if (morton_code){
		HANDLE_CUDA_ERROR(cudaMemcpyToSymbol(morton256_x, host_morton256_x, 256 * sizeof(uint32_t)));
		HANDLE_CUDA_ERROR(cudaMemcpyToSymbol(morton256_y, host_morton256_y, 256 * sizeof(uint32_t)));
		HANDLE_CUDA_ERROR(cudaMemcpyToSymbol(morton256_z, host_morton256_z, 256 * sizeof(uint32_t)));
	}

	// Estimate best block and grid size using CUDA Occupancy Calculator
	int blockSize;   // The launch configurator returned block size 
	int minGridSize; // The minimum grid size needed to achieve the  maximum occupancy for a full device launch 
	int gridSize;    // The actual grid size needed, based on input size 
	cudaOccupancyMaxPotentialBlockSize(&minGridSize, &blockSize, voxelize_triangle);
	// Round up according to array size 
	gridSize = int( (v.n_triangles + blockSize - 1) / blockSize );

	HANDLE_CUDA_ERROR(cudaEventRecord(start_vox, 0));
	voxelize_triangle << <gridSize, blockSize >> >(v, triangle_data, vtable, morton_code);
	CHECK_CUDA_ERROR();

	cudaDeviceSynchronize();
	HANDLE_CUDA_ERROR(cudaEventRecord(stop_vox, 0));
	HANDLE_CUDA_ERROR(cudaEventSynchronize(stop_vox));
	HANDLE_CUDA_ERROR(cudaEventElapsedTime(&elapsedTime, start_vox, stop_vox));
	printf("Voxelisation GPU time:  %3.1f ms\n", elapsedTime);

	// SANITY CHECKS
	//size_t t_seen, v_count;
	//HANDLE_CUDA_ERROR(cudaMemcpyFromSymbol((void*)&(t_seen),triangles_seen_count, sizeof(t_seen), 0, cudaMemcpyDeviceToHost));
	//HANDLE_CUDA_ERROR(cudaMemcpyFromSymbol((void*)&(v_count), voxel_count, sizeof(v_count), 0, cudaMemcpyDeviceToHost));
	//printf("We've seen %llu triangles on the GPU \n", t_seen);
	//printf("We've found %llu voxels on the GPU \n", v_count);

	// get stop time, and display the timing results
	HANDLE_CUDA_ERROR(cudaEventRecord(stop_total, 0));
	HANDLE_CUDA_ERROR(cudaEventSynchronize(stop_total));
	HANDLE_CUDA_ERROR(cudaEventElapsedTime(&elapsedTime, start_total, stop_total));
	printf("Total GPU time (including memory transfers):  %3.1f ms\n", elapsedTime);

	// Destroy timers
	HANDLE_CUDA_ERROR(cudaEventDestroy(start_total));
	HANDLE_CUDA_ERROR(cudaEventDestroy(stop_total));
	HANDLE_CUDA_ERROR(cudaEventDestroy(start_vox));
	HANDLE_CUDA_ERROR(cudaEventDestroy(stop_vox));
}
