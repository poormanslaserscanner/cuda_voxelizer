#include "util_io.h"
using namespace std;

size_t get_file_length(const std::string base_filename){
	// open file at the end
	std::ifstream input(base_filename.c_str(), ios_base::ate | ios_base::binary);
	assert(input);
	size_t length = input.tellg();
	input.close();
	return length; // get file length
}

void read_binary(void* data, const size_t length, const std::string base_filename){
	// open file
	std::ifstream input(base_filename.c_str(), ios_base::in | ios_base::binary);
	assert(input);
#ifndef SILENT
	fprintf(stdout, "Reading %llu kb of binary data from file %s \n", size_t(length / 1024.0f), base_filename.c_str()); fflush(stdout);
#endif
	input.seekg(0, input.beg);
	input.read((char*) data, 8);
	input.close();
	return;
}

void write_binary(void* data, size_t bytes, const std::string base_filename){
	string filename_output = base_filename + string(".bin");
#ifndef SILENT
	fprintf(stdout, "Writing data in binary format to %s (%llu kb) \n", filename_output.c_str(), size_t(bytes / 1024.0f));
#endif
	ofstream output(filename_output.c_str(), ios_base::out | ios_base::binary);
	output.write((char*)data, bytes);
	output.close();
}

void write_binvox(const unsigned int* vtable, const voxinfo& v, const AABox<glm::vec3> &bbox_mesh, const std::string base_filename){
	// Open file
	string filename_output = base_filename + string(".binvox");
#ifndef SILENT
	fprintf(stdout, "Writing data in binvox format to %s \n", filename_output.c_str());
#endif
	ofstream output(filename_output.c_str(), ios::out | ios::binary);
	assert(output);
	AABox<glm::uvec3> ioutbox;
	ioutbox.min = floor((bbox_mesh.min - v.bbox.min) / v.unit);
	ioutbox.min -= glm::uvec3(3u, 3u, 3u);
	ioutbox.max = ceil((bbox_mesh.max - v.bbox.min) / v.unit);
	ioutbox.max += glm::uvec3(3u, 3u, 3u);
	glm::uvec3 outgridsize(ioutbox.max - ioutbox.min);
	glm::vec3 outtranslate(glm::vec3(ioutbox.min) * v.unit + v.bbox.min);
	// Write ASCII header
	output << "#binvox 1" << endl;
	output << "dim " << outgridsize.x << " " << outgridsize.y << " " << outgridsize.z << "" << endl;
	output << "translate " << outtranslate.x << " " << outtranslate.y << " " << outtranslate.z << "" << endl;
	output << "scale " << v.unit << "" << endl;
	output << "data" << endl;

	// Write first voxel
	char currentvalue = checkVoxel(ioutbox.min.x, ioutbox.min.y, ioutbox.min.z, v.gridsize, vtable);
	output.write(&currentvalue, 1);
	int current_seen = 1;
	bool first = true;
	// Write BINARY Data
	for (size_t x = ioutbox.min.x; x < ioutbox.max.x; x++)
	{
		for (size_t z = ioutbox.min.z; z < ioutbox.max.z; z++)
		{
			for (size_t y = ioutbox.min.y; y < ioutbox.max.y; y++)
			{
				if (first)
				{
					first = false;
					continue;
				}
				char nextvalue = checkVoxel(x, y, z, v.gridsize, vtable);
				if (nextvalue != currentvalue || current_seen == 255){
					char to_print = current_seen;
					output.write(&to_print, 1);
					current_seen = 1;
					currentvalue = nextvalue;
					output.write(&currentvalue, 1);
				}
				else {
					current_seen++;
				}
			}
		}
	}

	// Write rest
	output.write((char*)&current_seen, 1);
	output.close();
}