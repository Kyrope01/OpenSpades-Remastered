/*
 Copyright (c) 2013 yvt

 This file is part of OpenSpades.

 OpenSpades is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 OpenSpades is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with OpenSpades.  If not, see <http://www.gnu.org/licenses/>.

 */

#include <algorithm>
#include <cmath>

#include "GLDynamicLight.h"

namespace spades {
	namespace draw {
		namespace {
			bool SegmentIntersectsAABB(Vector3 start, Vector3 end, const AABB3 &box) {
				Vector3 direction = end - start;
				float minT = 0.f;
				float maxT = 1.f;

				auto intersectAxis = [&](float axisStart, float axisDirection, float axisMin,
				                         float axisMax) {
					if (fabsf(axisDirection) < 1.e-6f) {
						return axisStart >= axisMin && axisStart <= axisMax;
					}

					float t1 = (axisMin - axisStart) / axisDirection;
					float t2 = (axisMax - axisStart) / axisDirection;
					if (t1 > t2)
						std::swap(t1, t2);
					minT = std::max(minT, t1);
					maxT = std::min(maxT, t2);
					return minT <= maxT;
				};

				return intersectAxis(start.x, direction.x, box.min.x, box.max.x) &&
				       intersectAxis(start.y, direction.y, box.min.y, box.max.y) &&
				       intersectAxis(start.z, direction.z, box.min.z, box.max.z);
			}
		} // namespace

		GLDynamicLight::GLDynamicLight(const client::DynamicLightParam &param) : param(param) {

			if (param.type == client::DynamicLightTypeSpotlight) {
				float t = tanf(param.spotAngle * .5f);
				Matrix4 mat;
				mat = Matrix4::FromAxis(param.spotAxis[0], param.spotAxis[1], param.spotAxis[2],
				                        param.origin);
				mat = mat * Matrix4::Scale(t * 2.0f, t * 2.0f, 1.f);

				projMatrix = mat.InversedFast();

				Matrix4 m = Matrix4::Identity();
				m.m[15] = 0.f;
				m.m[11] = 1.f;

				m.m[8] += .5f;
				m.m[9] += .5f;
				projMatrix = m * projMatrix;

				// Construct clipping planes which are oriented inside.
				// To do that, first we calculate tangent vectors:
				Vector3 planeTan[] = {
				  param.spotAxis[2] + param.spotAxis[0] * t,
				  param.spotAxis[2] + param.spotAxis[1] * t,
				  param.spotAxis[2] - param.spotAxis[0] * t,
				  param.spotAxis[2] - param.spotAxis[1] * t,
				};
				// Then, use them to derive normal vectors:
				Vector3 planeN[] = {
				  Vector3::Cross(param.spotAxis[1], planeTan[0]),
				  Vector3::Cross(planeTan[1], param.spotAxis[0]),
				  Vector3::Cross(planeTan[2], param.spotAxis[1]),
				  Vector3::Cross(param.spotAxis[0], planeTan[3]),
				};
				// Finally, find planes with these normal vectors:
				for (std::size_t i = 0; i < 4; ++i) {
					clipPlanes[i] = Plane3::PlaneWithPointOnPlane(param.origin, planeN[i]);
				}
			}

			if (param.type == client::DynamicLightTypeLinear) {
				poweredLength = (param.point2 - param.origin).GetPoweredLength();
			}
		}

		bool GLDynamicLight::Cull(const spades::AABB3 &box) const {
			if (param.type == client::DynamicLightTypeSpotlight) {
				for (const Plane3 &plane : clipPlanes) {
					if (!PlaneCullTest(plane, box)) {
						return false;
					}
				}
			}

			const client::DynamicLightParam &param = GetParam();
			AABB3 inflatedBox = box.Inflate(param.radius);
			if (param.type == client::DynamicLightTypeLinear) {
				return SegmentIntersectsAABB(param.origin, param.point2, inflatedBox);
			}

			return inflatedBox && param.origin;
		}

		bool GLDynamicLight::SphereCull(const spades::Vector3 &center, float radius) const {
			const client::DynamicLightParam &param = GetParam();

			if (param.type == client::DynamicLightTypeSpotlight) {
				for (const Plane3 &plane : clipPlanes) {
					if (plane.GetDistanceTo(center) < -radius) {
						return false;
					}
				}
			} else if (param.type == client::DynamicLightTypeLinear) {
				Vector3 segment = param.point2 - param.origin;
				float t = 0.f;
				if (poweredLength > 0.f) {
					t = Vector3::Dot(center - param.origin, segment) / poweredLength;
					t = std::min(1.f, std::max(0.f, t));
				}
				Vector3 closestPoint = param.origin + segment * t;
				float maxDistance = radius + param.radius;
				return (center - closestPoint).GetPoweredLength() < maxDistance * maxDistance;
			}

			float maxDistance = radius + param.radius;
			return (center - param.origin).GetPoweredLength() < maxDistance * maxDistance;
		}
	}
}
