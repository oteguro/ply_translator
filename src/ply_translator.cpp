#include "ply_translator.h"
#include "../external/tinyply/tinyply.h"

#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <thread>
#include <chrono>

#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MPointArray.h>
#include <maya/MIntArray.h>

namespace
{
	typedef std::chrono::time_point<std::chrono::high_resolution_clock> timepoint;
	std::chrono::high_resolution_clock c;
	inline std::chrono::time_point<std::chrono::high_resolution_clock> now()
	{
		return c.now();
	}

	inline double difference_micros(timepoint start, timepoint end)
	{
		return (double)std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	}

} // unnamed namespace 

MString const PlyTranslator::magic("ply");

PlyTranslator::PlyTranslator()
{
}

PlyTranslator::~PlyTranslator()
{
}

bool PlyTranslator::haveReadMethod() const
{
	return true;
}

bool PlyTranslator::haveWriteMethod() const
{
	return false; // TODO : 
}

bool PlyTranslator::haveReferenceMethod() const
{
	return false;
}

bool PlyTranslator::haveNamespaceSupport() const
{
	return true;
}

bool PlyTranslator::canBeOpened() const
{
	return true;
}

MString PlyTranslator::defaultExtension() const
{
	return "ply";
}

MPxFileTranslator::MFileKind PlyTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const
{
	MFileKind rval = kNotMyFileType;
	if ((size >= (short)magic.length()) &&
		(0 == strncmp(buffer, magic.asChar(), magic.length())))
	{
		rval = kIsMyFileType;
	}
	return rval;
}

MStatus PlyTranslator::reader(const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode)
{
	const MString fileName = file.fullName();
	MStatus retval(MStatus::kSuccess);

	try
	{
		MStatus stat;

		std::ifstream ss(std::string(fileName.asChar()), std::ios::binary);
		tinyply::PlyFile file(ss);

		uint32_t vertexCount = 0;
		uint32_t normalCount = 0;
		uint32_t colorCount  = 0;
		uint32_t faceCount   = 0;
		uint32_t uvCount     = 0;
		uint32_t faceColorCount = 0;
		uint32_t tristripCount = 0;

		std::vector<float>      vertices;
		std::vector<float>      normals;
		std::vector<uint8_t>    colors;
		std::vector<uint32_t>   faces;
		std::vector<float>      uvCoords;
		std::vector<uint8_t>    faceColors;
		std::vector<int>        stripIndices;

		vertexCount     = file.request_properties_from_element("vertex", {"x", "y", "z"},      vertices);
		normalCount     = file.request_properties_from_element("vertex", { "nx", "ny", "nz" }, normals);
		colorCount      = file.request_properties_from_element("vertex", { "red", "green", "blue", "alpha" }, colors);
		faceCount       = file.request_properties_from_element("face",   { "vertex_indices" }, faces, 3);
		uvCount         = file.request_properties_from_element("face",   { "texcoord" }, uvCoords);
		faceColorCount  = file.request_properties_from_element("face",   { "red", "green", "blue", "alpha" }, faceColors);
		tristripCount   = file.request_properties_from_element("tristrips", { "vertex_indices" }, stripIndices);

		timepoint before = now();
		file.read(ss);
		timepoint after  = now();

#ifdef _DEBUG
		std::cerr << "Parsing took " << difference_micros(before, after) << "micro sec.: " << std::endl;
		std::cerr << "\tRead " << vertices.size()   << " total vertices ("          << vertexCount << " properties)." << std::endl;
		std::cerr << "\tRead " << normals.size()    << " total normals ("           << normalCount  << " properties)." << std::endl;
		std::cerr << "\tRead " << colors.size()     << " total vertex colors ("     << colorCount << " properties)." << std::endl;
		std::cerr << "\tRead " << faces.size()      << " total faces (triangles) (" << faceCount << " properties)." << std::endl;
		std::cerr << "\tRead " << uvCoords.size()   << " total texcoords ("         << uvCount << " properties)." << std::endl;
		std::cerr << "\tRead " << faceColors.size() << " total face colors ("       << faceColorCount << " properties)." << std::endl;
		std::cerr << "\tRead " << stripIndices.size() << " total strip indices ("   << tristripCount << " properties)." << std::endl;
		std::cerr << std::endl;
#endif

		// Convert mesh data. 
		MPointArray cornerVertices;
		for(uint32_t i=0; i<vertexCount; ++i)
		{
			cornerVertices.append(MPoint(vertices[i*3], vertices[i*3+1], vertices[i*3+2]));
		}

		const int kNumVertexCountsPerPrimitive = 3;
		MIntArray polygonConnects;
		MIntArray polygonCounts;
		MIntArray faceList;

		if(faceCount)
		{
			for (uint32_t i = 0; i < faceCount; ++i)
			{
				polygonCounts.append(kNumVertexCountsPerPrimitive);

				polygonConnects.append(faces[i * 3]);
				polygonConnects.append(faces[i * 3 + 1]);
				polygonConnects.append(faces[i * 3 + 2]);

				faceList.append(i);
				faceList.append(i);
				faceList.append(i);
			}
		}
		else if(tristripCount > 0)
		{
			faceCount = 0;
			uint32_t traceIndex = 2;
			uint32_t tristripIndexCount = static_cast<uint32_t>(stripIndices.size());
			int windings = 0;

			while(tristripIndexCount > traceIndex)
			{
				int vindex0 = stripIndices[traceIndex - 2];
				int vindex1 = stripIndices[traceIndex - 1];
				int vindex2 = stripIndices[traceIndex];

				if(vindex0 != vindex1 && vindex1 != vindex2 && vindex0 != vindex2
				&& vindex0 >= 0 && vindex1 >= 0 && vindex2 >= 0)
				{
					polygonCounts.append(kNumVertexCountsPerPrimitive);

					if((windings%2) == 0)
					{
						polygonConnects.append(vindex0);
						polygonConnects.append(vindex1);
						polygonConnects.append(vindex2);
					}
					else
					{
						polygonConnects.append(vindex2);
						polygonConnects.append(vindex1);
						polygonConnects.append(vindex0);
					}

					faceList.append(faceCount);
					faceList.append(faceCount);
					faceList.append(faceCount);

					faceCount++;
					windings++;
				}
				else
				{
					windings = 0;
				}
				traceIndex++;
			}
		}

		if(vertexCount == 0 || faceCount == 0)
		{	// Invalid mesh was found... 
			return retval;
		}

		// Import mesh to scene. 
		MFnMesh     meshFn;
		meshFn.create(vertexCount,
					  faceCount,
					  cornerVertices,
					  polygonCounts,
					  polygonConnects,
					  MObject::kNullObj,
					  &stat);
		CHECK_MSTATUS(stat);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
	}

	return retval;
}

MStatus PlyTranslator::writer(const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode)
{


	return MStatus::kSuccess;
}

void* PlyTranslator::creator()
{
	return new PlyTranslator();
}

