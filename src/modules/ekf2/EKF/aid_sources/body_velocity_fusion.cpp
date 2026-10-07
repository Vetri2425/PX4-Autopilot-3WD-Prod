/****************************************************************************
 *
 *   Copyright (c) 2026 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file body_velocity_fusion.cpp
 * Generic body-frame (FRD) velocity fusion, shared by every aid source that
 * observes velocity in the vehicle body frame (external vision, wheel encoders).
 */

#include "ekf.h"
#include "ekf_derivation/generated/compute_body_vel_innov_var_h.h"
#include "ekf_derivation/generated/compute_body_vel_y_innov_var.h"
#include "ekf_derivation/generated/compute_body_vel_z_innov_var.h"

void Ekf::fuseBodyFrameVelocity(estimator_aid_source3d_s &aid_src, const uint64_t &timestamp,
				const Vector3f &measurement, const Vector3f &measurement_var, const float &innovation_gate)
{
	VectorState H[3];
	Vector3f innov_var;
	Vector3f innov = _R_to_earth.transpose() * _state.vel - measurement;
	const auto state_vector = _state.vector();
	sym::ComputeBodyVelInnovVarH(state_vector, P, measurement_var, &innov_var, &H[0], &H[1], &H[2]);

	updateAidSourceStatus(aid_src,
			      timestamp,				// sample timestamp
			      measurement,				// observation
			      measurement_var,				// observation variance
			      innov,					// innovation
			      innov_var,				// innovation variance
			      innovation_gate);				// innovation gate

	if (!aid_src.innovation_rejected) {
		for (uint8_t index = 0; index <= 2; index++) {
			if (index == 1) {
				sym::ComputeBodyVelYInnovVar(state_vector, P, measurement_var(index), &aid_src.innovation_variance[index]);

			} else if (index == 2) {
				sym::ComputeBodyVelZInnovVar(state_vector, P, measurement_var(index), &aid_src.innovation_variance[index]);
			}

			aid_src.innovation[index] = Vector3f(_R_to_earth.transpose().row(index)) * _state.vel - measurement(index);

			VectorState Kfusion = P * H[index] / aid_src.innovation_variance[index];
			measurementUpdate(Kfusion, H[index], aid_src.observation_variance[index], aid_src.innovation[index]);
		}

		aid_src.fused = true;
		aid_src.time_last_fuse = _time_delayed_us;

		_time_last_hor_vel_fuse = _time_delayed_us;
		_time_last_ver_vel_fuse = _time_delayed_us;
	}
}
