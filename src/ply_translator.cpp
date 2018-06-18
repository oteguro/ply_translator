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
	return true; 
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
	

	MStatus status;
	MVector vNormal;
	MColor vColor;
	float2 uvPoint;

	std::vector<float> verts;
	std::vector<float> norms;
	std::vector<uint8_t> colors;
	std::vector<int32_t> vertexIndicies;
	std::vector<float> faceTexcoords;

	MSelectionList slist;
	MGlobal::getActiveSelectionList(slist);
	MItSelectionList iter(slist);

	if (iter.isDone()) {
		fprintf(stderr, "Error: Nothing is selected.\n");
		return MS::kFailure;
	}

	// We will need to interate over a selected node's heirarchy 
	// in the case where shapes are grouped, and the group is selected.
	MItDag dagIterator(MItDag::kDepthFirst, MFn::kInvalid, &status);

	// Selection list loop
	for (; !iter.isDone(); iter.next())
	{
		MDagPath objectPath;
		// get the selected node
		status = iter.getDagPath(objectPath);

		// reset iterator's root node to be the selected node.
		status = dagIterator.reset(objectPath.node(),
			MItDag::kDepthFirst, MFn::kInvalid);

		// DAG iteration beginning at at selected node
		for (; !dagIterator.isDone(); dagIterator.next())
		{
			MDagPath dagPath;
			MObject  component = MObject::kNullObj;
			status = dagIterator.getPath(dagPath);

			if (!status) {
				fprintf(stderr, "Failure getting DAG path.\n");
				return MS::kFailure;
			}

			if (status)
			{
				// skip over intermediate objects
				MFnDagNode dagNode(dagPath, &status);
				if (dagNode.isIntermediateObject())
				{
					continue;
				}

				if (dagPath.hasFn(MFn::kNurbsSurface))
				{
					status = MS::kSuccess;
					fprintf(stderr, "Warning: skipping Nurbs Surface.\n");
				}
				else if ((dagPath.hasFn(MFn::kMesh)) &&
					(dagPath.hasFn(MFn::kTransform)))
				{
					continue;
				}
				else if (dagPath.hasFn(MFn::kMesh))
				{
					MFnMesh fnMesh(dagPath);
					int numVertices = fnMesh.numVertices();
					MItMeshVertex vertIter(dagPath);

					for (; !vertIter.isDone(); vertIter.next())
					{
						MPoint pnt = vertIter.position(MSpace::kWorld);
						verts.push_back((float)pnt.x);
						verts.push_back((float)pnt.y);
						verts.push_back((float)pnt.z);

						vertIter.getNormal(vNormal);
						norms.push_back((float)vNormal.x);
						norms.push_back((float)vNormal.y);
						norms.push_back((float)vNormal.z);

						vertIter.getColor(vColor);
						if (vColor.r <= 0.001 && vColor.g <= 0.001 && vColor.b <= 0.001)
						{
							vColor = MColor(0.5, 0.5, 0.5, 1.0);
						}
						colors.push_back((uint8_t) (vColor.r * 255));
						colors.push_back((uint8_t)(vColor.g * 255));
						colors.push_back((uint8_t)(vColor.b * 255));
						colors.push_back((uint8_t)(vColor.a * 255));
					}

					MItMeshPolygon faceIter(dagPath);
					MIntArray faceVertices;

					for (; !faceIter.isDone(); faceIter.next())
					{
						faceIter.getVertices(faceVertices);
						if (faceVertices.length() >= 4) {
							MGlobal::displayError("Mesh has faces with more than 3 sides!");
							return MS::kFailure;
						}

						for (unsigned int p=0; p<faceVertices.length(); p++) {
							vertexIndicies.push_back(faceVertices[p]);
							
							faceIter.getUV(faceVertices[p], uvPoint);
							faceTexcoords.push_back(uvPoint[0]);
							faceTexcoords.push_back(uvPoint[1]);
						}
					}
				}
			}
		}
	}

	// Tinyply does not perform any file i/o internally
	MString fileName = file.fullName(), unitName;
	std::filebuf fb;
	fb.open(fileName.asChar(), std::ios::out | std::ios::binary);
	std::ostream outputStream(&fb);

	tinyply::PlyFile myFile;

	myFile.add_properties_to_element("vertex", { "x", "y", "z" }, verts);
	myFile.add_properties_to_element("vertex", { "nx", "ny", "nz" }, norms);
	myFile.add_properties_to_element("vertex", { "red", "green", "blue", "alpha" }, colors);
	myFile.add_properties_to_element("face", { "vertex_indices" }, vertexIndicies, 3, tinyply::PlyProperty::Type::UINT8);
	myFile.add_properties_to_element("face", { "texcoord" }, faceTexcoords, 6, tinyply::PlyProperty::Type::UINT8);
	myFile.comments.push_back("generated by tinyply");

	std::ostringstream stream;
	myFile.write(stream, true);

	outputStream << stream.str();

	fb.close();

	return MStatus::kSuccess;
}

void* PlyTranslator::creator()
{
	return new PlyTranslator();
}

