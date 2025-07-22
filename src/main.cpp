#include "overlay_application.h"
#include "developer_console.h"

BOOL WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	try
	{
		OverlayApplication app;
		return app.Run();
	}
	catch (const std::exception &e)
	{
		DEV_LOG_ERROR("Fatal error: " + std::string(e.what()));
		return -1;
	}
	catch (...)
	{
		DEV_LOG_ERROR("Unknown fatal error occurred");
		return -1;
	}
}
