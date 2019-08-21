//
// Created by rgrandia on 19.08.19.
//

#include <ocs2_ballbot_example/dynamics/BallbotSystemDynamics.h>

// robcogen
#include <ocs2_robotic_tools/rbd_libraries/robcogen/iit/rbd/rbd.h>
#include <ocs2_robotic_tools/rbd_libraries/robcogen/iit/rbd/traits/TraitSelector.h>

#include "ocs2_ballbot_example/generated/forward_dynamics.h"
#include "ocs2_ballbot_example/generated/inertia_properties.h"
#include "ocs2_ballbot_example/generated/inverse_dynamics.h"
#include "ocs2_ballbot_example/generated/jsim.h"
#include "ocs2_ballbot_example/generated/transforms.h"

namespace ocs2 {
namespace ballbot {

void BallbotSystemDynamics::systemFlowMap(ad_scalar_t time, const ad_dynamic_vector_t& state, const ad_dynamic_vector_t& input,
                                          ad_dynamic_vector_t& stateDerivative) const {
  // compute actuationMatrix S_transposed which appears in the equations: M(q)\dot v + h = S^(transpose)\tau
  Eigen::Matrix<ad_scalar_t, 5, 3> S_transposed = Eigen::Matrix<ad_scalar_t, 5, 3>::Zero();

  const ad_scalar_t cyaw = cos(state(2));
  const ad_scalar_t cpitch = cos(state(3));
  const ad_scalar_t croll = cos(state(4));

  const ad_scalar_t syaw = sin(state(2));
  const ad_scalar_t spitch = sin(state(3));
  const ad_scalar_t sroll = sin(state(4));

  S_transposed(0, 0) = -((cyaw * sroll + (cpitch - croll * spitch) * syaw) / (pow(2, 0.5) * wheelRadius_));
  S_transposed(0, 1) = (-cyaw * (pow(3, 0.5) * croll + 2 * sroll) + (cpitch + (2 * croll - pow(3, 0.5) * sroll) * spitch) * syaw) /
                       (2 * pow(2, 0.5) * wheelRadius_);
  S_transposed(0, 2) = (cyaw * (pow(3, 0.5) * croll - 2 * sroll) + (cpitch + (2 * croll + pow(3, 0.5) * sroll) * spitch) * syaw) /
                       (2 * pow(2, 0.5) * wheelRadius_);

  S_transposed(1, 0) = (cyaw * (cpitch - croll * spitch) - sroll * syaw) / (pow(2, 0.5) * wheelRadius_);
  S_transposed(1, 1) = -((cyaw * (cpitch + (2 * croll - pow(3, 0.5) * sroll) * spitch) + (pow(3, 0.5) * croll + 2 * sroll) * syaw) /
                         (2 * pow(2, 0.5) * wheelRadius_));
  S_transposed(1, 2) = (-cyaw * (cpitch + (2 * croll + pow(3, 0.5) * sroll) * spitch) + (pow(3, 0.5) * croll - 2 * sroll) * syaw) /
                       (2 * pow(2, 0.5) * wheelRadius_);

  S_transposed(2, 0) = -(((ballRadius_ - wheelRadius_) * (croll * cpitch + spitch)) / (pow(2, 0.5) * wheelRadius_));
  S_transposed(2, 1) =
      -(((ballRadius_ - wheelRadius_) * (2 * croll * cpitch - pow(3, 0.5) * cpitch * sroll - spitch)) / (2 * pow(2, 0.5) * wheelRadius_));
  S_transposed(2, 2) =
      -(((ballRadius_ - wheelRadius_) * (2 * croll * cpitch + pow(3, 0.5) * cpitch * sroll - spitch)) / (2 * pow(2, 0.5) * wheelRadius_));

  S_transposed(3, 0) = ((ballRadius_ - wheelRadius_) * sroll) / (pow(2, 0.5) * wheelRadius_);
  S_transposed(3, 1) = ((ballRadius_ - wheelRadius_) * (pow(3, 0.5) * croll + 2 * sroll)) / (2 * pow(2, 0.5) * wheelRadius_);
  S_transposed(3, 2) = -(((ballRadius_ - wheelRadius_) * (pow(3, 0.5) * croll - 2 * sroll)) / (2 * pow(2, 0.5) * wheelRadius_));

  S_transposed(4, 0) = (ballRadius_ - wheelRadius_) / (pow(2, 0.5) * wheelRadius_);
  S_transposed(4, 1) = (wheelRadius_ - ballRadius_) / (2 * pow(2, 0.5) * wheelRadius_);
  S_transposed(4, 2) = (wheelRadius_ - ballRadius_) / (2 * pow(2, 0.5) * wheelRadius_);

  // test for the autogenerated code
  iit::Ballbot::tpl::JointState<ad_scalar_t> qdd;
  iit::Ballbot::tpl::JointState<ad_scalar_t> new_input = S_transposed * input;

  using trait_t = typename iit::rbd::tpl::TraitSelector<ad_scalar_t>::Trait;
  iit::Ballbot::dyn::tpl::InertiaProperties<trait_t> inertias;
  iit::Ballbot::tpl::MotionTransforms<trait_t> transforms;
  iit::Ballbot::dyn::tpl::ForwardDynamics<trait_t> forward_dyn(inertias, transforms);
  forward_dyn.fd(qdd, state.template head<5>(), state.template tail<5>(), new_input);

  // dxdt
  stateDerivative.template head<5>() = state.template tail<5>();
  stateDerivative.template tail<5>() = qdd;
}

}  // namespace ballbot
}  // namespace ocs2