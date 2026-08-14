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

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <Client/IModel.h>
#include <Client/IRenderer.h>
#include "GLDynamicLight.h"

namespace spades {
	namespace draw {
		class GLModelRenderer;
		struct GLShadowMapRenderParam;
		class GLModel : public client::IModel {
			friend class GLModelRenderer;

		public:
			GLModel();

			/**
			 * Returns a conservative world-space radius for a local bounding sphere.
			 *
			 * The largest row sum of A^T A is an upper bound for the squared largest
			 * singular value of the affine transform A.  Unlike using one matrix axis,
			 * this remains conservative for non-uniform scaling and shear while staying
			 * exact for the orthogonal transforms normally used by voxel models.
			 */
			static float GetTransformedBoundingRadius(const Matrix4 &matrix, float localRadius) {
				const Vector3 axis0 = matrix.GetAxis(0);
				const Vector3 axis1 = matrix.GetAxis(1);
				const Vector3 axis2 = matrix.GetAxis(2);

				const float g00 = axis0.GetPoweredLength();
				const float g11 = axis1.GetPoweredLength();
				const float g22 = axis2.GetPoweredLength();
				const float g01 = Vector3::Dot(axis0, axis1);
				const float g02 = Vector3::Dot(axis0, axis2);
				const float g12 = Vector3::Dot(axis1, axis2);

				const float row0 = g00 + fabsf(g01) + fabsf(g02);
				const float row1 = g11 + fabsf(g01) + fabsf(g12);
				const float row2 = g22 + fabsf(g02) + fabsf(g12);
				const float scaleSquared = std::max(row0, std::max(row1, row2));
				return localRadius * sqrtf(std::max(scaleSquared, 0.f));
			}

			/** Returns the radius of the model's local-space bounding sphere. */
			virtual float GetBoundingRadius() const = 0;

			/** Renders for shadow map */
			virtual void RenderShadowMapPass(const std::vector<client::ModelRenderParam> &params) = 0;

			/** Renders only in depth buffer (optional) */
			virtual void Prerender(const std::vector<client::ModelRenderParam> &params,
			                       bool ghostPass) = 0;

			/** Renders sunlighted solid geometry */
			virtual void RenderSunlightPass(const std::vector<client::ModelRenderParam> &params,
			                                bool ghostPass) = 0;

			/** Adds dynamic light */
			virtual void RenderDynamicLightPass(const std::vector<client::ModelRenderParam> &params,
			                                    const std::vector<GLDynamicLight> &lights) = 0;

		private:
			// members used when rendering by GLModelRenderer
			int renderId;
		};
	}
};
