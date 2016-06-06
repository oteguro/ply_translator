#pragma once

#include <maya/MStatus.h>
#include <maya/MObject.h>
#include <maya/MPxFileTranslator.h>


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

