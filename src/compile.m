function ret = compile( )
ret = false;
MEXOPTS={'-v' };%,'-g'};
pmls_dir = getenv('PMLS_INSTALL_DIR');
GLM_INC=['-I', pmls_dir, '/glm/include'];
%TRIMESH_INC=['-I', pmls_dir, '/trimesh2/include'];
FILES = {'cudavoxmex.cpp', 'util_cuda.cpp', 'voxelize.cu'};
%mexcuda( MEXOPTS{:}, GLM_INC, TRIMESH_INC, FILES{:} );
mexcuda( MEXOPTS{:}, GLM_INC, FILES{:} );
ret = true;
end

