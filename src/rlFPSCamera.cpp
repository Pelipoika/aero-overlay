/**********************************************************************************************
*
*   raylibExtras * Utilities and Shared Components for Raylib
*
*   FPCamera * Simple First person camera (C version)
*
*   LICENSE: MIT
*
*   Copyright (c) 2020 Jeffery Myers
*
*   Permission is hereby granted, free of charge, to any person obtaining a copy
*   of this software and associated documentation files (the "Software"), to deal
*   in the Software without restriction, including without limitation the rights
*   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*   copies of the Software, and to permit persons to whom the Software is
*   furnished to do so, subject to the following conditions:
*
*   The above copyright notice and this permission notice shall be included in all
*   copies or substantial portions of the Software.
*
*   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*   SOFTWARE.
*
**********************************************************************************************/

#include "rlFPSCamera.h"
#include "rlgl.h"
#include <cstdlib>

void rlFPCameraInit(rlFPCamera *camera, const float fovY, const Vector3 position)
{
	if (camera == nullptr)
		return;

	camera->InvertY = false;

	camera->MinimumViewY = -89.0f;
	camera->MaximumViewY = 89.0f;

	camera->Focused = IsWindowFocused();

	camera->TargetDistance     = 1;
	camera->PlayerEyesPosition = 0.5f;
	camera->ViewAngles         =
	{
		0, 0
	};

	camera->CameraPosition = position;
	camera->FOV.y          = fovY;

	camera->ViewCamera.position = position;
	camera->ViewCamera.position.y += camera->PlayerEyesPosition;
	camera->ViewCamera.target     = Vector3Add(camera->ViewCamera.position, {0, 0, camera->TargetDistance});
	camera->ViewCamera.up         = {0.0f, 1.0f, 0.0f};
	camera->ViewCamera.fovy       = fovY;
	camera->ViewCamera.projection = CAMERA_PERSPECTIVE;

	camera->NearPlane = 0.01;
	camera->FarPlane  = 10000.0;

	rlFPCameraResizeView(camera);
}

void rlFPCameraResizeView(rlFPCamera *camera)
{
	if (camera == nullptr)
		return;

	float width  = static_cast<float>(GetScreenWidth());
	float height = static_cast<float>(GetScreenHeight());

	camera->FOV.y = camera->ViewCamera.fovy;

	if (height != 0)
		camera->FOV.x = camera->FOV.y * (width / height);
}

Vector3 rlFPCameraGetPosition(const rlFPCamera *camera)
{
	return camera->CameraPosition;
}

void rlFPCameraSetPosition(rlFPCamera *camera, const Vector3 pos)
{
	camera->CameraPosition      = pos;
	Vector3 forward             = Vector3Subtract(camera->ViewCamera.target, camera->ViewCamera.position);
	camera->ViewCamera.position = camera->CameraPosition;
	camera->ViewCamera.target   = Vector3Add(camera->CameraPosition, forward);
}

RLAPI Ray rlFPCameraGetViewRay(const rlFPCamera *camera)
{
	return {camera->CameraPosition, camera->Forward};
}

void rlFPCameraUpdate(rlFPCamera *camera)
{
	if (camera == nullptr)
		return;

	// Angle clamp
	if (camera->ViewAngles.y < camera->MinimumViewY * DEG2RAD)
		camera->ViewAngles.y = camera->MinimumViewY * DEG2RAD;
	else if (camera->ViewAngles.y > camera->MaximumViewY * DEG2RAD)
		camera->ViewAngles.y = camera->MaximumViewY * DEG2RAD;

	// Recalculate camera target considering translation and rotation
	Vector3 target = Vector3Transform({0, 0, 1},
	                                  MatrixRotateZYX({camera->ViewAngles.y, -camera->ViewAngles.x, 0}));

	camera->Forward = Vector3Transform({0, 0, 1},
	                                   MatrixRotateZYX({0, -camera->ViewAngles.x, 0}));

	camera->Right = {camera->Forward.z * -1.0f, 0, camera->Forward.x};

	camera->ViewCamera.position = camera->CameraPosition;

	float eyeOffset = camera->PlayerEyesPosition;

	camera->ViewCamera.up.x = 0;
	camera->ViewCamera.up.z = 0;

	camera->ViewCamera.position.y += eyeOffset;

	camera->ViewCamera.target.x = camera->ViewCamera.position.x + target.x;
	camera->ViewCamera.target.y = camera->ViewCamera.position.y + target.y;
	camera->ViewCamera.target.z = camera->ViewCamera.position.z + target.z;
}

static void SetupCamera(const rlFPCamera *camera, const float aspect)
{
	rlDrawRenderBatchActive();			// Draw Buffers (Only OpenGL 3+ and ES2)
	rlMatrixMode(RL_PROJECTION);        // Switch to projection matrix
	rlPushMatrix();                     // Save previous matrix, which contains the settings for the 2d ortho projection
	rlLoadIdentity();                   // Reset current matrix (projection)

	if (camera->ViewCamera.projection == CAMERA_PERSPECTIVE)
	{
		// Setup perspective projection
		double top   = RL_CULL_DISTANCE_NEAR * tan(camera->ViewCamera.fovy * 0.5 * DEG2RAD);
		double right = top * aspect;

		rlFrustum(-right, right, -top, top, camera->NearPlane, camera->FarPlane);
	}
	else if (camera->ViewCamera.projection == CAMERA_ORTHOGRAPHIC)
	{
		// Setup orthographic projection
		double top   = camera->ViewCamera.fovy / 2.0;
		double right = top * aspect;

		rlOrtho(-right, right, -top, top, camera->NearPlane, camera->FarPlane);
	}

	// NOTE: zNear and zFar values are important when computing depth buffer values

	rlMatrixMode(RL_MODELVIEW);         // Switch back to modelview matrix
	rlLoadIdentity();                   // Reset current matrix (modelview)

	// Setup Camera view
	Matrix matView = MatrixLookAt(camera->ViewCamera.position, camera->ViewCamera.target, camera->ViewCamera.up);

	rlMultMatrixf(MatrixToFloatV(matView).v);      // Multiply modelview matrix by view matrix (camera)

	rlEnableDepthTest();                // Enable DEPTH_TEST for 3D
}

void rlFPCameraBeginMode3D(const rlFPCamera *camera)
{
	if (camera == nullptr)
		return;

	float aspect = static_cast<float>(GetScreenWidth()) / static_cast<float>(GetScreenHeight());
	SetupCamera(camera, aspect);
}

void rlFPCameraEndMode3D()
{
	EndMode3D();
}
