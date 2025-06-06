/**************************************************************************/
/*  jolt_hinge_joint_3d.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "jolt_hinge_joint_3d.h"

#include "../misc/jolt_type_conversions.h"
#include "../objects/jolt_body_3d.h"
#include "../spaces/jolt_space_3d.h"

#include "core/config/engine.h"

#include "Jolt/Physics/Constraints/FixedConstraint.h"
#include "Jolt/Physics/Constraints/HingeConstraint.h"

namespace {

constexpr double DEFAULT_BIAS = 0.3;
constexpr double DEFAULT_LIMIT_BIAS = 0.3;
constexpr double DEFAULT_SOFTNESS = 0.9;
constexpr double DEFAULT_RELAXATION = 1.0;

double estimate_physics_step() {
	Engine *engine = Engine::get_singleton();

	const double step = 1.0 / engine->get_physics_ticks_per_second();
	const double step_scaled = step * engine->get_time_scale();

	return step_scaled;
}

} // namespace

JPH::Constraint *JoltHingeJoint3D::_build_hinge(JPH::Body *p_jolt_body_a, JPH::Body *p_jolt_body_b, const Transform3D &p_shifted_ref_a, const Transform3D &p_shifted_ref_b, float p_limit) const {
	JPH::HingeConstraintSettings constraint_settings;

	constraint_settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
	constraint_settings.mPoint1 = to_jolt_r(p_shifted_ref_a.origin);
	constraint_settings.mHingeAxis1 = to_jolt(p_shifted_ref_a.basis.get_column(Vector3::AXIS_Z));
	constraint_settings.mNormalAxis1 = to_jolt(p_shifted_ref_a.basis.get_column(Vector3::AXIS_X));
	constraint_settings.mPoint2 = to_jolt_r(p_shifted_ref_b.origin);
	constraint_settings.mHingeAxis2 = to_jolt(p_shifted_ref_b.basis.get_column(Vector3::AXIS_Z));
	constraint_settings.mNormalAxis2 = to_jolt(p_shifted_ref_b.basis.get_column(Vector3::AXIS_X));
	constraint_settings.mLimitsMin = -p_limit;
	constraint_settings.mLimitsMax = p_limit;

	if (limit_spring_enabled) {
		constraint_settings.mLimitsSpringSettings.mFrequency = (float)limit_spring_frequency;
		constraint_settings.mLimitsSpringSettings.mDamping = (float)limit_spring_damping;
	}

	if (p_jolt_body_a == nullptr) {
		return constraint_settings.Create(JPH::Body::sFixedToWorld, *p_jolt_body_b);
	} else if (p_jolt_body_b == nullptr) {
		return constraint_settings.Create(*p_jolt_body_a, JPH::Body::sFixedToWorld);
	} else {
		return constraint_settings.Create(*p_jolt_body_a, *p_jolt_body_b);
	}
}

JPH::Constraint *JoltHingeJoint3D::_build_fixed(JPH::Body *p_jolt_body_a, JPH::Body *p_jolt_body_b, const Transform3D &p_shifted_ref_a, const Transform3D &p_shifted_ref_b) const {
	JPH::FixedConstraintSettings constraint_settings;

	constraint_settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
	constraint_settings.mAutoDetectPoint = false;
	constraint_settings.mPoint1 = to_jolt_r(p_shifted_ref_a.origin);
	constraint_settings.mAxisX1 = to_jolt(p_shifted_ref_a.basis.get_column(Vector3::AXIS_X));
	constraint_settings.mAxisY1 = to_jolt(p_shifted_ref_a.basis.get_column(Vector3::AXIS_Y));
	constraint_settings.mPoint2 = to_jolt_r(p_shifted_ref_b.origin);
	constraint_settings.mAxisX2 = to_jolt(p_shifted_ref_b.basis.get_column(Vector3::AXIS_X));
	constraint_settings.mAxisY2 = to_jolt(p_shifted_ref_b.basis.get_column(Vector3::AXIS_Y));

	if (p_jolt_body_a == nullptr) {
		return constraint_settings.Create(JPH::Body::sFixedToWorld, *p_jolt_body_b);
	} else if (p_jolt_body_b == nullptr) {
		return constraint_settings.Create(*p_jolt_body_a, JPH::Body::sFixedToWorld);
	} else {
		return constraint_settings.Create(*p_jolt_body_a, *p_jolt_body_b);
	}
}

void JoltHingeJoint3D::_update_motor_state() {
	if (unlikely(_is_fixed())) {
		return;
	}

	if (JPH::HingeConstraint *constraint = static_cast<JPH::HingeConstraint *>(jolt_ref.GetPtr())) {
		constraint->SetMotorState(motor_enabled ? JPH::EMotorState::Velocity : JPH::EMotorState::Off);
	}
}

void JoltHingeJoint3D::_update_motor_velocity() {
	if (unlikely(_is_fixed())) {
		return;
	}

	if (JPH::HingeConstraint *constraint = static_cast<JPH::HingeConstraint *>(jolt_ref.GetPtr())) {
		// We flip the direction since Jolt is CCW but Godot is CW.
		constraint->SetTargetAngularVelocity((float)-motor_target_speed);
	}
}

void JoltHingeJoint3D::_update_motor_limit() {
	if (unlikely(_is_fixed())) {
		return;
	}

	if (JPH::HingeConstraint *constraint = static_cast<JPH::HingeConstraint *>(jolt_ref.GetPtr())) {
		JPH::MotorSettings &motor_settings = constraint->GetMotorSettings();
		motor_settings.mMinTorqueLimit = (float)-motor_max_torque;
		motor_settings.mMaxTorqueLimit = (float)motor_max_torque;
	}
}

void JoltHingeJoint3D::_limits_changed() {
	rebuild();
	_wake_up_bodies();
}

void JoltHingeJoint3D::_limit_spring_changed() {
	rebuild();
	_wake_up_bodies();
}

void JoltHingeJoint3D::_motor_state_changed() {
	_update_motor_state();
	_wake_up_bodies();
}

void JoltHingeJoint3D::_motor_speed_changed() {
	_update_motor_velocity();
	_wake_up_bodies();
}

void JoltHingeJoint3D::_motor_limit_changed() {
	_update_motor_limit();
	_wake_up_bodies();
}

JoltHingeJoint3D::JoltHingeJoint3D(const JoltJoint3D &p_old_joint, JoltBody3D *p_body_a, JoltBody3D *p_body_b, const Transform3D &p_local_ref_a, const Transform3D &p_local_ref_b) :
		JoltJoint3D(p_old_joint, p_body_a, p_body_b, p_local_ref_a, p_local_ref_b) {
	rebuild();
}

double JoltHingeJoint3D::get_param(Parameter p_param) const {
	switch (p_param) {
		case PhysicsServer3D::HINGE_JOINT_BIAS: {
			return DEFAULT_BIAS;
		}
		case PhysicsServer3D::HINGE_JOINT_LIMIT_UPPER: {
			return limit_upper;
		}
		case PhysicsServer3D::HINGE_JOINT_LIMIT_LOWER: {
			return limit_lower;
		}
		case PhysicsServer3D::HINGE_JOINT_LIMIT_BIAS: {
			return DEFAULT_LIMIT_BIAS;
		}
		case PhysicsServer3D::HINGE_JOINT_LIMIT_SOFTNESS: {
			return DEFAULT_SOFTNESS;
		}
		case PhysicsServer3D::HINGE_JOINT_LIMIT_RELAXATION: {
			return DEFAULT_RELAXATION;
		}
		case PhysicsServer3D::HINGE_JOINT_MOTOR_TARGET_VELOCITY: {
			return motor_target_speed;
		}
		case PhysicsServer3D::HINGE_JOINT_MOTOR_MAX_IMPULSE: {
			// With Godot using max impulse instead of max torque we don't have much choice but to calculate this and hope the timestep doesn't change.
			return motor_max_torque * estimate_physics_step();
		}
		default: {
			ERR_FAIL_V_MSG(0.0, vformat("Unhandled parameter: '%d'. This should not happen. Please report this.", p_param));
		}
	}
}

void JoltHingeJoint3D::set_param(Parameter p_param, double p_value) {
	switch (p_param) {
		case PhysicsServer3D::HINGE_JOINT_BIAS: {
			if (!Math::is_equal_approx(p_value, DEFAULT_BIAS)) {
				WARN_PRINT(vformat("Hinge joint bias is not supported when using Jolt Physics. Any such value will be ignored. This joint connects %s.", _bodies_to_string()));
			}
		} break;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_UPPER: {
			limit_upper = p_value;
			_limits_changed();
		} break;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_LOWER: {
			limit_lower = p_value;
			_limits_changed();
		} break;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_BIAS: {
			if (!Math::is_equal_approx(p_value, DEFAULT_LIMIT_BIAS)) {
				WARN_PRINT(vformat("Hinge joint bias limit is not supported when using Jolt Physics. Any such value will be ignored. This joint connects %s.", _bodies_to_string()));
			}
		} break;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_SOFTNESS: {
			if (!Math::is_equal_approx(p_value, DEFAULT_SOFTNESS)) {
				WARN_PRINT(vformat("Hinge joint softness is not supported when using Jolt Physics. Any such value will be ignored. This joint connects %s.", _bodies_to_string()));
			}
		} break;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_RELAXATION: {
			if (!Math::is_equal_approx(p_value, DEFAULT_RELAXATION)) {
				WARN_PRINT(vformat("Hinge joint relaxation is not supported when using Jolt Physics. Any such value will be ignored. This joint connects %s.", _bodies_to_string()));
			}
		} break;
		case PhysicsServer3D::HINGE_JOINT_MOTOR_TARGET_VELOCITY: {
			motor_target_speed = p_value;
			_motor_speed_changed();
		} break;
		case PhysicsServer3D::HINGE_JOINT_MOTOR_MAX_IMPULSE: {
			// With Godot using max impulse instead of max torque we don't have much choice but to calculate this and hope the timestep doesn't change.
			motor_max_torque = p_value / estimate_physics_step();
			_motor_limit_changed();
		} break;
		default: {
			ERR_FAIL_MSG(vformat("Unhandled parameter: '%d'. This should not happen. Please report this.", p_param));
		} break;
	}
}

double JoltHingeJoint3D::get_jolt_param(JoltParameter p_param) const {
	switch (p_param) {
		case JoltPhysicsServer3D::HINGE_JOINT_LIMIT_SPRING_FREQUENCY: {
			return limit_spring_frequency;
		}
		case JoltPhysicsServer3D::HINGE_JOINT_LIMIT_SPRING_DAMPING: {
			return limit_spring_damping;
		}
		case JoltPhysicsServer3D::HINGE_JOINT_MOTOR_MAX_TORQUE: {
			return motor_max_torque;
		}
		default: {
			ERR_FAIL_V_MSG(0.0, vformat("Unhandled parameter: '%d'. This should not happen. Please report this.", p_param));
		}
	}
}

void JoltHingeJoint3D::set_jolt_param(JoltParameter p_param, double p_value) {
	switch (p_param) {
		case JoltPhysicsServer3D::HINGE_JOINT_LIMIT_SPRING_FREQUENCY: {
			limit_spring_frequency = p_value;
			_limit_spring_changed();
		} break;
		case JoltPhysicsServer3D::HINGE_JOINT_LIMIT_SPRING_DAMPING: {
			limit_spring_damping = p_value;
			_limit_spring_changed();
		} break;
		case JoltPhysicsServer3D::HINGE_JOINT_MOTOR_MAX_TORQUE: {
			motor_max_torque = p_value;
			_motor_limit_changed();
		} break;
		default: {
			ERR_FAIL_MSG(vformat("Unhandled parameter: '%d'. This should not happen. Please report this.", p_param));
		} break;
	}
}

bool JoltHingeJoint3D::get_flag(Flag p_flag) const {
	switch (p_flag) {
		case PhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT: {
			return limits_enabled;
		}
		case PhysicsServer3D::HINGE_JOINT_FLAG_ENABLE_MOTOR: {
			return motor_enabled;
		}
		default: {
			ERR_FAIL_V_MSG(false, vformat("Unhandled flag: '%d'. This should not happen. Please report this.", p_flag));
		}
	}
}

void JoltHingeJoint3D::set_flag(Flag p_flag, bool p_enabled) {
	switch (p_flag) {
		case PhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT: {
			limits_enabled = p_enabled;
			_limits_changed();
		} break;
		case PhysicsServer3D::HINGE_JOINT_FLAG_ENABLE_MOTOR: {
			motor_enabled = p_enabled;
			_motor_state_changed();
		} break;
		default: {
			ERR_FAIL_MSG(vformat("Unhandled flag: '%d'. This should not happen. Please report this.", p_flag));
		} break;
	}
}

bool JoltHingeJoint3D::get_jolt_flag(JoltFlag p_flag) const {
	switch (p_flag) {
		case JoltPhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT_SPRING: {
			return limit_spring_enabled;
		}
		default: {
			ERR_FAIL_V_MSG(false, vformat("Unhandled flag: '%d'. This should not happen. Please report this.", p_flag));
		}
	}
}

void JoltHingeJoint3D::set_jolt_flag(JoltFlag p_flag, bool p_enabled) {
	switch (p_flag) {
		case JoltPhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT_SPRING: {
			limit_spring_enabled = p_enabled;
			_limit_spring_changed();
		} break;
		default: {
			ERR_FAIL_MSG(vformat("Unhandled flag: '%d'. This should not happen. Please report this.", p_flag));
		} break;
	}
}

float JoltHingeJoint3D::get_applied_force() const {
	ERR_FAIL_NULL_V(jolt_ref, 0.0f);

	JoltSpace3D *space = get_space();
	ERR_FAIL_NULL_V(space, 0.0f);

	const float last_step = space->get_last_step();
	if (unlikely(last_step == 0.0f)) {
		return 0.0f;
	}

	if (_is_fixed()) {
		JPH::FixedConstraint *constraint = static_cast<JPH::FixedConstraint *>(jolt_ref.GetPtr());
		return constraint->GetTotalLambdaPosition().Length() / last_step;
	} else {
		JPH::HingeConstraint *constraint = static_cast<JPH::HingeConstraint *>(jolt_ref.GetPtr());
		const JPH::Vec3 total_lambda = JPH::Vec3(constraint->GetTotalLambdaRotation()[0], constraint->GetTotalLambdaRotation()[1], constraint->GetTotalLambdaRotationLimits() + constraint->GetTotalLambdaMotor());
		return total_lambda.Length() / last_step;
	}
}

float JoltHingeJoint3D::get_applied_torque() const {
	ERR_FAIL_NULL_V(jolt_ref, 0.0f);

	JoltSpace3D *space = get_space();
	ERR_FAIL_NULL_V(space, 0.0f);

	const float last_step = space->get_last_step();
	if (unlikely(last_step == 0.0f)) {
		return 0.0f;
	}

	if (_is_fixed()) {
		JPH::FixedConstraint *constraint = static_cast<JPH::FixedConstraint *>(jolt_ref.GetPtr());
		return constraint->GetTotalLambdaRotation().Length() / last_step;
	} else {
		JPH::HingeConstraint *constraint = static_cast<JPH::HingeConstraint *>(jolt_ref.GetPtr());
		return constraint->GetTotalLambdaRotation().Length() / last_step;
	}
}

void JoltHingeJoint3D::rebuild() {
	destroy();

	JoltSpace3D *space = get_space();
	if (space == nullptr) {
		return;
	}

	JPH::Body *jolt_body_a = body_a != nullptr ? body_a->get_jolt_body() : nullptr;
	JPH::Body *jolt_body_b = body_b != nullptr ? body_b->get_jolt_body() : nullptr;
	ERR_FAIL_COND(jolt_body_a == nullptr && jolt_body_b == nullptr);

	float ref_shift = 0.0f;
	float limit = JPH::JPH_PI;

	if (limits_enabled && limit_lower <= limit_upper) {
		const double limit_midpoint = (limit_lower + limit_upper) / 2.0f;

		ref_shift = float(-limit_midpoint);
		limit = float(limit_upper - limit_midpoint);
	}

	Transform3D shifted_ref_a;
	Transform3D shifted_ref_b;

	_shift_reference_frames(Vector3(), Vector3(0.0f, 0.0f, ref_shift), shifted_ref_a, shifted_ref_b);

	if (_is_fixed()) {
		jolt_ref = _build_fixed(jolt_body_a, jolt_body_b, shifted_ref_a, shifted_ref_b);
	} else {
		jolt_ref = _build_hinge(jolt_body_a, jolt_body_b, shifted_ref_a, shifted_ref_b, limit);
	}

	space->add_joint(this);

	_update_enabled();
	_update_iterations();
	_update_motor_state();
	_update_motor_velocity();
	_update_motor_limit();
}
