
#include <maya/MStatus.h>
#include <maya/MObject.h>
#include <maya/MFnPlugin.h>

#include "ply_translator.h"

#define PLUGIN_VENDOR   "oteguro"
#define PLUGIN_VERSION  "0.1"
#define TRANSLATOR_NAME "ply_translator"

MStatus initializePlugin(MObject obj)
{
	MStatus   status;
	MFnPlugin plugin(obj, PLUGIN_VENDOR, PLUGIN_VERSION);

	status = plugin.registerFileTranslator(TRANSLATOR_NAME, NULL, PlyTranslator::creator, NULL, NULL, false);

	if(!status)
	{
		status.perror("registerFileTranslator");
		return status;
	}

	return status;
}

MStatus uninitializePlugin(MObject obj)
{
	MStatus   status;
	MFnPlugin plugin(obj);

	status = plugin.deregisterFileTranslator(TRANSLATOR_NAME);

	if (!status)
	{
		status.perror("deregisterFileTranslator");
		return status;
	}

	return status;
}
