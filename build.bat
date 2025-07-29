@echo off
echo Building Parametric Slicer...

REM Check if GCC is available
gcc --version >nul 2>&1
if errorlevel 1 (
    echo Error: GCC not found. Please install MinGW or use WSL.
    echo You can download MinGW from: https://www.mingw-w64.org/
    pause
    exit /b 1
)

REM Create src directory if it doesn't exist
if not exist "src" (
    echo Error: src directory not found. Please run this script from the project root.
    pause
    exit /b 1
)

REM Compile all source files
echo Compiling source files...
gcc -Wall -Wextra -std=c99 -O2 -g -c src/stl_parser.c -o src/stl_parser.o
if errorlevel 1 (
    echo Error: Failed to compile stl_parser.c
    pause
    exit /b 1
)

gcc -Wall -Wextra -std=c99 -O2 -g -c src/slicer.c -o src/slicer.o
if errorlevel 1 (
    echo Error: Failed to compile slicer.c
    pause
    exit /b 1
)

gcc -Wall -Wextra -std=c99 -O2 -g -c src/path_generator.c -o src/path_generator.o
if errorlevel 1 (
    echo Error: Failed to compile path_generator.c
    pause
    exit /b 1
)

gcc -Wall -Wextra -std=c99 -O2 -g -c src/bvh.c -o src/bvh.o
if errorlevel 1 (
    echo Error: Failed to compile bvh.c
    pause
    exit /b 1
)

gcc -Wall -Wextra -std=c99 -O2 -g -c src/convex_decomposition.c -o src/convex_decomposition.o
if errorlevel 1 (
    echo Error: Failed to compile convex_decomposition.c
    pause
    exit /b 1
)

gcc -Wall -Wextra -std=c99 -O2 -g -c src/topology_evaluator.c -o src/topology_evaluator.o
if errorlevel 1 (
    echo Error: Failed to compile topology_evaluator.c
    pause
    exit /b 1
)

gcc -Wall -Wextra -std=c99 -O2 -g -c src/gpu_accelerator.c -o src/gpu_accelerator.o
if errorlevel 1 (
    echo Error: Failed to compile gpu_accelerator.c
    pause
    exit /b 1
)

gcc -Wall -Wextra -std=c99 -O2 -g -c src/main.c -o src/main.o
if errorlevel 1 (
    echo Error: Failed to compile main.c
    pause
    exit /b 1
)

REM Link the executable
echo Linking executable...
gcc src/main.o src/stl_parser.o src/slicer.o src/path_generator.o src/bvh.o src/convex_decomposition.o src/topology_evaluator.o src/gpu_accelerator.o -o parametric_slicer.exe -lm
if errorlevel 1 (
    echo Error: Failed to link executable
    pause
    exit /b 1
)

REM Build test program
echo Building BVH test program...
gcc -Wall -Wextra -std=c99 -O2 -g test_bvh.c src/stl_parser.o src/bvh.o -o test_bvh.exe -lm
if errorlevel 1 (
    echo Warning: Failed to build BVH test program
) else (
    echo BVH test program built successfully
)

gcc -Wall -Wextra -std=c99 -O2 -g test_convex.c src/stl_parser.o src/convex_decomposition.o -o test_convex.exe -lm
if errorlevel 1 (
    echo Warning: Failed to build convex decomposition test program
) else (
    echo Convex decomposition test program built successfully
)

gcc -Wall -Wextra -std=c99 -O2 -g test_topology.c src/stl_parser.o src/topology_evaluator.o -o test_topology.exe -lm
if errorlevel 1 (
    echo Warning: Failed to build topology test program
) else (
    echo Topology test program built successfully
)

gcc -Wall -Wextra -std=c99 -O2 -g test_gpu.c src/stl_parser.o src/topology_evaluator.o src/gpu_accelerator.o -o test_gpu.exe -lm
if errorlevel 1 (
    echo Warning: Failed to build GPU test program
) else (
    echo GPU test program built successfully
)

echo.
echo Build completed successfully!
echo Executable: parametric_slicer.exe
echo Test programs: test_bvh.exe, test_convex.exe, test_topology.exe, test_gpu.exe
echo.
echo Usage examples:
echo   parametric_slicer.exe test_cube.stl
echo   parametric_slicer.exe test_cube.stl --bvh 4 --sort-axis xyz
echo   parametric_slicer.exe test_cube.stl --convex hierarchical --max-parts 8
echo   parametric_slicer.exe test_cube.stl --topology complete
echo   test_bvh.exe test_cube.stl 4 6
echo   test_convex.exe test_cube.stl 0 8 0.8 0.1
echo   test_topology.exe test_cube.stl 5
echo   test_gpu.exe test_cube.stl auto
echo.
pause 