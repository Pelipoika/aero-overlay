#include "overlay_application.h"
#include <iostream>

int main()
{
	try
	{
		OverlayApplication app;
		return app.Run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Fatal error: " << e.what() << '\n';
		return -1;
	}
	catch (...)
	{
		std::cerr << "Unknown fatal error occurred" << '\n';
		return -1;
	}
}
