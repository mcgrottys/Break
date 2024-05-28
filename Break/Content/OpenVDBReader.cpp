#include "pch.h"
#include "OpenVDBReader.h"
#include <openvdb/openvdb.h>
//#include <openvdb/openvdb.h>

OpenVDBReader::OpenVDBReaderClass::OpenVDBReaderClass()
{
}

OpenVDBReader::OpenVDBReaderClass::~OpenVDBReaderClass()
{
}

void OpenVDBReader::OpenVDBReaderClass::ReadVDBFile()
{        
    // Initialize the OpenVDB library.  This must be called at least
    // once per program and may safely be called multiple times.
    openvdb::initialize();
    // Create an empty floating-point grid with background value 0.
    openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create();
    std::cout << "Testing random access:" << std::endl;
    // Get an accessor for coordinate-based access to voxels.
    openvdb::FloatGrid::Accessor accessor = grid->getAccessor();
    // Define a coordinate with large signed indices.
    openvdb::Coord xyz(1000, -200000000, 30000000);
    // Set the voxel value at (1000, -200000000, 30000000) to 1.
    accessor.setValue(xyz, 1.0);
    // Verify that the voxel value at (1000, -200000000, 30000000) is 1.
    std::cout << "Grid" << xyz << " = " << accessor.getValue(xyz) << std::endl;
    // Reset the coordinates to those of a different voxel.
    xyz.reset(1000, 200000000, -30000000);
    // Verify that the voxel value at (1000, 200000000, -30000000) is
    // the background value, 0.
    std::cout << "Grid" << xyz << " = " << accessor.getValue(xyz) << std::endl;
    // Set the voxel value at (1000, 200000000, -30000000) to 2.
    accessor.setValue(xyz, 2.0);
    // Set the voxels at the two extremes of the available coordinate space.
    // For 32-bit signed coordinates these are (-2147483648, -2147483648, -2147483648)
    // and (2147483647, 2147483647, 2147483647).
    accessor.setValue(openvdb::Coord::min(), 3.0f);
    accessor.setValue(openvdb::Coord::max(), 4.0f);
    std::cout << "Testing sequential access:" << std::endl;
    // Print all active ("on") voxels by means of an iterator.
    for (openvdb::FloatGrid::ValueOnCIter iter = grid->cbeginValueOn(); iter; ++iter) {
        std::cout << "Grid" << iter.getCoord() << " = " << *iter << std::endl;
    }
}

void OpenVDBReader::OpenVDBReaderClass::OpenVDBFile(std::wstring filePath)
{
    openvdb::initialize();
    // Create a VDB file object.
    openvdb::io::File file("C:\\Clouds\\cloud_01_variant_0000.vdb");
    // Open the file.  This reads the file header, but not any grids.
    file.open();
    // Loop over all grids in the file and retrieve a shared pointer
    // to the one named "LevelSetSphere".  (This can also be done
    // more simply by calling file.readGrid("LevelSetSphere").)
    openvdb::GridBase::Ptr baseGrid;
    for (openvdb::io::File::NameIterator nameIter = file.beginName();
        nameIter != file.endName(); ++nameIter)
    {
        // Read in only the grid we are interested in.
        if (nameIter.gridName() == "density") {
            baseGrid = file.readGrid(nameIter.gridName());
        }
        else {
            std::cout << "skipping grid " << nameIter.gridName() << std::endl;
        }
    }
}

void OpenVDBReader::OpenVDBReaderClass::LoadVDBFile()
{
    openvdb::initialize();
    // Create a VDB file object.
    openvdb::io::File file("C:\\Clouds\\cloud_01_variant_0000.vdb");
    // Open the file.  This reads the file header, but not any grids.
    file.open();
    // Loop over all grids in the file and retrieve a shared pointer
    // to the one named "LevelSetSphere".  (This can also be done
    // more simply by calling file.readGrid("LevelSetSphere").)
    openvdb::GridBase::Ptr baseGrid;
    for (openvdb::io::File::NameIterator nameIter = file.beginName();
        nameIter != file.endName(); ++nameIter)
    {
        // Read in only the grid we are interested in.
        if (nameIter.gridName() == "density") {
            baseGrid = file.readGrid(nameIter.gridName());
        }
        else {
            std::cout << "skipping grid " << nameIter.gridName() << std::endl;
        }
    }

    auto grid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);
    openvdb::FloatGrid::Accessor accessor = grid->getAccessor();

    int count = 0;
    int maxLOD = 4;
    for (int i = 0; i < maxLOD; i++)
    {
        for (int x = 0; x < std::pow(2, i); x++)
        {
            for (int y = 0; y < std::pow(2, i); y++)
            {
                for (int z = 0; z < std::pow(2, i); z++)
                {
                    const int tileWidth = 32; // Assume tile width
                    const int tileHeight = 32; // Assume tile height
                    const int tileDepth = 16; // Assume tile depth
                    int numTiles = std::pow(2, i);

                    std::vector<byte> tileData(tileWidth * tileHeight * tileDepth * 4); // 4 bytes per pixel (BGRA)

                    for (int tz = 0; tz < tileDepth; tz++) {
                        for (int ty = 0; ty < tileHeight; ty++) {
                            for (int tx = 0; tx < tileWidth; tx++) {
                                // Calculate world coordinates in the VDB grid
                                int wx = tx + x * tileWidth;
                                int wy = ty + y * tileHeight;
                                int wz = tz + z * tileDepth;

                                // Get the value from the VDB file
                                float vdbValue = accessor.getValue(openvdb::Coord(wx, wy, wz));

                                // Convert the VDB value to a byte (assuming a simple normalization, adjust as needed)
                                byte v = static_cast<byte>(vdbValue * 255.0f);

                                // Fill the tile data
                                int index = ((tz * tileHeight + ty) * tileWidth + tx) * 4;
                                tileData[index] = v;       // Red
                                tileData[index + 1] = v;   // Green
                                tileData[index + 2] = v;   // Blue
                                tileData[index + 3] = 255; // Alpha
                            }
                        }
                    }
                }
            }
        }
    }
}
