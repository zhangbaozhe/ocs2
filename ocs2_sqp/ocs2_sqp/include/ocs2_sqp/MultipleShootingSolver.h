//
// Created by rgrandia on 09.11.20.
//

#pragma once

#include <iostream>

#include <ocs2_core/constraint/ConstraintBase.h>
#include <ocs2_core/constraint/PenaltyBase.h>
#include <ocs2_core/cost/CostFunctionBase.h>
#include <ocs2_core/dynamics/SystemDynamicsBase.h>
#include <ocs2_core/initialization/SystemOperatingTrajectoriesBase.h>
#include <ocs2_core/integration/SensitivityIntegrator.h>
#include <ocs2_core/misc/Benchmark.h>
#include <ocs2_core/misc/ThreadPool.h>
#include <ocs2_oc/oc_solver/SolverBase.h>

#include <hpipm_catkin/HpipmInterface.h>

namespace ocs2 {

struct MultipleShootingSolverSettings {
  scalar_t dt = 0.01;  // user-defined time discretization
  size_t n_state = 0;
  size_t n_input = 0;
  size_t sqpIteration = 1;
  scalar_t deltaTol = 1e-6;

  // Discretization method
  SensitivityIntegratorType integratorType = SensitivityIntegratorType::RK2;

  // Inequality penalty method
  scalar_t inequalityConstraintMu = 0.0;
  scalar_t inequalityConstraintDelta = 1e-6;

  bool qr_decomp = true;  // Only meaningful if the system is constrained. True to use QR decomposiion, False to use lg <= Cx+Du+e <= ug
  bool printSolverStatus = false;      // Print HPIPM status after solving the QP subproblem
  bool printSolverStatistics = false;  // Print benchmarking of the multiple shooting method
  bool printLinesearch = false;        // Print linesearch information

  // Threading
  size_t nThreads = 4;
  int threadPriority = 50;
};

class MultipleShootingSolver : public SolverBase {
 public:
  MultipleShootingSolver(MultipleShootingSolverSettings settings, const SystemDynamicsBase* systemDynamicsPtr,
                         const CostFunctionBase* costFunctionPtr, const ConstraintBase* constraintPtr = nullptr,
                         const CostFunctionBase* terminalCostFunctionPtr = nullptr,
                         const SystemOperatingTrajectoriesBase* operatingTrajectoriesPtr = nullptr);

  ~MultipleShootingSolver() override;

  void reset() override;

  scalar_t getFinalTime() const override { return primalSolution_.timeTrajectory_.back(); };  // horizon is [t0, T] return T;
  void getPrimalSolution(scalar_t finalTime, PrimalSolution* primalSolutionPtr) const override { *primalSolutionPtr = primalSolution_; }

  size_t getNumIterations() const override { return totalNumIterations_; }
  const PerformanceIndex& getPerformanceIndeces() const override { return getIterationsLog().back(); };
  const std::vector<PerformanceIndex>& getIterationsLog() const override;

  /** Decides on time discretization along the horizon */
  static scalar_array_t timeDiscretizationWithEvents(scalar_t initTime, scalar_t finalTime, scalar_t dt, const scalar_array_t& eventTimes,
                                                     scalar_t eventDelta);

  /** Irrelevant baseclass stuff */
  void rewindOptimizer(size_t firstIndex) override{};
  const unsigned long long int& getRewindCounter() const override {
    throw std::runtime_error("[MultipleShootingSolver] no rewind counter");
  };
  const scalar_array_t& getPartitioningTimes() const override { return partitionTime_; };
  ScalarFunctionQuadraticApproximation getValueFunction(scalar_t time, const vector_t& state) const override {
    return ScalarFunctionQuadraticApproximation::Zero(0, 0);
  };
  vector_t getStateInputEqualityConstraintLagrangian(scalar_t time, const vector_t& state) const override { return vector_t::Zero(0); }

 private:
  /** Run a task in parallel with settings.nThreads */
  void runParallel(std::function<void(int)> taskFunction);

  /** Get profiling information as a string */
  std::string getBenchmarkingInformation() const;

  /** Entrypoint for this solver, called by the base class */
  void runImpl(scalar_t initTime, const vector_t& initState, scalar_t finalTime, const scalar_array_t& partitioningTimes) override;
  void runImpl(scalar_t initTime, const vector_t& initState, scalar_t finalTime, const scalar_array_t& partitioningTimes,
               const std::vector<ControllerBase*>& controllersPtrStock) override {
    runImpl(initTime, initState, finalTime, partitioningTimes);
  }

  /** Returns initial guess for the state trajectory */
  vector_array_t initializeStateTrajectory(const vector_t& initState, const scalar_array_t& timeDiscretization, int N) const;

  /** Returns initial guess for the input trajectory */
  vector_array_t initializeInputTrajectory(const scalar_array_t& timeDiscretization, const vector_array_t& stateTrajectory, int N) const;

  /** Creates QP around t, x, u. Returns performance metrics at the current {t, x, u} */
  PerformanceIndex setupQuadraticSubproblem(const scalar_array_t& time, const vector_t& initState, const vector_array_t& x,
                                            const vector_array_t& u);

  static void setupIntermediateNode(SystemDynamicsBase& systemDynamics, DynamicsSensitivityDiscretizer& sensitivityDiscretizer,
                                    CostFunctionBase& costFunction, ConstraintBase* constraintPtr, PenaltyBase* penaltyPtr,
                                    bool projectStateInputEqualityConstraints, scalar_t t, scalar_t dt, const vector_t& x,
                                    const vector_t& x_next, const vector_t& u, PerformanceIndex& performance,
                                    VectorFunctionLinearApproximation& dynamics, ScalarFunctionQuadraticApproximation& cost,
                                    VectorFunctionLinearApproximation& constraints);

  static void setTerminalNode(CostFunctionBase* terminalCostFunctionPtr, scalar_t t, const vector_t& x, PerformanceIndex& performance,
                              ScalarFunctionQuadraticApproximation& cost, VectorFunctionLinearApproximation& constraints);

  /** Computes only the performance metrics at the current {t, x, u} */
  PerformanceIndex computePerformance(const scalar_array_t& time, const vector_t& initState, const vector_array_t& x,
                                      const vector_array_t& u);

  /** Computes performance at an intermediate node */
  static void computePerformance(SystemDynamicsBase& systemDynamics, DynamicsDiscretizer& discretizer, CostFunctionBase& costFunction,
                                 ConstraintBase* constraintPtr, PenaltyBase* penaltyPtr, scalar_t t, scalar_t dt, const vector_t& x,
                                 const vector_t& x_next, const vector_t& u, PerformanceIndex& performance);

  /** Returns solution of the QP subproblem in delta coordinates: {delta_x, delta_u} */
  std::pair<vector_array_t, vector_array_t> getOCPSolution(const vector_t& delta_x0);

  /** Decides on the step to take and overrides given trajectories {x, u} <- {x + a*dx, u + a*du} */
  bool takeStep(const PerformanceIndex& baseline, const scalar_array_t& timeDiscretization, const vector_t& initState,
                const vector_array_t& dx, const vector_array_t& du, vector_array_t& x, vector_array_t& u);

  // Problem definition
  MultipleShootingSolverSettings settings_;
  DynamicsDiscretizer discretizer_;
  DynamicsSensitivityDiscretizer sensitivityDiscretizer_;
  std::vector<std::unique_ptr<SystemDynamicsBase>> systemDynamicsPtr_;
  std::vector<std::unique_ptr<CostFunctionBase>> costFunctionPtr_;
  std::vector<std::unique_ptr<ConstraintBase>> constraintPtr_;
  std::unique_ptr<CostFunctionBase> terminalCostFunctionPtr_;
  std::unique_ptr<SystemOperatingTrajectoriesBase> operatingTrajectoriesPtr_;
  std::unique_ptr<PenaltyBase> penaltyPtr_;

  // Threading
  std::unique_ptr<ThreadPool> threadPoolPtr_;

  // Solution
  PrimalSolution primalSolution_;

  // Solver interface
  HpipmInterface hpipmInterface_;

  // LQ approximation
  std::vector<VectorFunctionLinearApproximation> dynamics_;
  std::vector<ScalarFunctionQuadraticApproximation> cost_;
  std::vector<VectorFunctionLinearApproximation> constraints_;

  // Iteration performance log
  std::vector<PerformanceIndex> performanceIndeces_;

  // Benchmarking
  size_t totalNumIterations_;
  benchmark::RepeatedTimer initializationTimer_;
  benchmark::RepeatedTimer linearQuadraticApproximationTimer_;
  benchmark::RepeatedTimer solveQpTimer_;
  benchmark::RepeatedTimer computeControllerTimer_;

  scalar_array_t partitionTime_ = {};
};

}  // namespace ocs2