#pragma once
//#include "cuda.h"
//#include "cuda_runtime.h"
#include <stdint.h>
//#include "device_launch_parameters.h"
#include <glm/glm.hpp>

template <typename T>
struct AABox {
	T min;
	T max;
	AABox() : min(T()), max(T()){}
	AABox(T min, T max) : min(min), max(max){}
};

// voxelisation info (same for every triangle)
struct voxinfo
{
	AABox<glm::ivec3> bbox;
	unsigned int gridsizex;
	unsigned int gridsizey;
	unsigned int gridsizez;
	size_t n_triangles;
	voxinfo(AABox<glm::ivec3> bbox_, size_t n_triangles_) 
		: gridsizex(0), gridsizey(0), gridsizez(0), bbox(bbox_), n_triangles(n_triangles_)
	{
		int extend2 = 8;
		bbox.max.x = bbox.max.x + extend2;
		bbox.max.y = bbox.max.y + extend2;
		bbox.max.z = bbox.max.z + extend2;
		bbox.min.x = bbox.min.x - extend2;
		bbox.min.y = bbox.min.y - extend2;
		bbox.min.z = bbox.min.z - extend2;
		gridsizex = bbox.max.x - bbox.min.x;
		gridsizey = bbox.max.y - bbox.min.y;
		gridsizez = bbox.max.z - bbox.min.z;
//		int width = glm::max(glm::max(bbox.max.x - bbox.min.x, bbox.max.y - bbox.min.y), bbox.max.z - bbox.min.z);
//		gridsize = width;
		int idelta;
		int ndelta;
		int pdelta;
		idelta = (4u - gridsizex % 4u) % 4u;
		ndelta = idelta / 2;
		pdelta = idelta - ndelta;
		bbox.max.x = bbox.max.x + ndelta;
		bbox.min.x = bbox.min.x - pdelta;

		idelta = (4u - gridsizey % 4u) % 4u;
		ndelta = idelta / 2;
		pdelta = idelta - ndelta;
		bbox.max.y = bbox.max.y + ndelta;
		bbox.min.y = bbox.min.y - pdelta;

		idelta = (4u - gridsizez % 4u) % 4u;
		ndelta = idelta / 2;
		pdelta = idelta - ndelta;
		bbox.max.z = bbox.max.z + ndelta;
		bbox.min.z = bbox.min.z - pdelta;
		gridsizex = bbox.max.x - bbox.min.x;
		gridsizey = bbox.max.y - bbox.min.y;
		gridsizez = bbox.max.z - bbox.min.z;


#if 0
		unsigned int ngrid = 1;
		while ((ngrid <<= 1) < gridsize && ngrid);
		gridsize = ngrid;
		if (!gridsize)
			return;
		int nwidth = gridsize;
		extend2 = (nwidth - width);

		int ndelta;
		int pdelta;
		int idelta = extend2;
		ndelta = idelta / 2;
		pdelta = (idelta - ndelta);


		bbox.max.x = bbox.max.x + ndelta;
		bbox.max.y = bbox.max.y + ndelta;
		bbox.max.z = bbox.max.z + ndelta;
		bbox.min.x = bbox.min.x - pdelta;
		bbox.min.y = bbox.min.y - pdelta;
		bbox.min.z = bbox.min.z - pdelta;
#endif
	}

	void print(){
		fprintf(stdout, "Bounding Box: (%d, %d, %d) to (%d, %d, %d) \n", bbox.min.x, bbox.min.y, bbox.min.z, bbox.max.x, bbox.max.y, bbox.max.z);
		fprintf(stdout, "Grid size x: %i \n", gridsizex);
		fprintf(stdout, "Grid size y: %i \n", gridsizey);
		fprintf(stdout, "Grid size z: %i \n", gridsizez);
		fprintf(stdout, "Triangles: %zu \n", n_triangles);
	}


};

// create mesh bbox cube
template <typename T>
inline AABox<T> createMeshBBCube(AABox<T> box){
	AABox<T> answer(box.min, box.max);
	glm::ivec3 lengths = box.max - box.min;
	int max_length = glm::max(lengths.x, glm::max(lengths.y, lengths.z));
	for (int i = 0; i<3; i++) {
		int delta = max_length - lengths[i];
		if (delta != 0)
		{
			int ndelta;
			int pdelta;
			{
				ndelta = delta / 2;
				pdelta = delta - ndelta;
			}
			answer.min[i] = box.min[i] - ndelta;
			answer.max[i] = box.max[i] + pdelta;
		}
	}
	return answer;
}

// LUT tables to copy to GPU memory for quick morton decode / encode
