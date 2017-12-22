function ret = compile( )
ret = false;
MEXOPTS={'-v','-g', '-largeArrayDims','-DMEX'};
OPENMP= 'COMPFLAGS=$COMPFLAGS /openmp';
OPENMPL = 'LINKFLAGS=$LINKFLAGS /nodefaultlib:vcomp';
pmls_dir = getenv('PMLS_INSTALL_DIR');
GLM_INC=['-I', pmls_dir, '/glm/include'];
%TRIMESH_INC=['-I', pmls_dir, '/trimesh2/include'];
%FILES = {'cudavoxmex.cpp', 'voxelize.cpp', 'libiomp5md.lib'};
FILES = {'cudavoxmex.cpp', 'voxelize.cpp'};
%mexcuda( MEXOPTS{:}, GLM_INC, TRIMESH_INC, FILES{:} );
%mex( MEXOPTS{:}, OPENMP, OPENMPL, GLM_INC, FILES{:} );
mex( MEXOPTS{:}, GLM_INC, FILES{:} );
ret = true;
end

