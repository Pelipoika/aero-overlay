/**********************************************************************************************
*
*   raylib-extras * Utilities and Shared Components for Raylib
*
*   rlFPCamera * First person camera (C version)
*
*   LICENSE: ZLib
*
*   Copyright (c) 2021 Jeffery Myers
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

#ifndef FP_CAMERA_H
#define FP_CAMERA_H

#include "raylib.h"
#include "raymath.h"

typedef enum
{
	MOVE_FRONT = 0,
	MOVE_BACK,
	MOVE_RIGHT,
	MOVE_LEFT,
	MOVE_UP,
	MOVE_DOWN,
	TURN_LEFT,
	TURN_RIGHT,
	TURN_UP,
	TURN_DOWN,
	SPRINT,
	LAST_CONTROL
} rlFPCameraControls;

typedef struct
{
	bool InvertY;

	// how far down can the camera look
	float MinimumViewY;

	// how far up can the camera look
	float MaximumViewY;

	// the position of the base of the camera (on the floor)
	// note that this will not be the view position because it is offset by the eye height.
	// this value is also not changed by the view bobble
	Vector3 CameraPosition;

	// how far from the base of the camera is the player's view
	float PlayerEyesPosition;

	// the field of view in X and Y
	Vector2 FOV;

	// state for view movement
	float TargetDistance;

	// state for view angles
	Vector2 ViewAngles;

	// state for window focus
	bool Focused;

	// raylib camera for use with raylib modes.
	Camera ViewCamera;

	Vector3 Forward;
	Vector3 Right;

	//clipping planes
	// note must use BeginModeFP3D and EndModeFP3D instead of BeginMode3D/EndMode3D for clipping planes to work
	double NearPlane;
	double FarPlane;
} rlFPCamera;

// called to initialize a camera to default values
RLAPI void rlFPCameraInit(rlFPCamera *camera, float fovY, Vector3 position);

// called to update field of view in X when window resizes
RLAPI void rlFPCameraResizeView(rlFPCamera *camera);

// Get the camera's position in world (or game) space
RLAPI Vector3 rlFPCameraGetPosition(const rlFPCamera *camera);

// Set the camera's position in world (or game) space
RLAPI void rlFPCameraSetPosition(rlFPCamera *camera, Vector3 pos);

// returns the ray from the camera through the center of the view
RLAPI Ray rlFPCameraGetViewRay(const rlFPCamera *camera);

// update the camera for the current frame
RLAPI void rlFPCameraUpdate(rlFPCamera *camera);

// start drawing using the camera, with near/far plane support
RLAPI void rlFPCameraBeginMode3D(const rlFPCamera *camera);

// end drawing with the camera
RLAPI void rlFPCameraEndMode3D();

#endif //FP_CAMERA_H
