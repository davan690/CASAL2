/*
 * AgeingErrorMatrix.cpp
 *
 *  Created on: 4/09/2013
 *      Author: Admin
 */

#include "CovarianceMatrix.h"

#include "Minimisers/Manager.h"
#include "MCMCs/Manager.h"

namespace niwa {
namespace reports {
namespace ublas = boost::numeric::ublas;

/**
 * Default constructor
 */
CovarianceMatrix::CovarianceMatrix(Model* model) : Report(model) {
  run_mode_    = (RunMode::Type)(RunMode::kEstimation | RunMode::kProfiling | RunMode::kMCMC);
  model_state_ = State::kFinalise;
}

/**
 * Execute the report
 */
void CovarianceMatrix::DoExecute() {
  /*
   * This reports the covariance, correlation and Hessian matrix
   */
  LOG_TRACE();
  auto minimiser_ = model_->managers().minimiser()->active_minimiser();
  covariance_matrix_ = minimiser_->covariance_matrix();

  cache_ << "*"<< type_ << "[" << label_ << "]" << "\n";
  cache_ << "covariance_matrix " << REPORT_R_MATRIX << "\n";

  for (unsigned i = 0; i < covariance_matrix_.size1(); ++i) {
    for (unsigned j = 0; j < covariance_matrix_.size2(); ++j)
      cache_ << covariance_matrix_(i, j) << " ";
    cache_ << "\n";
  }

  if (model_->run_mode() == RunMode::kMCMC) {
    auto mcmc_ = model_->managers().mcmc()->active_mcmc();
    if (mcmc_->recalculate_covariance()) {
      cache_ << REPORT_END << "\n\n";
      LOG_FINE() << "During the MCMC run the covariance matrix was recalculated, so the modified matrix will be printed at the end of the chain";
      cache_ << "Modified_covariance_matrix " << REPORT_R_MATRIX  << "\n";
      auto covariance = mcmc_->covariance_matrix();
      for (unsigned i = 0; i < covariance.size1(); ++i) {
        for (unsigned j = 0; j < covariance.size2() - 1; ++j)
          cache_ << covariance(i, j) << " ";
        cache_ << covariance(i, covariance.size2() - 1) << "\n";
      }
    }
  }

  ready_for_writing_ = true;
}

} /* namespace reports */
} /* namespace niwa */
