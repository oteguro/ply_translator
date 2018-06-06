#pragma once

#include <maya/MStatus.h>
#include <maya/MObject.h>
#include <maya/MPxFileTranslator.h>

#include <string.h> 
#include <sys/types.h>
#include <maya/MStatus.h>
#include <maya/MPxCommand.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MItSelectionList.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItMeshEdge.h>
#include <maya/MFloatVector.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatArray.h>
#include <maya/MObjectArray.h>
#include <maya/MFnDagNode.h>
#include <maya/MItDag.h>
#include <maya/MDistance.h>
#include <maya/MIntArray.h>
#include <maya/MIOStream.h>
#include <maya/MStreamUtils.h>

#include <iterator>  


class PlyTranslator : public MPxFileTranslator
{
public:

			 PlyTranslator();
	virtual ~PlyTranslator();

	bool haveReadMethod () const;
	bool haveWriteMethod() const;

	bool haveReferenceMethod () const;
	bool haveNamespaceSupport() const;

	bool canBeOpened() const;

	MString defaultExtension() const;

	MFileKind identifyFile(const MFileObject& fileName, const char* buffer, short size) const;

	MStatus reader(const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode);

	MStatus writer(const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode);

	static void* creator();

private:

	static MString const magic;

}; // class PlyTranslator 

